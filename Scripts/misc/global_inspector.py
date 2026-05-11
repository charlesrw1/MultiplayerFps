#!/usr/bin/env python3
"""
global_inspector.py - C++ global variable access analyzer

Usage:
    python global_inspector.py <src_dir> [options]

Options:
    --output FILE         JSON output file (default: globals_report.json)
    --graph FILE          Generate interactive HTML graph (e.g. graph.html)
    --exclude DIR         Directory name to skip (repeatable, default: External)
    --include-dir DIR     Add include path passed to libclang (repeatable)
                          Defaults to src_dir itself so project headers resolve
    --no-pretty           Skip console pretty-print
    --include-headers     Also analyze .h/.hpp files for function bodies

Requirements:
    pip install libclang
    LLVM installed (provides libclang.dll on Windows)
"""

import argparse
import json
import os
import sys
from collections import defaultdict, deque
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List, Optional, Set


# ---------------------------------------------------------------------------
# libclang loading
# ---------------------------------------------------------------------------

def load_libclang():
    """Import clang.cindex, auto-detecting libclang.dll on Windows."""
    try:
        import clang.cindex as cl
    except ImportError:
        print("ERROR: libclang Python bindings not found.")
        print("  Install with: pip install libclang")
        sys.exit(1)

    # Try default (works if libclang is on PATH or bundled with pip package)
    try:
        cl.Index.create()
        return cl
    except Exception:
        pass

    # Windows: search common LLVM install locations
    candidate_paths = [
        r"C:\Program Files\LLVM\bin\libclang.dll",
        r"C:\Program Files (x86)\LLVM\bin\libclang.dll",
    ]

    # Check LIBCLANG_PATH environment variable
    env_path = os.environ.get("LIBCLANG_PATH")
    if env_path:
        candidate_paths = [env_path] + candidate_paths

    for path in candidate_paths:
        if os.path.exists(path):
            cl.Config.set_library_file(path)
            try:
                cl.Index.create()
                return cl
            except Exception as exc:
                print(f"  (tried {path}: {exc})")
                continue

    print("ERROR: libclang.dll not found.")
    print("  Install LLVM from: https://releases.llvm.org/download.html")
    print("  Then re-run. Or set LIBCLANG_PATH env var to the DLL path.")
    sys.exit(1)


# ---------------------------------------------------------------------------
# Data structures
# ---------------------------------------------------------------------------

@dataclass
class GlobalVar:
    name: str
    qualified_name: str
    type_str: str
    file: str
    line: int
    is_static: bool


@dataclass
class FunctionInfo:
    qualified_name: str
    file: str
    direct_globals: Set[str] = field(default_factory=set)   # qualified_names
    calls: Set[str] = field(default_factory=set)             # qualified_names


# ---------------------------------------------------------------------------
# File discovery
# ---------------------------------------------------------------------------

_CPP_EXTS = {".cpp", ".cxx", ".cc"}
_HEADER_EXTS = {".h", ".hpp", ".hxx"}


def find_source_files(
    src_dir: str,
    exclude_dirs: List[str],
    include_headers: bool,
) -> List[Path]:
    """Walk src_dir and return all C++ source files, skipping excluded dirs."""
    exts = set(_CPP_EXTS)
    if include_headers:
        exts |= _HEADER_EXTS

    exclude_lower = [e.lower() for e in exclude_dirs]

    def _is_excluded(path: str) -> bool:
        normalized = path.replace("\\", "/").lower()
        return any(f"/{e}/" in normalized or normalized.endswith(f"/{e}")
                   for e in exclude_lower)

    results: List[Path] = []
    for root, dirs, files in os.walk(src_dir):
        if _is_excluded(root):
            dirs.clear()
            continue
        # Prune excluded subdirs in-place so os.walk doesn't descend
        dirs[:] = [
            d for d in dirs
            if d.lower() not in exclude_lower
        ]
        for fname in files:
            if Path(fname).suffix.lower() in exts:
                results.append(Path(root) / fname)
    return results


