from io import TextIOWrapper
import os
#from pickle import NONE
#from pyclbr import Class
#import sys
from pydoc import classname
import shlex
from pathlib import Path
import time
import itertools
from typing import List, Optional


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
SOFTASSETPTR_TYPE = 15
CLASSTYPEINFO_TYPE = 16
UNORDERED_SET = 17
UNORDERED_MAP = 18

class CppType:
    def __init__(self, type: int, templates: Optional[List["CppType"]] = None, const: bool = False, typename: str = "") -> None:
        self.type: int = type
        self.template_args: List["CppType"] = templates if templates is not None else []
        self.constant: bool = const
        self.typename: str = typename

        
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

        # or return type
        self.new_type : CppType|None = None
        self.func_args : list[tuple[CppType,str]] = []

        self.is_static = False

        self.tooltip = ""
        self.custom_type = ""
        self.list_struct = ""
        self.hint = ""
        self.is_getter = False
        self.is_setter = False
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
    def __init__(self, name_and_bases : list[str]):
        self.object_type = CLASS_OBJECT
        self.classname = name_and_bases[0]
        if len(name_and_bases) > 1:
            self.supername = name_and_bases[1]
        self.interfaces : list[str] = []
        if len(name_and_bases) > 2:
            self.interfaces = name_and_bases[2:]
        self.properties : list[Property] = []

def write_headers(path:str, additional_includes:list[str]):
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
def write_prop(prop : Property) -> str:
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
    elif prop.prop_type == SOFTASSETPTR_TYPE:
        return f"REG_SOFT_ASSET_PTR({prop.name},{prop.get_flags()})"
    else:
        assert(0)
    return ""
    
def write_class(newclass : ClassDef):
    output = ""
    if newclass.object_type==CLASS_OBJECT:
        output += f"ClassTypeInfo {newclass.classname}::StaticType = ClassTypeInfo(\n \
                    \"{newclass.classname}\",\n \
                    &{newclass.supername}::StaticType,\n \
                    {newclass.classname}::get_props,\n \
                    default_class_create<{newclass.classname}>(),\n \
                    {newclass.classname}::CreateDefaultObject\n \
                );\n"


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
        nameWithoutColons = newclass.classname.replace("::","")

        output += f"static EnumIntPair enumstrs{nameWithoutColons}[] = " + "{\n"
        for p in newclass.properties:
            name = f'"{p.name}"'
            displayname = f'"{p.nameoverride}"'
            full_name = f"{newclass.classname}::{p.name}"
            output += f"\tEnumIntPair({name},{displayname},(int64_t){full_name}),\n"
        output = output[:-2]+"\n};\n"

        output += f"EnumTypeInfo EnumTrait<{newclass.classname}>::StaticEnumType = EnumTypeInfo(\"{newclass.classname}\",enumstrs{nameWithoutColons},{len(newclass.properties)});\n\n"
    return output

def parse_reflect_macro(line : str) -> Property:
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
            elif t=="setter":
                current_prop.is_setter = True
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



def parse_enum(enumname : str, file_iter : enumerate[str]) -> ClassDef:
    obj = ClassDef([enumname])
    obj.object_type = ENUM_OBJECT

    current_prop = None
    for _,line in file_iter:
        line : str = line.strip()
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

STANDARD_CPP_TYPES: dict[str, int] = {
    "void": NONE_TYPE,

    # std containers
    "std::string": STRING_TYPE,
    "string":STRING_TYPE,
    "std::vector": LIST_TYPE,
    "vector":LIST_TYPE,
    "std::unordered_set":UNORDERED_SET,
    "unordered_set":UNORDERED_SET,
    "std::unordered_map":UNORDERED_MAP,
    "unordered_map":UNORDERED_MAP,

    "MulticastDelegate": MULTICAST_TYPE,
    "AssetPtr": ASSETPTR_TYPE,
    "SoftAssetPtr": SOFTASSETPTR_TYPE,
    "float": FLOAT_TYPE,
    "bool": BOOL_TYPE,
    "int": INT_TYPE,
    "uint32_t": INT_TYPE,
    "int32_t": INT_TYPE,
    "int16_t": INT_TYPE,
    "uint16_t": INT_TYPE,
    "int64_t": INT_TYPE,
    "int8_t": INT_TYPE,
    "uint8_t": INT_TYPE,
    "EntityPtr": ENTITYPTR_TYPE,
    "glm::vec3": VEC3_TYPE,
    "glm::quat": QUAT_TYPE,
    "Color32": COLOR32_TYPE,
    "ClassTypeInfo":CLASSTYPEINFO_TYPE
}

