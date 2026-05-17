"""P3.1.5: assign explicit layout(location=N) to every shader stage varying.

SPIR-V requires explicit cross-stage `layout(location=N)` on `in`/`out`
varyings. GL accepts the strict-superset GLSL silently, so the rewrite
is safe to land before the SDL3 backend port begins.

Inputs:
  - Scripts/shader_links.json   (V/F pairings emitted by dump_shader_links.py)
  - Shaders/                    (source tree)

Two distinct location spaces:
  - Varying locations: shared across a V stage's `out`s and the linked F
    stage's `in`s. Same name -> same location.
  - Color-attachment locations: per fragment-shader file, alphabetical,
    starting at 0. These are F-shader `out` decls (render targets).

Algorithm:
  1. Classify each file from shader_links.json: vertex-side, fragment-side,
     or master (single file with _VERTEX_SHADER / _FRAGMENT_SHADER gates).
  2. Build varying-graph components from V/F pairs. Within each component
     gather the union of varying names (V `out` + F `in`), sort, assign
     locations type-aware (mat3 = 3 slots, mat4 = 4, else 1).
  3. For each F file (and the F block of every master), assign color-
     attachment locations to its `out` decls — alphabetical, slot 1 each.
  4. Rewrite files in place. Idempotent — a second run produces no diff.

Vertex attributes (V file `in` decls) already have explicit locations in
this codebase, so we don't touch them. The script asserts loudly if it
ever finds one missing.

Compute shaders have no varyings; skipped.

Usage: py Scripts/assign_shader_locations.py
"""
from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SHADERS = ROOT / "Shaders"
LINKS = Path(__file__).resolve().parent / "shader_links.json"

RX_VARYING = re.compile(
    r"""^(?P<indent>[ \t]*)
        (?P<layout>layout\s*\(\s*location\s*=\s*(?P<existing>\d+)\s*\)\s+)?
        (?P<interp>(?:flat|noperspective|smooth|centroid)[ \t]+)?
        (?P<inout>in|out)[ \t]+
        (?:highp[ \t]+|mediump[ \t]+|lowp[ \t]+)?
        (?P<type>vec[234]|mat[234]|float|int|uint|ivec[234]|uvec[234]|bool|double|dvec[234]|dmat[234])
        [ \t]+(?P<name>[A-Za-z_][A-Za-z0-9_]*)
        [ \t]*;(?P<trailing>.*)$""",
    re.VERBOSE,
)

RX_IFDEF = re.compile(r"^\s*#\s*ifdef\s+([A-Za-z_][A-Za-z0-9_]*)")
RX_IFNDEF = re.compile(r"^\s*#\s*ifndef\s+([A-Za-z_][A-Za-z0-9_]*)")
RX_IF = re.compile(r"^\s*#\s*if\b")
RX_ENDIF = re.compile(r"^\s*#\s*endif\b")
RX_ELSE = re.compile(r"^\s*#\s*el(?:se|if)\b")

def _extract_trailing(raw: str) -> str:
    """Return the text after the first `;` in the raw line, including any
    inline `// comment` — preserved so the rewrite doesn't lose authoring
    notes attached to varying decls."""
    semi = raw.find(";")
    if semi < 0:
        return ""
    return raw[semi + 1:].rstrip("\r\n")


TYPE_SLOTS = {
    "mat2": 2, "mat3": 3, "mat4": 4,
    "dmat2": 2, "dmat3": 3, "dmat4": 4,
}

STAGE_VERTEX = "vertex"
STAGE_FRAGMENT = "fragment"


def slot_count(typename: str) -> int:
    return TYPE_SLOTS.get(typename, 1)


def strip_comments(text: str) -> list[str]:
    """Return lines with block/line comments removed but indices preserved
    (each output line corresponds 1:1 to a source line). Block comments
    that span lines blank out the interior."""
    out: list[str] = []
    in_block = False
    for raw in text.splitlines():
        s = raw
        if in_block:
            end = s.find("*/")
            if end < 0:
                out.append("")
                continue
            s = s[end + 2:]
            in_block = False
        while True:
            start = s.find("/*")
            if start < 0:
                break
            end = s.find("*/", start + 2)
            if end < 0:
                s = s[:start]
                in_block = True
                break
            s = s[:start] + s[end + 2:]
        cm = s.find("//")
        if cm >= 0:
            s = s[:cm]
        out.append(s)
    return out


