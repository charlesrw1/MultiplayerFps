"""Dump every shader-program creation site in Source/ to JSON.

Output: Scripts/shader_links.json
  - raster:  list of {vert, frag, defines, site}
  - compute: list of {compute, defines, site}
  - single:  list of {shared, defines, site}  (MASTER-style shaders are
             registered indirectly via MaterialLocal_Loading; surfaced
             separately because there is no literal call site to grep)

Used by the layout(location=N) sweep tooling so V/F pairings come from the
engine, not guessed by filename convention.

Usage: py Scripts/dump_shader_links.py
"""
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / "Source"
OUT = Path(__file__).resolve().parent / "shader_links.json"

# matches: <ident>create_raster(  "v",  "f"  [, "defines"] )
# anchored on the function name; tolerates whitespace/newlines inside parens.
RX_RASTER = re.compile(
    r'create_raster\s*\(\s*"([^"]+)"\s*,\s*"([^"]+)"(?:\s*,\s*"([^"]*)")?\s*\)',
    re.MULTILINE,
)
RX_COMPUTE = re.compile(
    r'create_compute\s*\(\s*"([^"]+)"(?:\s*,\s*"([^"]*)")?\s*\)',
    re.MULTILINE,
)
RX_SINGLE = re.compile(
    r'create_shader_single_file\s*\(\s*"([^"]+)"(?:\s*,\s*"([^"]*)")?\s*\)',
    re.MULTILINE,
)
# create_shader_vert_frag (raw IGraphicsDevice path used by tests)
RX_VERTFRAG_RAW = re.compile(
    r'create_shader_vert_frag\s*\(\s*"([^"]+)"\s*,\s*"([^"]+)"(?:\s*,\s*"([^"]*)")?\s*\)',
    re.MULTILINE,
)
RX_COMPUTE_RAW = re.compile(
    r'create_shader_compute\s*\(\s*"([^"]+)"(?:\s*,\s*"([^"]*)")?\s*\)',
    re.MULTILINE,
)

SKIP_DIRS = {"External"}

# MASTER shaders enumerated from MaterialLocal_Loading.cpp — no literal call
# site grep can recover these because they go through program_def + a runtime
# string switch on material domain.
MASTER_SHADERS = [
    "MASTER/MasterDeferredShader.txt",
    "MASTER/MasterTerrainShader.txt",
    "MASTER/MasterDecalShader.txt",
    "MASTER/MasterUIShader.txt",
    "MASTER/MasterPostProcessShader.txt",
    "MASTER/MasterParticleShader.txt",
]


def line_of(text: str, idx: int) -> int:
    return text.count("\n", 0, idx) + 1


def split_defines(s: str | None) -> list[str]:
    if not s:
        return []
    return [d.strip() for d in s.split(",") if d.strip()]


def relpath(p: Path) -> str:
    return p.relative_to(ROOT).as_posix()


def scan() -> dict:
    raster: list[dict] = []
    compute: list[dict] = []
    single: list[dict] = []

    for path in SOURCE.rglob("*"):
        if not path.is_file() or path.suffix not in (".cpp", ".h"):
            continue
        if any(part in SKIP_DIRS for part in path.parts):
            continue
        text = path.read_text(encoding="utf-8", errors="replace")

        for m in RX_RASTER.finditer(text):
            raster.append({
                "vert": m.group(1),
                "frag": m.group(2),
                "defines": split_defines(m.group(3)),
                "site": f"{relpath(path)}:{line_of(text, m.start())}",
            })
        for m in RX_VERTFRAG_RAW.finditer(text):
            raster.append({
                "vert": m.group(1),
                "frag": m.group(2),
                "defines": split_defines(m.group(3)),
                "site": f"{relpath(path)}:{line_of(text, m.start())}",
            })
        for m in RX_COMPUTE.finditer(text):
            compute.append({
                "compute": m.group(1),
                "defines": split_defines(m.group(2)),
                "site": f"{relpath(path)}:{line_of(text, m.start())}",
            })
        for m in RX_COMPUTE_RAW.finditer(text):
            compute.append({
                "compute": m.group(1),
                "defines": split_defines(m.group(2)),
                "site": f"{relpath(path)}:{line_of(text, m.start())}",
            })
        for m in RX_SINGLE.finditer(text):
            single.append({
                "shared": m.group(1),
                "defines": split_defines(m.group(2)),
                "site": f"{relpath(path)}:{line_of(text, m.start())}",
            })

    # Deterministic ordering for diff stability.
    raster.sort(key=lambda r: (r["vert"], r["frag"], tuple(r["defines"]), r["site"]))
    compute.sort(key=lambda c: (c["compute"], tuple(c["defines"]), c["site"]))
    single.sort(key=lambda s: (s["shared"], tuple(s["defines"]), s["site"]))

    return {
        "raster": raster,
        "compute": compute,
        "single_file_call_sites": single,
        "master_shaders": MASTER_SHADERS,
    }


def main() -> None:
    data = scan()
    OUT.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
    print(f"wrote {relpath(OUT)}")
    print(f"  raster pairs:           {len(data['raster'])}")
    print(f"  compute programs:       {len(data['compute'])}")
    print(f"  single_file call sites: {len(data['single_file_call_sites'])}")
    print(f"  master shaders:         {len(data['master_shaders'])}")

    # Unique V/F pairs (collapse defines + site).
    uniq = sorted({(r["vert"], r["frag"]) for r in data["raster"]})
    print(f"  unique V/F pairs:       {len(uniq)}")


if __name__ == "__main__":
    main()
