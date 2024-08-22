import os
import sys
import subprocess
from pathlib import Path
import time
MAT_DIR = "./Data/"
TEXTURE_DIR = "./Data/"

current_time = time.time()

def process_tex_file(filepath):
    with open(filepath, "r") as file:

        modification_time_of_def = os.path.getmtime(filepath)

        for line in file:
            if len(line) == 0:
                continue
            tokens = line.split()
            input_image = TEXTURE_DIR+tokens[0]
            
            if not os.path.exists(input_image):
                print("input image does not exist: {}".format(input_image))
                continue

            dds_file = TEXTURE_DIR+Path(input_image).stem + ".dds"

            needs_output = not os.path.exists(dds_file) or os.path.getmtime(dds_file) < modification_time_of_def
            needs_output = needs_output or os.path.getmtime(input_image) >  os.path.getmtime(dds_file)

            if not needs_output:
                continue

            format = "BC1_UNORM"
            if len(tokens)>1:
                format = tokens[1]
            
            nvdxt_args = "-f " + format 
            nvdxt_args += " -y "
            nvdxt_args += " -o " + str(Path(input_image).parent) + " "
            nvdxt_args += input_image

            print("Executing: texconv.exe "+ nvdxt_args)

            command_line = ["./x64/Debug/texconv.exe"]
            for arg in nvdxt_args.split():
                command_line.append(arg)

            subprocess.run(command_line)










def walk_directory_and_process_tex_files(root_directory):
    for dirpath, dirnames, filenames in os.walk(root_directory):
        for filename in filenames:
            if filename.endswith('.tex'):
                file_path = os.path.join(dirpath, filename)
                process_tex_file(file_path)

if __name__ == "__main__":
    root_directory = TEXTURE_DIR
    walk_directory_and_process_tex_files(root_directory)