def walk_stage_blocks(stripped: list[str], master: bool) -> list[tuple[str, int]]:
    """For each source line index, return the stage it belongs to. Non-
    master files are mapped wholesale to STAGE_VERTEX or STAGE_FRAGMENT
    by the caller; this only matters for master files where stage gates
    flip mid-file.

    Returns list of (stage, depth) per line index; stage may be "neutral"
    if outside any stage block (top-level master decls — those are not
    varyings, they're shared declarations like UBOs)."""
    out: list[tuple[str, int]] = []
    # Stack of (gate_kind, gate_name) where gate_kind is 'vertex', 'fragment',
    # or 'neutral'. Anything pushed by an `#if` that isn't a known stage gate
    # gets 'neutral' and inherits the parent stage classification.
    stack: list[str] = []
    for line in stripped:
        m_ifdef = RX_IFDEF.match(line)
        m_ifndef = RX_IFNDEF.match(line)
        m_if = RX_IF.match(line)
        m_endif = RX_ENDIF.match(line)
        m_else = RX_ELSE.match(line)

        # Determine stage BEFORE processing this line's directive so the
        # directive line itself doesn't pollute its own classification.
        cur = stack[-1] if stack else "neutral"
        out.append((cur, len(stack)))

        if master and m_ifdef and m_ifdef.group(1) == "_VERTEX_SHADER":
            stack.append(STAGE_VERTEX)
        elif master and m_ifdef and m_ifdef.group(1) == "_FRAGMENT_SHADER":
            stack.append(STAGE_FRAGMENT)
        elif m_ifdef or m_ifndef or m_if:
            stack.append(cur if cur != "neutral" else "neutral")
        elif m_else:
            # Treat else/elif as continuing the current stage classification.
            pass
        elif m_endif:
            if stack:
                stack.pop()
    return out


def parse_varyings(path: Path, default_stage: str, master: bool) -> list[dict]:
    """Return list of varying decls. For non-master files, every line's
    stage is `default_stage`. For master, walk_stage_blocks decides per
    line (top-level decls outside any stage gate are skipped — they're
    UBOs/SSBOs, not stage I/O)."""
    text = path.read_text(encoding="utf-8")
    raw_lines = text.splitlines()
    stripped = strip_comments(text)
    line_stages = walk_stage_blocks(stripped, master)

    out: list[dict] = []
    for i, scan in enumerate(stripped):
        if not scan.strip():
            continue
        m = RX_VARYING.match(scan)
        if not m:
            continue
        stage = line_stages[i][0] if master else default_stage
        if stage == "neutral":
            # Master top-level decl outside any stage gate — must be a
            # uniform/SSBO/struct. Skip.
            continue
        inout = m.group("inout")
        existing_raw = m.group("existing")
        existing = int(existing_raw) if existing_raw is not None else None
        if stage == STAGE_VERTEX and inout == "in":
            # Vertex attribute. Pre-existing location is required and we
            # never touch them (they're driven by the engine's vertex
            # input layout). Skip cleanly if present; assert otherwise.
            if existing is None:
                raise SystemExit(
                    f"{path}:{i+1}: vertex attribute '{m.group('name')}' has no "
                    f"explicit layout(location). Fix manually."
                )
            continue
        out.append({
            "line_idx": i,
            "indent": m.group("indent"),
            "interp": m.group("interp") or "",
            "inout": inout,
            "type": m.group("type"),
            "name": m.group("name"),
            "stage": stage,
            "existing": existing,
            "trailing": _extract_trailing(raw_lines[i]),
        })
    return out