def parse_class_decl(line:str) -> list[str]:
    line  = line.replace(",", " , ")
    line  = line.replace("{", " { ")
    line  = line.replace(":", " : ")
    tokens = line.split()
    assert(tokens[0]=="class")
    class_name = tokens[1]
    bases : list[str] = []
    if ":" in tokens:
        idx = tokens.index(":") + 1
        while idx < len(tokens):
            if tokens[idx] == "public":
                idx += 1
                if idx < len(tokens) and tokens[idx] not in [",", "{"]:
                    bases.append(tokens[idx])
                    idx += 1
            elif tokens[idx] == ",":
                idx += 1
            elif tokens[idx] == "{":
                break
            else:
                # Handles cases like "class myclass : xyz {"
                bases.append(tokens[idx])
                idx += 1
    return [class_name] + bases

def parse_class_from_start(line:str, file_iter:enumerate[str]) -> ClassDef|None:
    if remove_comment_from_end(line).strip().endswith(";"):  # to skip forward declares, hacky
        return None

    name_and_bases = parse_class_decl(line)
    c = ClassDef(name_and_bases)
    # now check its really a class, expects CLASS_BODY(name) on next 1-3 lines...
    good = False
    for i in range(0,3):
        _,line = next(file_iter)
        line = line.strip()
        if line.startswith(f"CLASS_BODY({name_and_bases[0]})"):
            good = True
            break
    if good:
        return c
    return None




def parse_type_from_tokens(idx: int, tokens:list[str], typenames: dict[str, int]) -> tuple[CppType, int]:
    const_local = False
    
    if tokens[idx] == "const":
        const_local = True
        idx += 1
    base = tokens[idx]
    idx += 1
    base_type = NONE_TYPE
    base_typename = ""
    #is_ptr = False

    if base in STANDARD_CPP_TYPES:
        base_type = STANDARD_CPP_TYPES[base]
    elif base in typenames:
        base_type = typenames[base]
        base_typename = base
    else:
        print(f"unknown type, assuming struct: {base}")
        base_typename = base
        base_type = STRUCT_TYPE
        #raise Exception(f"cant parse type: {base}")


    template_args : list[CppType]= []
    if idx < len(tokens) and tokens[idx] == "<":
        idx += 1
        while idx < len(tokens):
            if tokens[idx] == ">":
                idx += 1
                break
            arg, idx = parse_type_from_tokens(idx, tokens,typenames)
            template_args.append(arg)
            if idx < len(tokens) and tokens[idx] == ",":
                idx += 1
        # end of template args

    # skip pointer or reference
    while idx < len(tokens) and tokens[idx] in ("*", "&"):
        idx += 1

    return CppType(base_type, template_args, const_local, base_typename), idx

def parse_type(string: str, typenames: dict[str, int]) -> CppType:
    string = string.replace("<", " < ")
    string = string.replace(">", " > ")
    string = string.replace("*", " * ")
    string = string.replace(",", " , ")
    tokens = string.split()
    cpp_type, _ = parse_type_from_tokens(0,tokens,typenames)
    return cpp_type