# ---------------------------------------------------------------------------
# Inspector
# ---------------------------------------------------------------------------

class Inspector:
    def __init__(self, cl, exclude_dirs: List[str], include_dirs: List[str] = None):
        self.cl = cl
        self.exclude_dirs = [e.lower() for e in exclude_dirs]
        self.include_dirs: List[str] = include_dirs or []
        self.index = cl.Index.create()
        # Keyed by clang USR string to deduplicate across TUs
        self.globals: Dict[str, GlobalVar] = {}
        self.functions: Dict[str, FunctionInfo] = {}
        self.parse_errors: List[str] = []

    # ------------------------------------------------------------------
    # Helpers
    # ------------------------------------------------------------------

    def _get_qualified_name(self, cursor) -> str:
        cl = self.cl
        parts = []
        c = cursor
        while c and c.kind != cl.CursorKind.TRANSLATION_UNIT:
            if c.spelling:
                parts.append(c.spelling)
            c = c.semantic_parent
        return "::".join(reversed(parts))

    def _is_file_or_namespace_scope(self, cursor) -> bool:
        cl = self.cl
        parent = cursor.semantic_parent
        while parent:
            k = parent.kind
            if k == cl.CursorKind.TRANSLATION_UNIT:
                return True
            if k == cl.CursorKind.NAMESPACE:
                parent = parent.semantic_parent
                continue
            return False  # Inside a class, function, etc.
        return False

    def _parse(self, filepath: Path, skip_bodies: bool = False):
        """Parse a file with libclang; return TU or None on hard failure."""
        cl = self.cl
        args = ["-x", "c++", "-std=c++17"]
        for d in self.include_dirs:
            args += ["-I", d]
        opts = (cl.TranslationUnit.PARSE_SKIP_FUNCTION_BODIES
                if skip_bodies else 0)
        try:
            tu = self.index.parse(str(filepath), args=args, options=opts)
            errors = [d for d in tu.diagnostics
                      if d.severity >= cl.Diagnostic.Error]
            if len(errors) > 10:
                self.parse_errors.append(
                    f"{filepath}: {len(errors)} parse errors — results may be incomplete"
                )
            return tu
        except Exception as exc:
            self.parse_errors.append(f"{filepath}: {exc}")
            return None

    # ------------------------------------------------------------------
    # Pass 1: global collection
    # ------------------------------------------------------------------

    def collect_file_globals(self, filepath: Path):
        """Parse filepath and register all file/namespace-scope VAR_DECLs."""
        cl = self.cl
        tu = self._parse(filepath, skip_bodies=True)
        if tu is None:
            return

        fp_str = str(filepath)

        def _walk(cursor):
            if cursor.kind == cl.CursorKind.VAR_DECL:
                loc = cursor.location
                if loc.file and os.path.normpath(str(loc.file.name)) == os.path.normpath(fp_str):
                    if self._is_file_or_namespace_scope(cursor):
                        usr = cursor.get_usr()
                        if usr and usr not in self.globals:
                            is_static = (
                                cursor.storage_class == cl.StorageClass.STATIC
                            )
                            self.globals[usr] = GlobalVar(
                                name=cursor.spelling,
                                qualified_name=self._get_qualified_name(cursor),
                                type_str=cursor.type.spelling,
                                file=fp_str,
                                line=loc.line,
                                is_static=is_static,
                            )
            for child in cursor.get_children():
                _walk(child)

        _walk(tu.cursor)

    # ------------------------------------------------------------------
    # Pass 2: function analysis
    # ------------------------------------------------------------------

    def analyze_file_functions(self, filepath: Path):
        """Parse filepath with function bodies; record direct globals and calls."""
        cl = self.cl
        tu = self._parse(filepath, skip_bodies=False)
        if tu is None:
            return

        fp_str = str(filepath)
        # Build a fast lookup: USR -> qualified_name for known globals
        usr_to_qname: Dict[str, str] = {
            usr: g.qualified_name for usr, g in self.globals.items()
        }

        def _analyze_body(func_cursor, qname: str):
            info = FunctionInfo(qualified_name=qname, file=fp_str)

            def _walk_body(cursor):
                if cursor.kind == cl.CursorKind.DECL_REF_EXPR:
                    ref = cursor.referenced
                    if ref:
                        usr = ref.get_usr()
                        if usr in usr_to_qname:
                            info.direct_globals.add(usr_to_qname[usr])
                elif cursor.kind == cl.CursorKind.CALL_EXPR:
                    ref = cursor.referenced
                    if ref and ref.spelling:
                        callee_qname = self._get_qualified_name(ref)
                        if callee_qname:
                            info.calls.add(callee_qname)
                for child in cursor.get_children():
                    _walk_body(child)

            _walk_body(func_cursor)
            return info

        _FUNC_KINDS = (
            cl.CursorKind.FUNCTION_DECL,
            cl.CursorKind.CXX_METHOD,
            cl.CursorKind.CONSTRUCTOR,
            cl.CursorKind.DESTRUCTOR,
        )

        def _walk(cursor):
            if cursor.kind in _FUNC_KINDS:
                loc = cursor.location
                if (cursor.is_definition()
                        and loc.file
                        and os.path.normpath(str(loc.file.name)) == os.path.normpath(fp_str)):
                    qname = self._get_qualified_name(cursor)
                    self.functions[qname] = _analyze_body(cursor, qname)
            for child in cursor.get_children():
                _walk(child)

        _walk(tu.cursor)

    # ------------------------------------------------------------------
    # Pass 3: transitive closure
    # ------------------------------------------------------------------

    def compute_file_reports(self) -> Dict[str, dict]:
        """
        For each file, compute direct + transitive global sets.
        Returns a dict keyed by filepath.
        """
        # Group functions by the file they are defined in
        file_to_funcs: Dict[str, Set[str]] = defaultdict(set)
        for qname, finfo in self.functions.items():
            file_to_funcs[finfo.file].add(qname)

        results: Dict[str, dict] = {}

        for filepath, func_qnames in file_to_funcs.items():
            # Direct globals = union of all functions in this file
            direct: Set[str] = set()
            for qn in func_qnames:
                direct |= self.functions[qn].direct_globals

            # BFS over call graph for transitive globals
            visited: Set[str] = set(func_qnames)
            queue: deque = deque(func_qnames)
            transitive: Set[str] = set(direct)

            while queue:
                qn = queue.popleft()
                fn = self.functions.get(qn)
                if fn is None:
                    continue
                for callee in fn.calls:
                    if callee not in visited:
                        visited.add(callee)
                        queue.append(callee)
                        callee_fn = self.functions.get(callee)
                        if callee_fn:
                            transitive |= callee_fn.direct_globals

            results[filepath] = {
                "direct_globals": sorted(direct),
                "transitive_globals": sorted(transitive),
                "functions": {
                    qn: {
                        "direct_globals": sorted(self.functions[qn].direct_globals),
                        "calls": sorted(self.functions[qn].calls),
                    }
                    for qn in func_qnames
                },
            }

        return results


