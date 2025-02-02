import os
import sys
import shlex
from pathlib import Path
import time

VERSION = 1

NONE_TYPE = 0
INT_TYPE = 1
BOOL_TYPE = 2
FLOAT_TYPE = 3
STRING_TYPE = 4
ENTITYPTR_TYPE = 5
ASSETPTR_TYPE = 6
VEC3_TYPE = 7
QUAT_TYPE = 8
FUNCTION_TYPE = 9
MULTICAST_TYPE = 10
LIST_TYPE = 11
COLOR32_TYPE = 12
ENUM_TYPE = 13
STRUCT_TYPE = 14

CLASS_OBJECT = 0
STRUCT_OBJECT = 1
ENUM_OBJECT = 2

class Property:
    def __init__(self):
        self.name = ""
        self.nameoverride = ""
        self.transient = False
        self.hide = False
        self.prop_type : int = NONE_TYPE
        self.tooltip = ""
        self.custom_type = ""
        self.list_struct = ""
        self.hint = ""
        self.is_getter = False
        self.enum_or_struct_name = ""
    def get_flags(self):
        if not self.transient and not self.hide:
            return "PROP_DEFAULT"
        if self.transient:
            return "PROP_EDITABLE"
        if self.hide:
            return "PROP_SERIALIZE"
        return "0"


class ClassDef:
    def __init__(self):
        self.object_type = CLASS_OBJECT
        self.classname = ""
        self.supername = ""
        self.properties = []

def write_headers(path, additional_includes):
    out = f"// **** GENERATED SOURCE FILE version:{VERSION} ****\n"
    out += f"#include \"{path}\"\n"
    out += "#include \"Framework/ReflectionProp.h\"\n"
    out += "#include \"Framework/ReflectionMacros.h\"\n"
    out += "#include \"Game/AssetPtrMacro.h\"\n"
    out += "#include \"Game/AssetPtrArrayMacro.h\"\n"
    out += "#include \"Game/EntityPtrMacro.h\"\n"
    out += "#include \"Scripting/FunctionReflection.h\"\n"
    out += "#include \"Framework/VectorReflect2.h\"\n"
    out += "#include \"Framework/EnumDefReflection.h\"\n"
    for inc in additional_includes:
        out += f"#include {inc}\n"

    return out + "\n"

# code gen to make macros to make templates to make code ...
def write_prop(prop : Property):

    prop.custom_type = f'"{prop.custom_type}"'
    prop.hint = f'"{prop.hint}"'


    if prop.prop_type == BOOL_TYPE:
        return f"REG_BOOL_W_CUSTOM({prop.name},{prop.get_flags()},{prop.custom_type},{prop.hint})"
    elif prop.prop_type == FLOAT_TYPE:
        return f"REG_FLOAT({prop.name},{prop.get_flags()},{prop.hint})"
    elif prop.prop_type == INT_TYPE:
        return f"REG_INT({prop.name},{prop.get_flags()},{prop.hint})"
    elif prop.prop_type == ASSETPTR_TYPE:
        return f"REG_ASSET_PTR({prop.name},{prop.get_flags()})"
    elif prop.prop_type == STRING_TYPE:
        return f"REG_STDSTRING_CUSTOM_TYPE({prop.name},{prop.get_flags()},{prop.custom_type})"
    elif prop.prop_type == ENTITYPTR_TYPE:
        return f"REG_ENTITY_PTR({prop.name},{prop.get_flags()})"
    elif prop.prop_type == FUNCTION_TYPE:
        name = prop.nameoverride if len(prop.nameoverride)!=0 else prop.name
        name = f'"{name}"'
        if prop.is_getter:
            return f"REG_GETTER_FUNCTION({prop.name},{name})"
        else:
            return f"REG_FUNCTION_EXPLICIT_NAME({prop.name},{name})"
    elif prop.prop_type == MULTICAST_TYPE:
        return f"REG_MULTICAST_DELEGATE({prop.name})"
    elif prop.prop_type == VEC3_TYPE:
        return f"REG_VEC3({prop.name},{prop.get_flags()})"
    elif prop.prop_type == QUAT_TYPE:
        return f"REG_QUAT({prop.name},{prop.get_flags()})"
    elif prop.prop_type == LIST_TYPE:
        return f"REG_STDVECTOR_NEW({prop.name},{prop.get_flags()})"
    elif prop.prop_type == COLOR32_TYPE:
        return f"REG_INT_W_CUSTOM({prop.name}, {prop.get_flags()}, \"\", \"ColorUint\")"
    elif prop.prop_type == NONE_TYPE:
        return f"REG_STRUCT_CUSTOM_TYPE({prop.name},{prop.get_flags()},{prop.custom_type})"
    elif prop.prop_type == ENUM_TYPE:
        return f"REG_ENUM({prop.name},{prop.get_flags()},{prop.hint},{prop.enum_or_struct_name})"
    else:
        assert(0)
    