def parse_function(cur_line: str, typenames: dict[str, int]) -> tuple[str, list[tuple[CppType, str]]]:
    # Example: "void MyFunc(int a, float b)"
    l_paren = cur_line.find("(")
    r_paren = cur_line.rfind(")")
    if l_paren == -1 or r_paren == -1:
        raise Exception("Malformed function declaration")
    before_paren = cur_line[:l_paren].strip()
    after_paren = cur_line[l_paren + 1:r_paren].strip()
    # Get function name and return type
    tokens = before_paren.split()
    if len(tokens) < 2:
        raise Exception("Cannot parse function signature")
    #return_type_str = " ".join(tokens[:-1])
    func_name : str = tokens[-1]
    # Parse arguments
    args : list[tuple[CppType, str]]= []
    if after_paren:
        arg_strs = [a.strip() for a in after_paren.split(",") if a.strip()]
        for arg in arg_strs:
            # Example: "int a"
            arg_tokens = arg.split()
            if len(arg_tokens) < 2:
                raise Exception(f"Cannot parse argument: {arg}")
            type_str = " ".join(arg_tokens[:-1])
            name_str = arg_tokens[-1]
            cpp_type = parse_type(type_str, typenames)
            args.append((cpp_type, name_str))
    return func_name, args

# handles both functions and variables
def parse_property(cur_line: str, file: enumerate[str], typenames: dict[str, int]) -> Property:
    assert(cur_line.startswith("REF") or cur_line.startswith("REFLECT"))

    prop = Property()
    # Handle REFLECT macro
    if cur_line.startswith("REFLECT"):
        prop = parse_reflect_macro(cur_line)
        _, cur_line = next(file)
    elif cur_line.startswith("REF"):
        cur_line = cur_line[cur_line.find("REF") + 3:]
    
    cur_line = remove_comment_from_end(cur_line)

    # Check for static
    tokens = cur_line.split()
    is_static = tokens[0] == "static"
    prop.is_static = is_static
    # Remove 'static' for further parsing
    if is_static:
        cur_line = " ".join(tokens[1:])
    # Determine if function or variable
    assignment_sign = cur_line.find("=")
    l_paren = cur_line.find("(")
    r_paren = cur_line.find(")")
    if l_paren != -1 and r_paren != -1 and (r_paren < assignment_sign or assignment_sign == -1):
        # Function
        func_name, args = parse_function(cur_line, typenames)
        prop.prop_type = FUNCTION_TYPE
        prop.name = func_name
        prop.func_args = args
        # Parse return type
        before_paren = cur_line[:l_paren].strip()
        tokens = before_paren.split()
        return_type_str = " ".join(tokens[:-1])
        prop.new_type = parse_type(return_type_str, typenames)
    else:
        # Variable
        # Remove trailing ';' or '=' and value
        if "=" in cur_line:
            cur_line = cur_line[:cur_line.find("=")].strip()
        if ";" in cur_line:
            cur_line = cur_line[:cur_line.find(";")].strip()
        tokens = cur_line.split()
        if len(tokens) < 2:
            raise Exception(f"Cannot parse variable declaration: {cur_line}")
        type_str = " ".join(tokens[:-1])
        var_name = tokens[-1]
        prop.name = var_name
        prop.new_type = parse_type(type_str, typenames)
        # Try to determine prop_type from type_str
        base_type = type_str.split("<")[0].strip()
        if base_type in STANDARD_CPP_TYPES:
            prop.prop_type = STANDARD_CPP_TYPES[base_type]
        elif base_type in typenames:
            prop.prop_type = typenames[base_type]
            prop.enum_or_struct_name = base_type
        else:
            prop.prop_type = NONE_TYPE
    return prop


def remove_comment_from_end(line:str)->str:
    find = line.find("//")
    if find != -1:
        return line[:find]
    return line