# ---------------------------------------------------------------------------
# Output
# ---------------------------------------------------------------------------

def build_report(inspector: "Inspector", file_reports: Dict[str, dict]) -> dict:
    """Assemble the full JSON report structure."""
    globals_out = {}
    for usr, g in inspector.globals.items():
        globals_out[g.qualified_name] = {
            "name": g.name,
            "type": g.type_str,
            "file": g.file,
            "line": g.line,
            "is_static": g.is_static,
        }

    return {
        "globals": globals_out,
        "files": file_reports,
        "parse_errors": inspector.parse_errors,
    }


def pretty_print(report: dict) -> None:
    """Print a ranked summary to stdout."""
    globals_meta = report["globals"]

    def _describe(qname: str) -> str:
        g = globals_meta.get(qname)
        if g is None:
            return qname
        tag = "static" if g["is_static"] else "non-static"
        return f"({tag}) {qname} — {g['type']}"

    files = report["files"]
    ranked = sorted(files.items(), key=lambda kv: len(kv[1]["transitive_globals"]), reverse=True)

    print("\n=== Global Variable Access Report ===\n")
    for filepath, data in ranked:
        ndirect = len(data["direct_globals"])
        ntrans = len(data["transitive_globals"])
        print(f"{filepath}  [direct: {ndirect}, transitive: {ntrans}]")

        if data["direct_globals"]:
            print("  Direct:")
            for qname in data["direct_globals"]:
                print(f"    {_describe(qname)}")

        direct_set = set(data["direct_globals"])
        extra = [g for g in data["transitive_globals"] if g not in direct_set]
        if extra:
            print("  Transitive (additional):")
            for qname in extra:
                print(f"    {_describe(qname)}")
        print()

    nerrors = len(report["parse_errors"])
    if nerrors:
        print(f"parse errors: {nerrors} file(s) — see output JSON for details\n")