def write_class(newclass : ClassDef):
    output = ""
    if newclass.object_type==CLASS_OBJECT:
        output += f"CLASS_IMPL({newclass.classname});\n"
    if newclass.object_type == CLASS_OBJECT or newclass.object_type == STRUCT_OBJECT:
        output += f"const PropertyInfoList* {newclass.classname}::get_props()\n"
        output += "{\n"
        if len(newclass.properties) > 0:
            for p in newclass.properties:
                if p.prop_type == LIST_TYPE:
                    output += f"\tMAKE_VECTOR_ATOM_CALLBACKS_ASSUMED({p.name})\n"


            output += f"    START_PROPS({newclass.classname})"
            for p in newclass.properties:
                output += "\n\t\t" + write_prop(p) + ","
            output=output[:-1]
            output += f"\n    END_PROPS({newclass.classname})\n"
        else:
            output += "\treturn nullptr;\n"
        output += "}\n\n"
    elif newclass.object_type == ENUM_OBJECT:
        output += f"static EnumIntPair enumstrs{newclass.classname}[] = " + "{\n"
        for p in newclass.properties:
            name = f'"{p.name}"'
            displayname = f'"{p.nameoverride}"'
            full_name = f"{newclass.classname}::{p.name}"
            output += f"\tEnumIntPair({name},{displayname},(int64_t){full_name}),\n"
        output = output[:-2]+"\n};\n"

        output += f"EnumTypeInfo EnumTrait<{newclass.classname}>::StaticType = EnumTypeInfo(\"{newclass.classname}\",enumstrs{newclass.classname},{len(newclass.properties)});\n\n"
    return output

def parse_reflect_macro(line : str):
    start_p = line.find("(")
    end_p = line.rfind(")")
    if start_p == -1 or end_p == -1:
        raise Exception("expected REFLECT(...)")
    argstr = line[start_p+1:end_p]
    argstr = argstr.replace(","," , ")
    argstr = argstr.replace("="," = ")
    tokens = shlex.split(argstr)
    token_iter = iter(tokens)

    current_prop = Property()
    try:
        for t in token_iter:
            if t == "type":
                if next(token_iter) != "=":
                    raise Exception("expected '=' after 'type'")
                current_prop.custom_type = next(token_iter)
            elif t=="name":
                if next(token_iter) != "=":
                    raise Exception("expected '=' after 'type'")
                current_prop.nameoverride = next(token_iter)
            elif t=="transient":
                current_prop.transient = True
            elif t=="hide":
                current_prop.hide = True
            elif t=="tooltip":
                if next(token_iter) != "=":
                    raise Exception("expected '=' after 'type'")
                current_prop.tooltip = next(token_iter)
            elif t=="hint":
                if next(token_iter) != "=":
                    raise Exception("expected '=' after 'type'")
                current_prop.hint = next(token_iter)
            elif t=="getter":
                current_prop.is_getter = True
            else:
                raise Exception(f"unexpected REFLECT(...) arg \"{t}\"")

            next_or_final = next(token_iter, None)
            if next_or_final == None:
                break
            if next_or_final!=",":
                raise Exception("expected comma between args")
    except StopIteration as e:
        raise Exception("unexpected REFLECT() arg end")
    
    return current_prop

gLine = ""
gLineNum = 0


def parse_enum(enumname, file_iter : enumerate[str]) -> ClassDef:
    obj = ClassDef()
    obj.classname = enumname
    obj.object_type = ENUM_OBJECT

    current_prop = None
    for lLineNum,lLine in file_iter:
        gLine = lLine
        gLineNum = lLineNum

        line = lLine.strip()
        if line == "};":
            return obj
        elif line.startswith("REFLECT"):
            current_prop = parse_reflect_macro(line) # parse name overrides
        elif line.startswith("//"):
            continue
        elif len(line) == 0:
            continue
        elif line.startswith("{"): 
            continue
        else:
            line = line.replace("="," = ")
            line = line.replace(","," , ")

            value = line.split()[0]
            if current_prop == None:
                current_prop = Property()
            current_prop.name = value
            obj.properties.append(current_prop)
            current_prop = None
    if current_prop != None:
        raise Exception(f"Parsing enum '{enumname}': REFLECT() should have preceded enum value")
    
    return obj

