import codegen_run
import os
import sys

SOURCE_DIR = "/../Source/"
DIRS_TO_SKIP = ["./.generated","./External"]
FULL_REBUILD = True
if __name__ == "__main__":
    #try:
        this_dir = os.path.dirname(os.path.abspath(__file__))
        os.chdir(this_dir+SOURCE_DIR)
        codegen_run.do_codegen('.', DIRS_TO_SKIP,FULL_REBUILD)
    #except Exception as e:
     #   print(e.args[0])
     #   sys.exit(1)