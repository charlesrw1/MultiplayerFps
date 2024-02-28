import os
import sys
import subprocess
import lz4.frame
import struct

DATA_DIR = "./Data/"

PAK_VERSION = 1

class Archive_File_Flags:
    COMPRESSED = 1
    TEXT = 2

class Archive_Entry:
    string_offset : int     # 4 bytes
    data_offset : int       # 8
    data_length : int       # 12 
    original_size : int     # 16
    flags : int             # 20 bytes

    path = ""

    
class Archive_Header:
    #magic + version          8 bytes
    num_entries : int       # 12
    data_offset : int       # 16

# format
# 16 byte header
# N*20 byte entries
# string table[]
# data[]

archive_entries = []

ACCEPTED_FORMATS = [".txt",".dds",".ttf",".hdr",".glb",".txt"]

for root, dirs, files in os.walk(DATA_DIR):
      for file in files:
            real_root = root[len(DATA_DIR):]
            real_root = real_root.replace('\\','/')
            ext = file[file.rfind('.'):]
            if ext not in ACCEPTED_FORMATS:
                 continue
            
            entry = Archive_Entry()
            entry.path = real_root + "/" + file
            archive_entries.append(entry)
            print("{}".format(entry.path))

with open("archive.dat","bw") as archive:
    header = Archive_Header
    entry_offset = 16
    string_offset = 16 + 20*len(archive_entries)
    header.num_entries = len(archive_entries)

    archive.write(b'\x00'*string_offset)
    for entry in archive_entries:
          path : str = entry.path
          entry.string_offset = archive.tell() - string_offset
          archive.write(path.encode('ascii'))
          archive.write(b'\x00')
    
    header.data_offset = archive.tell()

    for entry in archive_entries:
         full_path : str = DATA_DIR + entry.path
         with open(full_path, "br") as file:
              file.seek(0)
              ba = bytearray(file.read())
              uncompressed_size = len(ba)
              entry.original_size = uncompressed_size
              entry.data_length = uncompressed_size
              entry.data_offset = archive.tell()
              entry.flags = 0

              archive.write(ba)

    archive.seek(0)
    archive.write(b'ABCD')
    archive.write(struct.pack("<I",PAK_VERSION))
    archive.write(struct.pack("<I", header.num_entries))
    archive.write(struct.pack("<I", header.data_offset))

    for entry in archive_entries:
         archive.write(struct.pack("<I", entry.string_offset))
         archive.write(struct.pack("<I", entry.data_offset))
         archive.write(struct.pack("<I", entry.data_length))
         archive.write(struct.pack("<I", entry.original_size))
         archive.write(struct.pack("<I", entry.flags))


    