struct_names = []
enum_names = []

def parse_file(root, file_name):
    #print(f"File: {file_name}")
    classes = []
    additional_includes = []
    current_class = None
    file_path = "{}/{}".format(root,file_name)
    with open(file_path,"r") as file:
        file_iter = enumerate(iter(file),start=1)
        gLineNum  = 0
        gLine = ""
        try:
            for gLineNum,gLine in file_iter:
                line = gLine.strip()
                if line.startswith("NEWCLASS"):
                    start_p = line.find("(")
                    end_p = line.find(")")
                    comma = line.find(",")
                    if start_p != -1 and end_p != -1 and comma != -1:
                        classname = line[start_p+1:comma].strip()
                        supername = line[comma+1:end_p].strip()
                        print(f"FOUND CLASS: {classname} : {supername}")
                        if current_class != None:
                            classes.append(current_class)
                        current_class = ClassDef()
                        current_class.classname = classname
                        current_class.supername = supername
                    else:
                        raise Exception("expected NEWCLASS(<classname>, <super>")
                elif line.startswith("NEWSTRUCT"):
                    start_p = line.find("(")
                    end_p = line.find(")")
                    if start_p==-1 or end_p==-1:
                        raise Exception("expected NEWSTRUCT(<structname>)")
                    structname = line[start_p+1:end_p]
                    print(f"FOUND STRUCT: {structname}")
                    if current_class != None:
                        classes.append(current_class)
                    current_class = ClassDef()
                    current_class.classname = structname
                    current_class.object_type = STRUCT_OBJECT

                elif line.startswith("NEWENUM"):
                    start_p = line.find("(")
                    end_p = line.find(")")
                    comma = line.find(",")
                    if start_p==-1 or end_p==-1:
                        raise Exception("expected NEWENUM(<enumname>)")
                    enumname = line[start_p+1:comma].strip()
                    print(f"FOUND ENUM: {enumname}")
                    if current_class != None:
                        classes.append(current_class)
                    current_class = None
                    
                    outclass = parse_enum(enumname,file_iter)
                    classes.append(outclass)

                elif line.startswith("REFLECT"):
                    if current_class == None:
                        raise Exception("REFLECT seen before NEWCLASS")
                    
                    current_prop = parse_reflect_macro(line)

                    gLineNum,gLine = next(file_iter)
                    line = gLine.strip()
                    assignment_sign = line.find("=")
                    l_paren = line.find("(")
                    r_paren = line.find(")")
                    if l_paren != -1 and r_paren != -1 and (r_paren < assignment_sign or assignment_sign == -1):
                        line = line.replace("("," ( ")
                        tokens = shlex.split(line)
                        index = tokens.index("(")
                        func_name = tokens[index-1]
                        current_prop.prop_type = FUNCTION_TYPE
                        current_prop.name = func_name
                    else:
                        # get variable name
                        line = line.replace("="," = ")
                        line = line.replace(";", " ; ")
                        tokens = shlex.split(line)
                        index = -1
                        try:
                            index = tokens.index("=")
                        except Exception as e:
                            index = -1
                        if index == -1:
                            index = tokens.index(";")
                        varname = tokens[index-1]
                        current_prop.name = varname
                        matches = [
                            ("std::string",STRING_TYPE),
                            ("MulticastDelegate",MULTICAST_TYPE),
                            ("AssetPtr",ASSETPTR_TYPE),
                            ("std::vector",LIST_TYPE),
                            ("float",FLOAT_TYPE),
                            ("bool",BOOL_TYPE),
                            ("int",INT_TYPE),
                            ("uint32_t",INT_TYPE),
                            ("int8_t",INT_TYPE), 
                            ("uint8_t",INT_TYPE),
                            ("EntityPtr",ENTITYPTR_TYPE),
                            ("glm::vec3",VEC3_TYPE),
                            ("glm::quat",QUAT_TYPE),
                            ("Color32",COLOR32_TYPE)
                            ]
                        current_prop.prop_type = NONE_TYPE
                        for (str,type) in matches:
                            if line.startswith(str):
                                current_prop.prop_type = type
                                break
                        if current_prop.prop_type==NONE_TYPE:
                            type = tokens[0]
                            if type in enum_names:
                                current_prop.prop_type = ENUM_TYPE
                                current_prop.enum_or_struct_name = type
                            elif type in struct_names:
                                current_prop.prop_type = STRUCT_TYPE
                                current_prop.enum_or_struct_name = type
                        if current_prop.prop_type==NONE_TYPE and len(current_prop.custom_type)==0:
                            raise Exception(f"unknown type for variable, use 'type=' for structs")
                    current_class.properties.append(current_prop)
                elif line.startswith("GENERATED_CLASS_INCLUDE"):
                    start_p = line.find("(")
                    end_p = line.find(")")
                    if start_p == -1 or end_p == -1:
                        raise Exception("expcted GENERATED_CLASS_INCLUDE(...)")
                    include_name = line[start_p+1:end_p].strip()
                    additional_includes.append(include_name)
        except Exception as e:
            raise Exception("{}:{}:\"{}\"-{}".format(file_name,gLineNum,gLine,e.args[0]))
    if current_class != None:
        classes.append(current_class)
    return (classes,additional_includes)

