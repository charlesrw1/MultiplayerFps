import os

with open("manifest.txt", "w") as f:
    for root, dirs, files in os.walk(os.curdir):
      for file in files:
            print(file)
            f.write(file+"\n")