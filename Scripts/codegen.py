import codegen_lib
import os
import sys

SOURCE_DIR = "/../Source/"
DIRS_TO_SKIP = ["./.generated","./External"]
FULL_REBUILD = False
if __name__ == "__main__":
    try:
        this_dir = os.path.dirname(os.path.abspath(__file__))
        os.chdir(this_dir+SOURCE_DIR)
        codegen_lib.do_codegen('.', DIRS_TO_SKIP,FULL_REBUILD)
    except Exception as e:
        print("unknown codegen error")
        sys.exit(1)