# ---------------------------------------------------------------------------
# Graph visualization
# ---------------------------------------------------------------------------

def generate_graph_html(report: dict, output_path: str) -> None:
    """Generate an interactive HTML/vis.js graph of file→global relationships."""
    globals_meta = report["globals"]
    files = report["files"]

    nodes = []
    edges = []
    seen_globals: set = set()

    for filepath, data in files.items():
        ntrans = len(data["transitive_globals"])
        if ntrans == 0:
            continue
        fname = os.path.basename(filepath)
        file_id = f"f:{filepath}"
        nodes.append({
            "id": file_id,
            "label": fname,
            "title": f"{filepath}<br>direct: {len(data['direct_globals'])}, transitive: {ntrans}",
            "group": "file",
            "value": ntrans,
        })
        direct_set = set(data["direct_globals"])
        for qname in data["direct_globals"]:
            seen_globals.add(qname)
            edges.append({
                "from": file_id,
                "to": f"g:{qname}",
                "edgeType": "direct",
            })
        for qname in data["transitive_globals"]:
            if qname not in direct_set:
                seen_globals.add(qname)
                edges.append({
                    "from": file_id,
                    "to": f"g:{qname}",
                    "edgeType": "transitive",
                })

    for qname in seen_globals:
        gmeta = globals_meta.get(qname, {})
        is_static = gmeta.get("is_static", False)
        nodes.append({
            "id": f"g:{qname}",
            "label": gmeta.get("name", qname),
            "title": f"{qname}<br>{gmeta.get('type','?')}<br>{'static' if is_static else 'non-static'}<br>{gmeta.get('file','')}:{gmeta.get('line','')}",
            "group": "static_global" if is_static else "global",
            "value": 1,
        })

    graph_data = json.dumps({"nodes": nodes, "edges": edges})

    html = f"""<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>Global Variable Access Graph</title>
<script src="https://unpkg.com/vis-network@9.1.9/dist/vis-network.min.js"></script>
<link href="https://unpkg.com/vis-network@9.1.9/dist/dist/vis-network.min.css" rel="stylesheet"/>
<style>
  * {{ box-sizing: border-box; margin: 0; padding: 0; }}
  body {{ background: #0d1117; color: #c9d1d9; font-family: monospace; font-size: 13px; }}
  #controls {{
    padding: 8px 16px; background: #161b22; border-bottom: 1px solid #30363d;
    display: flex; gap: 20px; align-items: center; flex-wrap: wrap;
  }}
  #network {{ width: 100%; height: calc(100vh - 44px); }}
  label {{ display: flex; align-items: center; gap: 6px; cursor: pointer; }}
  input[type=range] {{ width: 120px; accent-color: #58a6ff; }}
  #minVal {{ min-width: 20px; }}
  .legend {{ display: flex; gap: 12px; align-items: center; margin-left: auto; }}
  .dot {{ width: 10px; height: 10px; border-radius: 50%; display: inline-block; }}
  .diamond {{ width: 10px; height: 10px; transform: rotate(45deg); display: inline-block; }}
  #stats {{ color: #8b949e; font-size: 11px; }}
</style>
</head>
<body>
<div id="controls">
  <strong>Global Access Graph</strong>
  <label>Min transitive:
    <input type="range" id="minGlobals" min="0" max="30" value="2">
    <span id="minVal">2</span>
  </label>
  <label><input type="checkbox" id="showTransitive"> Show transitive edges</label>
  <label><input type="checkbox" id="showGlobalNodes" checked> Show global nodes</label>
  <span id="stats"></span>
  <div class="legend">
    <span><span class="dot" style="background:#58a6ff"></span> File</span>
    <span><span class="diamond" style="background:#f0883e"></span>&nbsp;Non-static global</span>
    <span><span class="diamond" style="background:#bc8cff"></span>&nbsp;Static global</span>
  </div>
</div>
<div id="network"></div>
<script>
const RAW = {graph_data};

const FILE_COLOR   = {{ border:'#58a6ff', background:'#1f3550', highlight:{{border:'#79c0ff',background:'#2d4a6e'}} }};
const GLOB_COLOR   = {{ border:'#f0883e', background:'#3d2210', highlight:{{border:'#ffa657',background:'#5c3318'}} }};
const STATIC_COLOR = {{ border:'#bc8cff', background:'#2b1a3d', highlight:{{border:'#d2a8ff',background:'#3d2858'}} }};

function build(minT, showTrans, showGlobal) {{
  const fileNodes = RAW.nodes.filter(n => n.group === 'file' && n.value >= minT);
  const fileIds   = new Set(fileNodes.map(n => n.id));

  let filteredEdges = RAW.edges.filter(e => fileIds.has(e.from));
  if (!showTrans) filteredEdges = filteredEdges.filter(e => e.edgeType === 'direct');

  const usedGlobalIds = new Set(filteredEdges.map(e => e.to));
  const globalNodes = showGlobal
    ? RAW.nodes.filter(n => n.group !== 'file' && usedGlobalIds.has(n.id))
    : [];

  const nodes = new vis.DataSet([
    ...fileNodes.map(n => ({{
      id: n.id, label: n.label, title: n.title,
      color: FILE_COLOR, shape: 'dot',
      size: Math.max(8, Math.min(30, n.value * 2)),
      font: {{ color:'#c9d1d9', size:11 }},
    }})),
    ...globalNodes.map(n => ({{
      id: n.id, label: n.label, title: n.title,
      color: n.group === 'static_global' ? STATIC_COLOR : GLOB_COLOR,
      shape: 'diamond', size: 8,
      font: {{ color:'#8b949e', size:10 }},
    }})),
  ]);

  const edges = showGlobal ? new vis.DataSet(filteredEdges.map(e => ({{
    from: e.from, to: e.to,
    dashes: e.edgeType === 'transitive',
    color: {{ color: e.edgeType === 'direct' ? '#58a6ff' : '#30363d', opacity: 0.6 }},
    width: e.edgeType === 'direct' ? 1.5 : 0.8,
    arrows: {{ to: {{ enabled: true, scaleFactor: 0.4 }} }},
  }}))) : new vis.DataSet([]);

  document.getElementById('stats').textContent =
    `${{nodes.length}} nodes · ${{edges.length}} edges`;
  return {{ nodes, edges }};
}}

const container = document.getElementById('network');
let network = null;

function redraw() {{
  const minT       = parseInt(document.getElementById('minGlobals').value);
  const showTrans  = document.getElementById('showTransitive').checked;
  const showGlobal = document.getElementById('showGlobalNodes').checked;
  document.getElementById('minVal').textContent = minT;
  const {{ nodes, edges }} = build(minT, showTrans, showGlobal);

  if (!network) {{
    network = new vis.Network(container, {{ nodes, edges }}, {{
      physics: {{ solver:'forceAtlas2Based', stabilization:{{ iterations:120 }}, forceAtlas2Based:{{ gravitationalConstant:-60, springLength:120 }} }},
      interaction: {{ hover:true, tooltipDelay:100 }},
      edges: {{ smooth:{{ type:'continuous', roundness:0.2 }} }},
    }});
  }} else {{
    network.setData({{ nodes, edges }});
  }}
}}

document.getElementById('minGlobals').addEventListener('input', redraw);
document.getElementById('showTransitive').addEventListener('change', redraw);
document.getElementById('showGlobalNodes').addEventListener('change', redraw);
redraw();
</script>
</body>
</html>"""

    with open(output_path, "w", encoding="utf-8") as fh:
        fh.write(html)
    print(f"Graph written to: {output_path}")


