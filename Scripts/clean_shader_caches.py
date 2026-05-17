"""Clean cached shader artefacts so the next launch regenerates them.

Two kinds of caches confuse iteration on shader / material code:

  - ShaderCache/*.bin
      Driver-blob cache emitted by the engine's program manager. Stale
      blobs survive shader source changes via filename hash collisions
      in some edge cases.

  - Data/**/*_shader.glsl
      Generated GLSL emitted by MaterialLocal_Loading.cpp::load_from_file
      (see line ~282) when EDITOR_BUILD is defined. Each .pmm material's
      master-shader expansion lands here; the engine regenerates on load
      when the source-side timestamp is newer, but big sweeps (rename a
      varying across MASTER, edit shader_links, etc.) leave content-stale
      files with new-enough timestamps that the engine accepts.

Run after any of:
  - Scripts/assign_shader_locations.py
  - editing MASTER/*.txt or any varying name / type
  - editing the SPIR-V toolchain

Both directories are safe to wipe: regeneration is automatic on next run.

Usage:
  py Scripts/clean_shader_caches.py            # delete everything (default)
  py Scripts/clean_shader_caches.py --dry-run  # list what would go
  py Scripts/clean_shader_caches.py --cache-only
  py Scripts/clean_shader_caches.py --glsl-only
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SHADER_CACHE = ROOT / "ShaderCache"
DATA = ROOT / "Data"


def find_cache_bins() -> list[Path]:
    if not SHADER_CACHE.is_dir():
        return []
    return sorted(p for p in SHADER_CACHE.iterdir() if p.is_file())


def find_generated_glsl() -> list[Path]:
    if not DATA.is_dir():
        return []
    return sorted(DATA.rglob("*.glsl"))


def delete(paths: list[Path], label: str, dry_run: bool) -> int:
    if not paths:
        print(f"{label}: nothing to clean")
        return 0
    verb = "would delete" if dry_run else "deleting"
    print(f"{label}: {verb} {len(paths)} file(s)")
    if dry_run:
        for p in paths[:5]:
            print(f"  {p.relative_to(ROOT).as_posix()}")
        if len(paths) > 5:
            print(f"  ... and {len(paths) - 5} more")
        return 0
    errors = 0
    for p in paths:
        try:
            p.unlink()
        except OSError as e:
            print(f"  failed to remove {p}: {e}", file=sys.stderr)
            errors += 1
    return errors


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--cache-only", action="store_true")
    ap.add_argument("--glsl-only", action="store_true")
    args = ap.parse_args()

    if args.cache_only and args.glsl_only:
        ap.error("--cache-only and --glsl-only are mutually exclusive")

    errors = 0
    if not args.glsl_only:
        errors += delete(find_cache_bins(), "ShaderCache/*", args.dry_run)
    if not args.cache_only:
        errors += delete(find_generated_glsl(), "Data/**/*.glsl", args.dry_run)

    return 1 if errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
