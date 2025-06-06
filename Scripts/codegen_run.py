from codegen_lib import *
import codegen_generate as generate
import time

def do_codegen(path:str, skip_dirs:list[str], full_rebuild:bool):
    print("Starting codegen script...")
    start_time = time.perf_counter()
    typenames : dict[str,ClassDef] = read_typenames_from_files(skip_dirs)

    print("fixing pts in typenames")
    typenames["ClassBase"] = ClassDef(["ClassBase"],ClassDef.TYPE_CLASS)
    ClassDef.fixup_types(typenames) # this will sort out parents etc

    end_time = time.perf_counter()
    elapsed_time = (end_time - start_time) * 1000  # Convert to milliseconds
    print(f"read enums and structs in {elapsed_time:.2f} ms")

    source_files_to_build : list[tuple[str,str]] = get_source_files_to_build(path, skip_dirs, full_rebuild)

    GENERATED_ROOT = "./.generated/"
    print("Cleaning .generated/...")
    clean_old_source_files(GENERATED_ROOT, full_rebuild)

    print("Parsing files...")
    output_files : list[ParseOutput] = []
    for (root,filename) in source_files_to_build:  
        output : ParseOutput|None = parse_file_for_output(root,filename,typenames)
        if output != None:
            output_files.append(output)

    print("Writing output...")
    for o in output_files:
        generate.write_output_file(GENERATED_ROOT,o.filename,o.root,o.classes,o.additional_includes, typenames)

    end_time = time.perf_counter()
    elapsed_time = (end_time - start_time) * 1000  # Convert to milliseconds
    print(f"Finished in {elapsed_time:.2f} ms")