skip_dirs = ["./.generated","./External"]

#just parse the whole project, takes under a second for me
def read_enum_and_struct_values():
    start_time = time.perf_counter()
    for root, dirs, files in os.walk("."):
        if root.replace("\\","/") in skip_dirs:
            continue

        for file_name in files:
            if os.path.splitext(file_name)[-1] != ".h":
                continue
            with open(root+"/"+file_name,"r") as file:
                for line in file:
                    line = line.strip()
                    if line.startswith("NEWENUM"):
                        start_p = line.find("(")
                        end_p = line.find(")")
                        comma = line.find(",")
                        if start_p==-1 or end_p==-1:
                            continue
                        enumname = line[start_p+1:comma].strip()
                        enum_names.append(enumname)
                    elif line.startswith("NEWSTRUCT"):
                        start_p = line.find("(")
                        end_p = line.find(")")
                        if start_p==-1 or end_p==-1:
                            continue
                        structname = line[start_p+1:end_p]
                        struct_names.append(structname)
    end_time = time.perf_counter()
    elapsed_time = (end_time - start_time) * 1000  # Convert to milliseconds
    print(f"read enums and structs in {elapsed_time:.2f} ms")



def do_codegen(path):
    print("Starting codegen script...")
    start_time = time.perf_counter()

    read_enum_and_struct_values()

    all_classes = []
    source_files_to_build = []

    for root, dirs, files in os.walk(path):
        if root.replace("\\","/") in skip_dirs:
            continue

        for file_name in files:
            if os.path.splitext(file_name)[-1] != ".h":
                continue

            generated_path : str = "./.generated" + (root[1:] if root == "." else root[1:]) + "/"
            generated_path += os.path.splitext(file_name)[-2] + ".gen.cpp"
            generated_path.replace("\\","/")
            if not os.path.exists(generated_path):
                source_files_to_build.append( (root,file_name) )
            else:
                if os.path.getmtime(root+"/"+file_name) > os.path.getmtime(generated_path):
                    source_files_to_build.append( (root,file_name) )
    print("Cleaning .generated/...")
    GENERATED_ROOT = "./.generated/"
    for root,dirs,files in os.walk(GENERATED_ROOT):
        root = root.replace("\\","/")
        for file_name in files:
            input_generated = list(Path(root+"/"+file_name).parts)
            without_gen = input_generated[1:]
            last_part = input_generated[-1]
            without_gen[-1] = last_part[:last_part.find(".")]+".h"
            actual_path = "/".join(without_gen)
            
            if not os.path.exists(actual_path):
                path_to_remove = (root + "/" + file_name).replace("\\","/")
                print(f"removing unused file: {path_to_remove}")
                os.remove(path_to_remove)


    for (root,filename) in source_files_to_build:  
        all_classes = []
        additional_includes = []
        try:
            all_classes,additional_includes = parse_file(root,filename)  
        except Exception as e:
            print(e.args[0])
            sys.exit(1)
        else:
            generated_path = root + "/" + os.path.splitext(filename)[0]
            if generated_path[0]=='.':
                generated_path=generated_path[2:]
            generated_path = "./.generated/"+generated_path+".gen.cpp"
            generated_path.replace("\\","/")

            os.makedirs(os.path.dirname(generated_path), exist_ok=True)
            with open(generated_path,"w") as file:
                print(f"Writing {generated_path}")
                if len(all_classes)>0:
                    file.write(write_headers(root+"/"+filename, additional_includes))
                    for c in all_classes:
                        file.write(write_class(c))

    end_time = time.perf_counter()
    elapsed_time = (end_time - start_time) * 1000  # Convert to milliseconds
    print(f"Finished in {elapsed_time:.2f} ms")

try:
    this_dir = os.path.dirname(os.path.abspath(__file__))
    os.chdir(this_dir+"/../Source/")
    do_codegen('.')
except Exception as e:
    print("unknown codegen error")
    sys.exit(1)