def collect_roles() -> tuple[dict[str, str], dict[str, set[str]], set[str]]:
    """Classify every shader path mentioned in shader_links, plus orphan
    files in Shaders/ that aren't referenced by any call site (dead /
    yet-to-be-wired-up shaders — annotate them by V/F suffix convention
    so the whole tree is SPIR-V compatible).

    Returns:
      role: path -> 'vertex' | 'fragment' | 'master' | 'compute'
      graph: varying graph (V <-> F edges); orphans get isolated edges
             between same-basename V↔F pairs when both exist
      compute_set: names of compute shaders to skip
    """
    data = json.loads(LINKS.read_text(encoding="utf-8"))
    role: dict[str, str] = {}
    graph: dict[str, set[str]] = {}
    compute_set = {c["compute"] for c in data["compute"]}
    for c in compute_set:
        role[c] = "compute"
    for r in data["raster"]:
        v, f = r["vert"], r["frag"]
        if role.get(v) not in (None, "vertex"):
            raise SystemExit(f"{v}: role conflict ({role.get(v)} vs vertex)")
        if role.get(f) not in (None, "fragment"):
            raise SystemExit(f"{f}: role conflict ({role.get(f)} vs fragment)")
        role[v] = "vertex"
        role[f] = "fragment"
        graph.setdefault(v, set()).add(f)
        graph.setdefault(f, set()).add(v)
    for m in data["master_shaders"]:
        role[m] = "master"
        graph.setdefault(m, set()).add(m)

    # Discover orphans. Heuristic: filename ends in V.txt -> vertex,
    # ends in F.txt -> fragment. Anything else is left alone (probably an
    # include-only header — those have no in/out at top level anyway).
    for path in sorted(SHADERS.rglob("*.txt")):
        rel = path.relative_to(SHADERS).as_posix()
        if rel in role:
            continue
        if rel.endswith("V.txt"):
            role[rel] = "vertex"
            graph.setdefault(rel, set())
        elif rel.endswith("F.txt"):
            role[rel] = "fragment"
            graph.setdefault(rel, set())
        # else: include header / unknown — skipped.

    # Pair orphan V↔F by basename (e.g., LightAccumulationV.txt <->
    # LightAccumulationF.txt). Anything that doesn't pair stays in its
    # own singleton component.
    for rel, r in list(role.items()):
        if r == "vertex" and rel.endswith("V.txt"):
            base = rel[:-len("V.txt")]
            mate = base + "F.txt"
            if role.get(mate) == "fragment":
                graph[rel].add(mate)
                graph[mate].add(rel)

    return role, graph, compute_set


def connected_components(graph: dict[str, set[str]]) -> list[set[str]]:
    seen: set[str] = set()
    comps: list[set[str]] = []
    for start in graph:
        if start in seen:
            continue
        stack = [start]
        comp: set[str] = set()
        while stack:
            n = stack.pop()
            if n in comp:
                continue
            comp.add(n)
            stack.extend(graph.get(n, ()))
        seen |= comp
        comps.append(comp)
    return comps


def file_path(rel: str) -> Path:
    return SHADERS / rel


def assign_component_varyings(
    comp: set[str], role: dict[str, str]
) -> tuple[dict[str, int], dict[str, list[dict]]]:
    """Pool varyings across the component. Returns:
      name -> location
      path -> list of parsed varying decls (cached for rewrite step)
    """
    type_by_name: dict[str, str] = {}
    existing_by_name: dict[str, int] = {}
    decls_by_path: dict[str, list[dict]] = {}
    for rel in sorted(comp):
        p = file_path(rel)
        if not p.exists():
            continue
        r = role[rel]
        master = (r == "master")
        default_stage = {
            "vertex": STAGE_VERTEX,
            "fragment": STAGE_FRAGMENT,
            "master": "neutral",
        }[r]
        decls = parse_varyings(p, default_stage, master)
        decls_by_path[rel] = decls
        for v in decls:
            is_varying = (
                (v["stage"] == STAGE_VERTEX and v["inout"] == "out") or
                (v["stage"] == STAGE_FRAGMENT and v["inout"] == "in")
            )
            if not is_varying:
                continue
            prior = type_by_name.get(v["name"])
            if prior is not None and prior != v["type"]:
                raise SystemExit(
                    f"varying type conflict for '{v['name']}': "
                    f"{prior} vs {v['type']} in component {sorted(comp)}"
                )
            type_by_name[v["name"]] = v["type"]
            if v["existing"] is not None:
                prior_loc = existing_by_name.get(v["name"])
                if prior_loc is not None and prior_loc != v["existing"]:
                    raise SystemExit(
                        f"varying '{v['name']}' has conflicting pre-existing "
                        f"locations {prior_loc} vs {v['existing']} across "
                        f"{sorted(comp)}"
                    )
                existing_by_name[v["name"]] = v["existing"]

    return _pack_locations(type_by_name, existing_by_name, "varying"), decls_by_path