def parse_file(file_path:str, typenames:dict[str,int]):
    #print(f"File: {file_name}")
    classes :list[ClassDef]= []
    additional_includes : list[str] = []

    current_class : ClassDef|None = None

    with open(file_path,"r") as file:
        file_iter = enumerate(iter(file),start=1)
        cur_namespace = ""
        try:
            for line_num,orig_line in file_iter:
                line:str= orig_line.strip()
                if orig_line.startswith("class"): #because of inner classes, use orig_line...
                    if current_class != None:
                        classes.append(current_class)
                        current_class = None
                    current_class = parse_class_from_start(line,file_iter)  # valid if current class is none
                elif line.startswith("NEWENUM"):
                    start_p = line.find("(")
                    end_p = line.find(")")
                    comma = line.find(",")
                    if start_p==-1 or end_p==-1:
                        raise Exception("expected NEWENUM(<enumname>)")
                    enumname = line[start_p+1:comma].strip()
                    enumname = cur_namespace + enumname
                    print(f"FOUND ENUM: {enumname}")
                    if current_class != None:
                        classes.append(current_class)
                        current_class = None
                    
                    outclass = parse_enum(enumname,file_iter)
                    classes.append(outclass)
                elif line.startswith("REFLECT") or line.startswith("REF"):
                    if current_class == None:
                        raise Exception("REFLECT seen before NEWCLASS")
                    current_prop = parse_property(line, file_iter, typenames)
                    current_class.properties.append(current_prop)

                elif line.startswith("GENERATED_CLASS_INCLUDE"):
                    start_p = line.find("(")
                    end_p = line.find(")")
                    if start_p == -1 or end_p == -1:
                        raise Exception("expcted GENERATED_CLASS_INCLUDE(...)")
                    include_name : str = line[start_p+1:end_p].strip()
                    additional_includes.append(include_name)
                elif line.startswith("namespace"):
                    tokens = shlex.split(line)
                    assert(tokens[0]=="namespace")
                    cur_namespace += tokens[1] + "::"
        except Exception as e:
            what_line_num, what_line_str =    itertools.tee(file_iter)
            raise Exception("{}:{}:\"{}\"-{}".format(file_path,what_line_num,what_line_str,e.args[0]))
    if current_class != None:
        classes.append(current_class)
    return (classes,additional_includes)


def should_skip_this(path : str, skip_dirs:list[str]) -> bool:
    path = path.replace("\\","/")
    for d in skip_dirs:
        if path.startswith(d):
            return True
    return False

def read_typenames_from_text(filetext:TextIOWrapper) -> dict[str,int]:
    cur_namespace = ""
    typenames : dict[str,int] = {}
    file_iter = enumerate(filetext)
    for _,org_line in file_iter:
        line = org_line.strip()
        if line.startswith("NEWENUM"):
            start_p = line.find("(")
            end_p = line.find(")")
            comma = line.find(",")
            if start_p==-1 or end_p==-1:
                continue
            enumname = line[start_p+1:comma].strip()
            typenames[cur_namespace+enumname] = ENUM_TYPE
        elif line.startswith("NEWSTRUCT"):
            start_p = line.find("(")
            end_p = line.find(")")
            if start_p==-1 or end_p==-1:
                continue
            structname = line[start_p+1:end_p]
            typenames[cur_namespace+structname] = STRUCT_TYPE
        elif org_line.startswith("class"):
            c = parse_class_from_start(line,file_iter)
            if c != None:
                typenames[cur_namespace+c.classname] = STRUCT_TYPE
        elif line.startswith("namespace"):
            tokens = shlex.split(line)
            assert(tokens[0]=="namespace")
            if len(tokens) > 1:
                cur_namespace += tokens[1] + "::"
    return typenames


def read_typenames_from_files(skip_dirs:list[str]) -> dict[str,int]:
    typenames : dict[str,int] ={}
    for root, _, files in os.walk("."):
        if should_skip_this(root, skip_dirs):
            continue
        for file_name in files:
            if os.path.splitext(file_name)[-1] != ".h":
                continue
            with open(root+"/"+file_name,"r") as file:
                typenames_out = read_typenames_from_text(file)
                typenames = typenames | typenames_out
    return typenames

# read 

