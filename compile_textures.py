import os
import sys
import subprocess

MAT_DIR = "./Data/Materials/"
TEXTURE_DIR = "./Data/Textures/"

#read mat files, for each texture, determine type (specular/normal/color)
#if dds doesn't exist or source file modified, generate dds file with nvdxt

ALBEDO_MAX_SIZE = 1024
NORMAL_MAX_SIZE = 512
ROUGHNESS_MAX_SIZE = 512
METALNESS_MAX_SIZE = 512
AO_MAX_SIZE = 512

def compile_texture_list(texture_list, alphamode):
    for ttype, path in texture_list:
        last_modified_time = os.path.getmtime(TEXTURE_DIR+path)
    
        dds_file = TEXTURE_DIR + path[0:path.rfind('.')]+".dds"
        if os.path.isfile(dds_file) and os.path.getmtime(dds_file) > last_modified_time:
            continue

        path_folder = ""
        if path.rfind('/')!=-1:
            path_folder = path[0:path.rfind('/')+1]

        if ttype[-1].isdigit():
            ttype = ttype[:-1]

        #specular textures get dxt1, normalmaps get uncompressed, color textures get dxt1 or dxt5
        args = "-dxt1a"

        if ttype == "albedo":
            if alphamode:
                args = "-dxt5"
            else:
                args = "-dxt1a"
        elif ttype == "normal":
            args = "-u888 -swapRB"
        elif ttype == "special" or ttype == "aux":
            args = "-dxt1a"
        else:
            print("unknown tex type {}".format(ttype))
            continue

        nvdxt_args = "-outdir {}".format(TEXTURE_DIR+path_folder)
        nvdxt_args += " " + args
        nvdxt_args += " -file {}".format(TEXTURE_DIR + path)
        nvdxt_args += " -quick"

        print("Executing: nvdxt.exe "+ nvdxt_args)

        command_line = ["nvdxt.exe"]
        for arg in nvdxt_args.split():
            command_line.append(arg)

        subprocess.run(command_line)


matfiles = os.listdir(MAT_DIR)

IMAGE_DECL = ["albedo","normal","rough","metal","ao","special"]

for matfile in matfiles:
    if os.path.isdir(MAT_DIR+matfile):
        continue
    with open(MAT_DIR+matfile, "r") as file:
        cur_name : str = ""
        alphamode : str = ""
        images = []
        for line in file:
            stripped_line = line.strip()
            if not stripped_line or stripped_line[0]=='#':
                continue
            if line[0]!='\t':
                #if we are entering a new material, compile last one's textures
                if cur_name:
                    compile_texture_list(images, alphamode)
                
                cur_name = line
                alphamode = ""
                images.clear()
            else:
                tokens = line.strip().split()
                if tokens:
                    if tokens[0] in IMAGE_DECL:
                        images.append((tokens[0],tokens[1].replace('"','')))
                    elif tokens[0] == "alpha":
                        alphamode = tokens[1]

        if cur_name:
            compile_texture_list(images, alphamode)