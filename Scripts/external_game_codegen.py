import argparse
import os
from pathlib import Path

import codegen_generate as generate
from codegen_lib import ClassDef, ParseOutput, get_source_files_to_build, parse_file_for_output, read_typenames_from_files


def read_types(root: Path, skip_dirs: list[str]):
    previous = Path.cwd()
    try:
        os.chdir(root)
        return read_typenames_from_files(skip_dirs)
    finally:
        os.chdir(previous)


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate reflection code for an external CsRemake game")
    parser.add_argument("--engine-root", required=True)
    parser.add_argument("--game-root", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()

    engine_root = Path(args.engine_root).resolve()
    game_root = Path(args.game_root).resolve()
    game_source = game_root / "Source"
    output = Path(args.output).resolve()
    lua_output = output / "lua"
    output.mkdir(parents=True, exist_ok=True)
    lua_output.mkdir(parents=True, exist_ok=True)

    if not game_source.is_dir():
        raise SystemExit(f"external game source directory not found: {game_source}")

    engine_types = read_types(
        engine_root / "Source",
        ["./.generated", "./External", "./MyGame"],
    )
    game_types = read_types(game_source, ["./.generated"])
    typenames = engine_types | game_types
    ClassDef.fixup_types(typenames)

    previous = Path.cwd()
    try:
        os.chdir(game_source)
        source_files = get_source_files_to_build(".", ["./.generated"], True, 0.0)
        combined = ParseOutput()
        for root, filename in source_files:
            parsed = parse_file_for_output(root, filename, typenames)
            if parsed is None:
                continue
            combined.classes += parsed.classes
            combined.additional_includes += parsed.additional_includes
            combined.additional_includes.append(f'"{root}/{filename}"')

        generate.write_output_file(
            str(lua_output),
            str(output) + os.sep,
            "ExternalGame.h",
            ".",
            combined.classes,
            combined.additional_includes,
            typenames,
            "game_lua_stubs.lua",
        )
    finally:
        os.chdir(previous)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
