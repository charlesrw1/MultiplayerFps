#!/usr/bin/env python3
"""
Static scanner: assert zero raw ID3D11*/D3DCompile* uses outside the DX11
backend (and the shared SPIR-V->HLSL->DXBC toolchain).

Gate for the M1 DX11 backend (see docs/rendering/gfx_abstraction.md, P3.1
D6). Run from repo root.

Walks Source/**/*.{h,cpp,c,hpp}, strips line + block comments, then matches
`\\bID3D11[A-Za-z0-9_]*\\b` and `\\bD3DCompile[A-Za-z0-9_]*\\s*\\(` against
each line. Files inside External/, the DX11 backend itself, and the shared
SpirvCompile toolchain (which compiles HLSL->DXBC for both backends, see D0)
are skipped.

Exits 0 on clean, 1 with file:line:match on any leak.
"""

import os
import re
import sys

SOURCE_DIR = "Source"

# Path patterns that are part of the DX11 backend, the shared HLSL/DXBC
# toolchain, or otherwise allowed.
ACCEPT_PATHS = (
    # External libraries (incl. vendored imgui_impl_dx11).
    "Source/External/",
    # The DX11 backend itself.
    "Source/Render/Dx11/",
    # Shared SPIR-V -> HLSL -> DXBC toolchain (D0), used by the DX11 backend
    # but lives outside Source/Render/Dx11/ alongside the rest of the
    # SPIR-V compile pipeline.
    "Source/Render/SpirvCompile.h",
    "Source/Render/SpirvCompile.cpp",
)

EXTS = (".cpp", ".c", ".h", ".hpp")

DX11_TYPE = re.compile(r"\bID3D11[A-Za-z0-9_]*\b")
D3D_COMPILE = re.compile(r"\bD3DCompile[A-Za-z0-9_]*\s*\(")


def strip_comments(text: str) -> str:
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
                m = DX11_TYPE.search(line) or D3D_COMPILE.search(line)
                if m:
                    leaks.append((rel.replace("\\", "/"), lineno, m.group(0)))

    if not leaks:
        print("OK: no raw ID3D11*/D3DCompile* uses outside the DX11 backend.")
        return 0

    print(f"FAIL: {len(leaks)} raw ID3D11*/D3DCompile* use(s) outside backend:")
    for path, lineno, match in leaks:
        print(f"  {path}:{lineno}: {match}")
    return 1


if __name__ == "__main__":
    sys.exit(main())
