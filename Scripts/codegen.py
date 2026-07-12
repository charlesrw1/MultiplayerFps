import codegen_run
import codegen_lib
import os
import sys

SOURCE_DIR = "/../Source/"
DIRS_TO_SKIP = ["./.generated","./External"]
FULL_REBUILD = False
LUA_OUTPUT_PATH = "./../TestFilesData/scripts"

if __name__ == "__main__":
    do_rebuild = FULL_REBUILD or "rebuild" in sys.argv
    # --strict makes unknown types a hard error instead of silently degrading.
    if "--strict" in sys.argv or "strict" in sys.argv:
        codegen_lib.STRICT_UNKNOWN_TYPES = True
        print("strict mode: unknown types will be hard errors")
    if do_rebuild:
        print("doing full rebuild")
    this_dir = os.path.dirname(os.path.abspath(__file__))
    os.chdir(this_dir+SOURCE_DIR)
    try:
        codegen_run.do_codegen(LUA_OUTPUT_PATH, '.', DIRS_TO_SKIP,do_rebuild)
    except codegen_lib.CodegenError as e:
        # Already pre-formatted as file:line:col: error: message
        print(str(e))
        sys.exit(1)