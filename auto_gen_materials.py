import os
import sys
import datetime
prefix = sys.argv[2]
with open(sys.argv[1], "w") as f:
    f.write("#generated materials {}\n".format(datetime.datetime.now()))
    for root, dirs, files in os.walk(os.curdir):
      for file in files:
            path = os.path.splitext(file)
            if path[1] != ".png" and path[1] != ".jpg":
                continue
            f.write("{}/{}\n".format(prefix,path[0]))
            f.write("\timage1 \"{}/{}\"\n".format(prefix,file))