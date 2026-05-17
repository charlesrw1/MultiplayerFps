#!/usr/bin/env python3
"""
Static scanner: assert zero raw gl* function calls outside the OpenGL backend.

Gate for the Phase 1 graphics-device abstraction wrap (see
docs/rendering/gfx_abstraction.md sub-phase 1.9). Run from repo root.

Walks Source/**/*.{h,cpp,c}, strips line + block comments, then matches the
regex `\\bgl[A-Z][a-zA-Z0-9_]*\\s*\\(` against each line. Files inside
External/ and the accept-list (transitional/OpenGL-only callers) are skipped.

Exits 0 on clean, 1 with file:line:match on any leak.
"""

import os
import re
import sys

SOURCE_DIR = "Source"

# Path patterns that are part of the OpenGL backend or otherwise allowed.
# Backslashes are normalized to forward slashes before matching.
ACCEPT_PATHS = (
    # External libraries.
    "Source/External/",
    # The OpenGL backend itself.
    "Source/Render/OpenGl",
)

EXTS = (".cpp", ".c", ".h", ".hpp")

GL_CALL = re.compile(r"\bgl[A-Z][a-zA-Z0-9_]*\s*\(")


def strip_comments(text: str) -> str:
    # Block comments first, then line comments.
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    out_lines = []
    for line in text.splitlines():
        out_lines.append(re.sub(r"//.*", "", line))
    return "\n".join(out_lines)


def path_accepted(rel_path: str) -> bool:
    norm = rel_path.replace("\\", "/")
    return any(norm.startswith(p) for p in ACCEPT_PATHS)


def main() -> int:
    leaks = []
    for root, _dirs, files in os.walk(SOURCE_DIR):
        for f in files:
            if not f.endswith(EXTS):
                continue
            full = os.path.join(root, f)
            rel = os.path.relpath(full, ".")
            if path_accepted(rel):
                continue
            try:
                with open(full, encoding="utf-8") as fp:
                    raw = fp.read()
            except (OSError, UnicodeDecodeError):
                continue
            stripped = strip_comments(raw)
            for lineno, line in enumerate(stripped.splitlines(), 1):
                m = GL_CALL.search(line)
                if m:
                    leaks.append((rel.replace("\\", "/"), lineno, m.group(0)))

    if not leaks:
        print("OK: no raw gl* calls outside backend.")
        return 0

    print(f"FAIL: {len(leaks)} raw gl* call(s) outside backend:")
    for path, lineno, match in leaks:
        print(f"  {path}:{lineno}: {match}")
    return 1


if __name__ == "__main__":
    sys.exit(main())
