import codegen_run
import codegen_lib
import os
import sys

SOURCE_DIR = "/../Source/"
BASE_SKIP_DIRS = ["./.generated","./External"]
# Sibling project dirs that have their own codegen pass (own vcxproj + own mega file).
# Core's own pass (path==".") must skip these so it doesn't double-generate their classes.
# Add one line here whenever a new project splits itself out of Core.
SPLIT_PROJECT_DIRS = ["./MyGame"]
FULL_REBUILD = False
LUA_OUTPUT_PATH = "./../TestFilesData/scripts"

if __name__ == "__main__":
    positional = [a for a in sys.argv[1:] if a not in ("rebuild", "strict", "--strict")]
    codegen_path = positional[0] if len(positional) > 0 else "."
    mega_name = positional[1] if len(positional) > 1 else "MEGA"

    do_rebuild = FULL_REBUILD or "rebuild" in sys.argv
    # --strict makes unknown types a hard error instead of silently degrading.
    if "--strict" in sys.argv or "strict" in sys.argv:
        codegen_lib.STRICT_UNKNOWN_TYPES = True
        print("strict mode: unknown types will be hard errors")
    if do_rebuild:
        print("doing full rebuild")
    this_dir = os.path.dirname(os.path.abspath(__file__))
    os.chdir(this_dir+SOURCE_DIR)

    skip_dirs = BASE_SKIP_DIRS + SPLIT_PROJECT_DIRS if codegen_path == "." else BASE_SKIP_DIRS
    generated_root = codegen_path.rstrip("/") + "/.generated/" if codegen_path != "." else "./.generated/"
    # "lua_stubs.lua" is the documented, authoritative LSP stub filename (see docs/scripting_system.md) -
    # keep it exact for Core's own pass; sibling projects get their own file so they don't clobber it.
    lua_stub_filename = "lua_stubs.lua" if mega_name == "MEGA" else mega_name.lower() + "_lua_stubs.lua"

    try:
        codegen_run.do_codegen(LUA_OUTPUT_PATH, codegen_path, skip_dirs, do_rebuild,
                                generated_root=generated_root, mega_name=mega_name,
                                lua_stub_filename=lua_stub_filename)
    except codegen_lib.CodegenError as e:
        # Already pre-formatted as file:line:col: error: message
        print(str(e))
        sys.exit(1)