# ---------------------------------------------------------------------------
# Main entry point
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description="Analyze C++ source for global variable access (direct + transitive)."
    )
    parser.add_argument("src_dir", help="Root source directory to analyze")
    parser.add_argument("--output", default="globals_report.json",
                        help="JSON output file (default: globals_report.json)")
    parser.add_argument("--graph", default=None, metavar="FILE",
                        help="Generate interactive HTML graph (e.g. --graph graph.html)")
    parser.add_argument("--exclude", action="append", default=None,
                        metavar="DIR",
                        help="Directory name to exclude (repeatable, default: External)")
    parser.add_argument("--include-dir", action="append", default=None,
                        dest="include_dirs", metavar="DIR",
                        help="Add include path for libclang (repeatable). Defaults to src_dir.")
    parser.add_argument("--no-pretty", action="store_true",
                        help="Skip console pretty-print, only write JSON")
    parser.add_argument("--include-headers", action="store_true",
                        help="Also analyze .h/.hpp files for function bodies")
    args = parser.parse_args()

    if args.exclude is None:
        args.exclude = ["External"]

    if not os.path.isdir(args.src_dir):
        print(f"ERROR: src_dir not found: {args.src_dir}")
        sys.exit(1)

    # Default include dir to src_dir so project headers resolve without extra flags
    include_dirs = args.include_dirs if args.include_dirs else [args.src_dir]

    cl = load_libclang()
    print(f"Scanning: {args.src_dir}")
    print(f"Excluding dirs: {args.exclude}")
    print(f"Include dirs:   {include_dirs}")

    files = find_source_files(args.src_dir, args.exclude, args.include_headers)
    print(f"Found {len(files)} source file(s)")

    inspector = Inspector(cl, args.exclude, include_dirs=include_dirs)

    # Pass 1: collect globals from all files
    print("Pass 1: collecting globals...")
    for f in files:
        inspector.collect_file_globals(f)
    print(f"  Found {len(inspector.globals)} global variable(s)")

    # Pass 2: analyze function bodies in .cpp files only
    cpp_files = [f for f in files if f.suffix.lower() in _CPP_EXTS]
    if args.include_headers:
        cpp_files = files
    print(f"Pass 2: analyzing {len(cpp_files)} file(s) for function bodies...")
    for f in cpp_files:
        inspector.analyze_file_functions(f)
    print(f"  Found {len(inspector.functions)} function definition(s)")

    # Pass 3: transitive closure
    print("Pass 3: computing transitive closure...")
    file_reports = inspector.compute_file_reports()

    # Output
    report = build_report(inspector, file_reports)
    with open(args.output, "w", encoding="utf-8") as fh:
        json.dump(report, fh, indent=2)
    print(f"\nReport written to: {args.output}")

    if args.graph:
        generate_graph_html(report, args.graph)

    if not args.no_pretty:
        pretty_print(report)


if __name__ == "__main__":
    main()