def _pack_locations(
    type_by_name: dict[str, str],
    existing_by_name: dict[str, int],
    kind: str,
) -> dict[str, int]:
    """Assign locations:
      - Names with a pre-existing layout keep that location (reserved).
      - Remaining names are placed alphabetically into the first free
        contiguous slot run that fits their slot_count.
      - If two pre-existing names collide on the same slot, error out —
        that is a pre-existing source bug worth surfacing.
    """
    # Pre-existing layouts may overlap legitimately across mutually-
    # exclusive #ifdef branches (e.g. MasterDeferred FORWARD vs deferred
    # GBUFFER outputs both at location 0). We trust the source author for
    # those — only auto-assigned slots are required to be collision-free.
    reserved_for_packing: set[int] = set()
    for name, base in existing_by_name.items():
        span = slot_count(type_by_name[name])
        for s in range(base, base + span):
            reserved_for_packing.add(s)

    loc: dict[str, int] = dict(existing_by_name)
    pending = sorted(n for n in type_by_name if n not in existing_by_name)
    for name in pending:
        span = slot_count(type_by_name[name])
        cursor = 0
        while not all((cursor + s) not in reserved_for_packing for s in range(span)):
            cursor += 1
        for s in range(span):
            reserved_for_packing.add(cursor + s)
        loc[name] = cursor
    return loc


def rewrite_file(
    path: Path,
    decls: list[dict],
    varying_loc: dict[str, int],
) -> bool:
    """Rewrite a single file. Color-attachment locations (fragment `out`s)
    are assigned per-file, respecting any pre-existing layout(location)
    on those outs."""
    if not decls:
        return False
    text = path.read_text(encoding="utf-8")
    lines = text.splitlines(keepends=True)

    color_type: dict[str, str] = {}
    color_existing: dict[str, int] = {}
    for v in decls:
        if v["stage"] == STAGE_FRAGMENT and v["inout"] == "out":
            color_type[v["name"]] = v["type"]
            if v["existing"] is not None:
                prior = color_existing.get(v["name"])
                if prior is not None and prior != v["existing"]:
                    raise SystemExit(
                        f"{path}: color attachment '{v['name']}' has "
                        f"conflicting pre-existing locations"
                    )
                color_existing[v["name"]] = v["existing"]
    color_loc = _pack_locations(color_type, color_existing, "color attachment")

    changed = False
    for v in decls:
        is_varying = (
            (v["stage"] == STAGE_VERTEX and v["inout"] == "out") or
            (v["stage"] == STAGE_FRAGMENT and v["inout"] == "in")
        )
        if is_varying:
            loc = varying_loc.get(v["name"])
        elif v["stage"] == STAGE_FRAGMENT and v["inout"] == "out":
            loc = color_loc[v["name"]]
        else:
            continue
        if loc is None:
            raise SystemExit(f"no location for {v['name']} in {path}")
        # Skip if pre-existing layout already matches our assignment.
        if v["existing"] == loc:
            continue
        line = lines[v["line_idx"]]
        # Recompose the line: indent + layout(...) + interp + inout type name;
        # plus any trailing inline comment / whitespace, plus the original
        # line ending so we preserve LF vs CRLF.
        eol = "\r\n" if line.endswith("\r\n") else ("\n" if line.endswith("\n") else "")
        body = (
            f"{v['interp']}{v['inout']} {v['type']} {v['name']};"
            f"{v['trailing']}{eol}"
        )
        new_line = f"{v['indent']}layout(location = {loc}) {body}"
        if new_line != line:
            lines[v["line_idx"]] = new_line
            changed = True
    if changed:
        path.write_text("".join(lines), encoding="utf-8")
    return changed


def main() -> int:
    role, graph, compute_set = collect_roles()

    # Sanity: graph nodes must exist on disk.
    for n in graph:
        if not file_path(n).exists():
            print(f"WARN: graph references missing file: {n}", file=sys.stderr)

    components = connected_components(graph)
    rewritten = 0
    for comp in components:
        varying_loc, decls_by_path = assign_component_varyings(comp, role)
        for rel, decls in decls_by_path.items():
            if rewrite_file(file_path(rel), decls, varying_loc):
                rewritten += 1

    print(f"components: {len(components)}")
    print(f"compute shaders skipped: {len(compute_set)}")
    print(f"files rewritten: {rewritten}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
