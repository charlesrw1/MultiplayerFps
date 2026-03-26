#!/usr/bin/env python3
"""
check_deps.py — Module coupling checker for CsRemake.

Walks Source/ headers, resolves recursive #include chains,
and reports whether each module's includes stay within the
allowed dependency set.
"""

import os
import re
import sys
from collections import defaultdict

# ── Configuration ──────────────────────────────────────────────────────────────

SOURCE_ROOT = os.path.join(os.path.dirname(__file__), "..", "Source")

# Keys are module folder names (top-level dirs under Source/).
# Values list the modules they are explicitly allowed to depend on.
# Dependencies are transitive: if A→B and B→C then A may also include C.
ALLOWED_DEPS: dict[str, list[str]] = {
    "Framework":        [ "External"],
    "Input":            ["Framework"],
    "Scripting":        ["Framework"],
    "Render":           ["Framework","Assets"],
    "LevelSerialization": ["Framework", "Game"],
    "LevelEditor":      ["Framework", "Render", "Game", "LevelSerialization", "UI"],
}

# ── Helpers ────────────────────────────────────────────────────────────────────

INCLUDE_RE = re.compile(r'^\s*#\s*include\s+"([^"]+)"', re.MULTILINE)

# Cache: absolute path → set of absolute paths it directly includes
_direct_cache: dict[str, set[str]] = {}
# Cache: absolute path → set of absolute paths reachable (transitive)
_transitive_cache: dict[str, set[str]] = {}


def find_header(rel: str, including_file: str, all_headers: set[str]) -> str | None:
    """Try to resolve a quoted include relative to the including file, then Source root."""
    # Relative to the file that contains the #include
    candidate = os.path.normpath(os.path.join(os.path.dirname(including_file), rel))
    if candidate in all_headers:
        return candidate
    # Relative to SOURCE_ROOT
    candidate2 = os.path.normpath(os.path.join(SOURCE_ROOT, rel))
    if candidate2 in all_headers:
        return candidate2
    return None


def direct_includes(path: str, all_headers: set[str]) -> set[str]:
    if path in _direct_cache:
        return _direct_cache[path]
    result: set[str] = set()
    try:
        with open(path, encoding="utf-8", errors="replace") as fh:
            text = fh.read()
        for rel in INCLUDE_RE.findall(text):
            resolved = find_header(rel, path, all_headers)
            if resolved:
                result.add(resolved)
    except OSError:
        pass
    _direct_cache[path] = result
    return result


def transitive_includes(path: str, all_headers: set[str],
                        _visiting: set[str] | None = None) -> set[str]:
    if path in _transitive_cache:
        return _transitive_cache[path]
    if _visiting is None:
        _visiting = set()
    if path in _visiting:          # cycle guard
        return set()
    _visiting.add(path)
    result: set[str] = set()
    for dep in direct_includes(path, all_headers):
        result.add(dep)
        result |= transitive_includes(dep, all_headers, _visiting)
    _visiting.discard(path)
    _transitive_cache[path] = result
    return result


def module_of(path: str) -> str | None:
    """Return the module name (top-level Source/ sub-folder) for an absolute path."""
    rel = os.path.relpath(path, SOURCE_ROOT)
    parts = rel.split(os.sep)
    if len(parts) >= 2:
        return parts[0]
    return None   # file sits directly in Source/ (e.g. Types.h) — no module


def allowed_modules(module: str) -> set[str]:
    """Compute the full transitive set of allowed modules for *module*."""
    allowed = {module}
    queue = list(ALLOWED_DEPS.get(module, []))
    while queue:
        m = queue.pop()
        if m in allowed:
            continue
        allowed.add(m)
        queue.extend(ALLOWED_DEPS.get(m, []))
    return allowed


# ── Main logic ─────────────────────────────────────────────────────────────────

def collect_headers() -> set[str]:
    headers: set[str] = set()
    for dirpath, _dirs, files in os.walk(SOURCE_ROOT):
        for f in files:
            if f.endswith((".h", ".hpp")):
                headers.add(os.path.normpath(os.path.join(dirpath, f)))
    return headers


def run() -> None:
    all_headers = collect_headers()

    # Group headers by module
    module_files: dict[str, list[str]] = defaultdict(list)
    for h in sorted(all_headers):
        m = module_of(h)
        if m and m in ALLOWED_DEPS:
            module_files[m].append(h)

    # For display: shorten an absolute path to be relative to SOURCE_ROOT
    def short(p: str) -> str:
        return os.path.relpath(p, SOURCE_ROOT).replace("\\", "/")

    overall_ok = True

    for module in sorted(ALLOWED_DEPS):
        files = module_files.get(module, [])
        if not files:
            continue

        ok_module = True
        violations: list[tuple[str, str, str]] = []   # (file, via, bad_module)

        allowed = allowed_modules(module)

        for hdr in files:
            all_deps = transitive_includes(hdr, all_headers)
            for dep in all_deps:
                dep_module = module_of(dep)
                if dep_module and dep_module not in allowed:
                    ok_module = False
                    overall_ok = False
                    violations.append((short(hdr), short(dep), dep_module))

        if ok_module:
            print(f"[GOOD] {module}")
        else:
            # Summarise: count unique offending (file, bad_module) pairs
            count = len(violations)
            print(f"[BAD]  {module}  ({count} violation{'s' if count != 1 else ''})")
            # Group by file for readability
            by_file: dict[str, list[tuple[str, str]]] = defaultdict(list)
            for src, dep, bad_mod in violations:
                by_file[src].append((dep, bad_mod))
            for src_file, deps in sorted(by_file.items()):
                print(f"       {src_file}")
                for dep_path, bad_mod in sorted(set(deps)):
                    print(f"         includes [{bad_mod}] {dep_path}")

    print()
    if overall_ok:
        print("All modules are clean.")
    else:
        print("Coupling violations found.")
        sys.exit(1)


if __name__ == "__main__":
    run()
