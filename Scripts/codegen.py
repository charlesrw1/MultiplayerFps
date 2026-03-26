import codegen_run
import os
import sys

SOURCE_DIR = "/../Source/"
DIRS_TO_SKIP = ["./.generated","./External"]
FULL_REBUILD = False
LUA_OUTPUT_PATH = "./../Data/scripts"

if __name__ == "__main__":
    #try:
    do_rebuild = FULL_REBUILD or "rebuild" in sys.argv
    if do_rebuild:
        print("doing full rebuild")
    this_dir = os.path.dirname(os.path.abspath(__file__))
    os.chdir(this_dir+SOURCE_DIR)
    codegen_run.do_codegen(LUA_OUTPUT_PATH, '.', DIRS_TO_SKIP,do_rebuild)
    #except Exception as e:
     #   print(e.args[0])
     #   sys.exit(1)