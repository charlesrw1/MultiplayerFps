from codegen_lib import *
import codegen_generate as generate
import time

def do_codegen(lua_output_path : str, path:str, skip_dirs:list[str], full_rebuild:bool):

    print(f"Starting codegen script... fullrebuild={full_rebuild}")
    start_time = time.perf_counter()
    typenames : dict[str,ClassDef] = read_typenames_from_files(skip_dirs)

    print("fixing pts in typenames")
    ClassDef.fixup_types(typenames) # this will sort out parents etc

    end_time = time.perf_counter()
    elapsed_time = (end_time - start_time) * 1000  # Convert to milliseconds
    print(f"read enums and structs in {elapsed_time:.2f} ms")

    GENERATED_ROOT = "./.generated/"
    mega_file_time = 0.0
    try:
        mega_file_time = os.path.getmtime(GENERATED_ROOT+"MEGA.gen.cpp")
    except:
        pass
    print(f"mega file time {mega_file_time}")

    source_files_to_build : list[tuple[str,str]] = get_source_files_to_build(path, skip_dirs, full_rebuild,mega_file_time)
    print(source_files_to_build)
    if len(source_files_to_build)==0:
        print("Skipping")
        return
    source_files_to_build : list[tuple[str,str]] = get_source_files_to_build(path, skip_dirs, full_rebuild,0.0)

    print("Cleaning .generated/...")
    clean_old_source_files(GENERATED_ROOT, full_rebuild)

    print("Parsing files...")
    output_files : list[ParseOutput] = []
    for (root,filename) in source_files_to_build:  
        output : ParseOutput|None = parse_file_for_output(root,filename,typenames)
        if output != None:
            output_files.append(output)

    print("Writing output...")

    mega_output = ParseOutput()
    for o in output_files:
        mega_output.classes += o.classes
        mega_output.additional_includes += o.additional_includes
        mega_output.additional_includes.append(f'"{o.root}/{o.filename}"')

    #for o in output_files:
    #   generate.write_output_file(GENERATED_ROOT,o.filename,o.root,o.classes,o.additional_includes, typenames)
    generate.write_output_file(lua_output_path, GENERATED_ROOT,"MEGA.h",".",mega_output.classes,mega_output.additional_includes,typenames)

    end_time = time.perf_counter()
    elapsed_time = (end_time - start_time) * 1000  # Convert to milliseconds
    print(f"Finished in {elapsed_time:.2f} ms")