def get_source_files_to_build(path:str, skip_dirs:list[str], add_all_files:bool) -> list[tuple[str,str]]:
    source_files_to_build : list[tuple[str,str]] = []
    for root, _, files in os.walk(path):
        if should_skip_this(root, skip_dirs):
            continue
        print(root)
        for file_name in files:
            if os.path.splitext(file_name)[-1] != ".h":
                continue
            generated_path : str = "./.generated" + (root[1:] if root == "." else root[1:]) + "/"
            generated_path += os.path.splitext(file_name)[-2] + ".gen.cpp"
            generated_path.replace("\\","/")
            if not os.path.exists(generated_path) or add_all_files:
                source_files_to_build.append( (root,file_name) )
            else:
                if os.path.getmtime(root+"/"+file_name) > os.path.getmtime(generated_path):
                    source_files_to_build.append( (root,file_name) )
    return source_files_to_build

def clean_old_source_files(GENERATED_ROOT:str,delete_all_generated:bool):
    print("Cleaning .generated/...")
    for root,_,files in os.walk(GENERATED_ROOT):
        root = root.replace("\\","/")
        for file_name in files:
            input_generated = list(Path(root+"/"+file_name).parts)
            without_gen = input_generated[1:]
            last_part = input_generated[-1]
            without_gen[-1] = last_part[:last_part.find(".")]+".h"
            actual_path = "/".join(without_gen)
            
            if not os.path.exists(actual_path) or delete_all_generated:
                path_to_remove = (root + "/" + file_name).replace("\\","/")
                print(f"removing unused file: {path_to_remove}")
                assert(path_to_remove.find(".gen.")!=-1)
                os.remove(path_to_remove)

class ParseOutput:
    def __init__(self):
        self.classes : list[ClassDef] = []
        self.additional_includes : list[str] = []
        self.root : str = ""
        self.filename : str = ""

def parse_file_for_output(root:str,filename:str, typenames:dict[str,int]) -> ParseOutput|None:
    file_path : str= "{}/{}".format(root,filename)
    output = ParseOutput()
    output.classes,output.additional_includes = parse_file(file_path, typenames) 
    output.root = root
    output.filename = filename

    if len(output.classes) == 0:
        return None
    return output

def write_output_file(GENERATED_DIR:str,filename:str,root:str,classes:list[ClassDef],additional_includes:list[str]):
    generated_path = root + "/" + os.path.splitext(filename)[0]
    if generated_path[0]=='.':
        generated_path=generated_path[2:]
    generated_path = GENERATED_DIR+generated_path+".gen.cpp"
    generated_path.replace("\\","/")
    os.makedirs(os.path.dirname(generated_path), exist_ok=True)
    with open(generated_path,"w") as file:
        print(f"Writing {generated_path}")
        if len(classes)>0:
            file.write(write_headers(root+"/"+filename, additional_includes))
            for c in classes:
                file.write(write_class(c))

def do_codegen(path:str, skip_dirs:list[str], full_rebuild:bool):
    print("Starting codegen script...")
    start_time = time.perf_counter()
    typenames : dict[str,int] = read_typenames_from_files(skip_dirs)
    end_time = time.perf_counter()
    elapsed_time = (end_time - start_time) * 1000  # Convert to milliseconds
    print(f"read enums and structs in {elapsed_time:.2f} ms")

    source_files_to_build : list[tuple[str,str]] = get_source_files_to_build(path, skip_dirs, full_rebuild)

    GENERATED_ROOT = "./.generated/"
    print("Cleaning .generated/...")
    clean_old_source_files(GENERATED_ROOT, full_rebuild)

    output_files : list[ParseOutput] = []
    for (root,filename) in source_files_to_build:  
        output : ParseOutput|None = parse_file_for_output(root,filename,typenames)
        if output != None:
            output_files.append(output)
    for o in output_files:
        write_output_file(GENERATED_ROOT,o.filename,o.root,o.classes,o.additional_includes)

    end_time = time.perf_counter()
    elapsed_time = (end_time - start_time) * 1000  # Convert to milliseconds
    print(f"Finished in {elapsed_time:.2f} ms")

