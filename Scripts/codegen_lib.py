from io import TextIOWrapper
import os
import shlex
from pathlib import Path

import itertools
from textwrap import indent
from typing import Any, List, Optional


VERSION = 1

# where are enums reeee

NONE_TYPE = 0 # also stand in for void

INT_TYPE = 1
BOOL_TYPE = 2
FLOAT_TYPE = 3
ENUM_TYPE = 5

FUNCTION_TYPE = 9
MULTICAST_TYPE = 10

# containers
ARRAY_TYPE = 20
UNORDERED_SET = 21
UNORDERED_MAP = 22
STRING_TYPE = 23 # std::string

VEC2_TYPE = 30
VEC3_TYPE = 31
QUAT_TYPE = 32
COLOR32_TYPE = 33

STRUCT_TYPE = 40 # STRUCT_BODY() type
OLD_VOID_STRUCT_TYPE = 41 # old, get rid of this


# ptr types
ASSET_PTR_TYPE = 50  
SOFTASSET_PTR_TYPE = 51
HANDLE_PTR_TYPE = 52 # entities/components
UNIQUE_PTR_TYPE = 53 # std::uniqueptr to a class type
RAW_PTR_TYPE = 54

CLASSTYPEINFO_TYPE = 60
STRINGNAME_TYPE = 61

OTHER_CLASS_TYPE = 100


class CppType:
    def __init__(self,type_string_raw:str, typename: Any, type: int, templates: Optional[List["CppType"]] = None, const: bool = False, is_ptr : bool=False) -> None:
        self.type: int = type
        self.template_args: List["CppType"] = templates if templates is not None else []
        self.constant: bool = const
        self.typename: ClassDef|None = typename
        self.is_pointer : bool = is_ptr

        self.type_string_raw : str = type_string_raw

    def get_raw_type_string(self)->str:
        out : str = ""
        if self.constant:
            out += "const "
        out += self.type_string_raw
        if self.template_args:
            out += "<"
            for p in self.template_args:
                out += p.get_raw_type_string()
            out += ">"
        if self.is_pointer:
            out += "*"
        return out

class Property:
    def __init__(self):
        self.name = ""
        self.nameoverride = ""
        self.transient = False
        self.hide = False

        # or return type
        self.new_type : CppType
        self.func_args : list[tuple[CppType,str]] = []
        self.return_type : CppType

        self.is_static = False
        self.is_virtual = False

        self.tooltip = ""
        self.custom_type = ""
        self.list_struct = ""
        self.hint = ""
        self.is_getter = False
        self.is_setter = False

    def get_flags(self):
        if not self.transient and not self.hide:
            return "PROP_DEFAULT"
        if self.transient:
            return "PROP_EDITABLE"
        if self.hide:
            return "PROP_SERIALIZE"
        return "0"


class ClassDef:      
    TYPE_CLASS = 0
    TYPE_STRUCT = 1
    TYPE_ENUM = 2
    TYPE_INTERFACE = 3

    def __init__(self, name_and_bases : list[str], type:int, scriptable : bool):
        self.source_file : str = ""
        self.source_file_line : int = 0

        self.object_type : int = type
        self.classname = name_and_bases[0]
        self.supername : str = ""
        if len(name_and_bases) > 1:
            self.supername = name_and_bases[1]

        self.super_type_def : ClassDef|None = None
        self.interface_defs :list[ClassDef] = []

        self.interfaces_names : list[str] = []
        if len(name_and_bases) > 2:
            self.interfaces_names = name_and_bases[2:]
        self.properties : list[Property] = []

        self.tooltip = ""

        self.scriptable = scriptable

    def find_property_for_name(self, name:str) -> Property|None:
        for p in self.properties:
            if p.name==name:
                return p
        return None

    @staticmethod
    def fixup_types(typenames:dict[str,"ClassDef"]):
        for t in typenames.values():
            t.fixup_classdef_types(typenames)
    def fixup_classdef_types(self, typenames:dict[str,"ClassDef"]):
        assert(typenames[self.classname]!=None)
        if self.object_type!=ClassDef.TYPE_CLASS:
            return
        if len(self.supername) == 0:
            return
        self.super_type_def = typenames[self.supername]
        for t in self.interfaces_names:
            self.interface_defs.append(typenames[t])
    def is_self_derived_from(self,other:"ClassDef"):
        assert(other.object_type==ClassDef.TYPE_CLASS)
        assert(self.object_type==ClassDef.TYPE_CLASS)
        mysuper : ClassDef|None = self.super_type_def
        while mysuper != None:
            if mysuper == other:
                return True
            mysuper = mysuper.super_type_def
        return False

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

def parse_enum(enumname : str, file_iter : enumerate[str], typenames:dict[str,ClassDef]) -> ClassDef:
    obj = typenames[enumname]
    assert(obj.object_type==ClassDef.TYPE_ENUM)

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
    "std::vector": ARRAY_TYPE,
    "vector":ARRAY_TYPE,
    "std::unordered_set":UNORDERED_SET,
    "unordered_set":UNORDERED_SET,
    "std::unordered_map":UNORDERED_MAP,
    "unordered_map":UNORDERED_MAP,

    "MulticastDelegate": MULTICAST_TYPE,

    "AssetPtr": ASSET_PTR_TYPE,
    "SoftAssetPtr": SOFTASSET_PTR_TYPE,
    "obj": HANDLE_PTR_TYPE,
    "EntityPtr": HANDLE_PTR_TYPE,
    "uptr" : UNIQUE_PTR_TYPE,
    "unique_ptr" : UNIQUE_PTR_TYPE,
    "std::unique_ptr" : UNIQUE_PTR_TYPE,
    "ClassTypePtr" : CLASSTYPEINFO_TYPE,

    "StringName" : STRINGNAME_TYPE,

    "float": FLOAT_TYPE,
    "bool": BOOL_TYPE,
    "int": INT_TYPE,
    "uint32_t": INT_TYPE,
    "int32_t": INT_TYPE,
    "int16_t": INT_TYPE,
    "uint16_t": INT_TYPE,
    "short" : INT_TYPE,
    "int64_t": INT_TYPE,
    "uint64_t": INT_TYPE,
    "int8_t": INT_TYPE,
    "uint8_t": INT_TYPE,
    "char": INT_TYPE,


    "glm::vec3": VEC3_TYPE,
    "vec3": VEC3_TYPE,
    "glm::vec2": VEC2_TYPE,
    "vec2": VEC2_TYPE,
    "glm::quat": QUAT_TYPE,
    "quat": QUAT_TYPE,

    "Color32":COLOR32_TYPE,
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

def find_class_in_typenames(line:str,file_iter:enumerate[str], typenames:dict[str,ClassDef])->ClassDef|None:
    if remove_comment_from_end(line).strip().endswith(";"):  # to skip forward declares, hacky
        return None
    name_and_bases = parse_class_decl(line)
    classname = name_and_bases[0]
    if classname in typenames:
        assert(typenames[classname].object_type==ClassDef.TYPE_CLASS)
        return typenames[classname]
    return None

def find_struct_in_typenames(line:str,file_iter:enumerate[str], typenames:dict[str,ClassDef])->ClassDef|None:
    if remove_comment_from_end(line).strip().endswith(";"):  # to skip forward declares, hacky
        return None
    name = parse_struct_decl(line)
    if name in typenames:
        assert(typenames[name].object_type==ClassDef.TYPE_STRUCT)
        return typenames[name]
    return None


def parse_class_body_macro(line : str, inDef : ClassDef):
    start_p = line.find("(")
    end_p = line.rfind(")")
    if start_p == -1 or end_p == -1:
        raise Exception("expected CLASS_BODY(name,...)")
    argstr = line[start_p+1:end_p]
    argstr = argstr.replace(","," , ")
    argstr = argstr.replace("="," = ")
    tokens = shlex.split(argstr)
    if len(tokens) == 0:
        raise Exception("expectd classname in CLASS_BODY() for " + inDef.classname)
    if len(tokens)>=2: # classname, ',' , ...
        tokens = tokens[2:]
    else:
        return # no args
    token_iter = iter(tokens)

    try:
        for t in token_iter:
            if t == "scriptable":
                print(f"found scriptable tag for {inDef.classname}")
                inDef.scriptable = True
            else:
                raise Exception(f"unexpected CLASS_BODY(...) arg \"{t}\"")

            next_or_final = next(token_iter, None)
            if next_or_final == None:
                break
            if next_or_final!=",":
                raise Exception("expected comma between args")
    except StopIteration as e:
        raise Exception("unexpected CLASS_BODY() arg end")



def parse_class_from_start(line:str, file_iter:enumerate[str]) -> ClassDef|None:
    if remove_comment_from_end(line).strip().endswith(";"):  # to skip forward declares, hacky
        return None

    name_and_bases = parse_class_decl(line)
    c = ClassDef(name_and_bases, ClassDef.TYPE_CLASS, False)

    # now check its really a class, expects CLASS_BODY(name) on next 1-3 lines...
    good = False
    for i in range(0,3):
        _,line = next(file_iter)
        line = line.strip()
        if line.startswith(f"CLASS_BODY("):
            parse_class_body_macro(line, c)
            good = True
            break
    if good:
        return c
    return None

def parse_struct_decl(line : str) -> str:
    line  = line.replace(",", " , ")
    line  = line.replace("{", " { ")
    line  = line.replace(":", " : ")
    tokens = line.split()
    assert(tokens[0]=="struct")
    return tokens[1]

def parse_struct_from_start(line:str, file_iter:enumerate[str]) -> ClassDef|None:
    if remove_comment_from_end(line).strip().endswith(";"):  # to skip forward declares, hacky
        return None
    
    name_and_bases = parse_struct_decl(line)
    c = ClassDef([name_and_bases],ClassDef.TYPE_STRUCT, False)

    # now check its really a struct, expects struct_body() on next 1-3 lines...
    good = False
    try:
        for i in range(0,3):
            _,line = next(file_iter)
            line = line.strip()
            if line.startswith(f"STRUCT_BODY("):
                good = True
                break
        if good:
            return c
    except Exception as e:
        return None # stop iteration
    return None

    


def parse_type_from_tokens(idx: int, tokens:list[str], typenames: dict[str, ClassDef]) -> tuple[CppType, int]:
    const_local = False
    
    if tokens[idx] == "const":
        const_local = True
        idx += 1
    base = tokens[idx]
    idx += 1

    base_type : int = NONE_TYPE
    base_typename : ClassDef|None = None
    #is_ptr = False

    if base in STANDARD_CPP_TYPES:
        base_type = STANDARD_CPP_TYPES[base]
    elif base in typenames:
        thetype : ClassDef = typenames[base]
        base_typename = thetype
        if thetype.object_type == ClassDef.TYPE_ENUM:
            base_type = ENUM_TYPE
        elif thetype.object_type == ClassDef.TYPE_CLASS:
            base_type = OTHER_CLASS_TYPE
        elif thetype.object_type == ClassDef.TYPE_STRUCT:
            base_type = STRUCT_TYPE
    else:
        print(f"unknown type, assuming struct: {base}")
        base_type = OLD_VOID_STRUCT_TYPE
       # raise Exception(f"cant parse type: {base}")

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

    # hack
    if base_type== HANDLE_PTR_TYPE and base=="EntityPtr":
        assert(len(template_args)==0)
        template_args.append(CppType("Entity",typenames["Entity"],OTHER_CLASS_TYPE))

    # skip pointer or reference
    is_pointer = False
    while idx < len(tokens) and tokens[idx] in ("*"):
        if is_pointer == True:
            raise RuntimeError("double pointer, not allowed")
        is_pointer = True
        idx += 1
    is_reference = False
    while idx < len(tokens) and tokens[idx] in ("&"):
        if is_reference == True:
            raise RuntimeError("double pointer, not allowed")
        is_reference = True
        idx += 1

    if is_pointer and base_type==OTHER_CLASS_TYPE and (not base_typename is None) and ClassDef.is_self_derived_from(base_typename,typenames["IAsset"]):
        print(f"Found a raw IAsset derived pointer, setting to AssetPtr... ({base})")
        template_args.append(CppType(base,None,OTHER_CLASS_TYPE,None,False,False))
        base_type = ASSET_PTR_TYPE
        base_typename = None



    return CppType(base, base_typename,base_type, template_args, const_local, is_pointer), idx

def parse_type(string: str, typenames: dict[str, ClassDef]) -> CppType:
    string = string.replace("<", " < ")
    string = string.replace(">", " > ")
    string = string.replace("*", " * ")
    string = string.replace("&", " & ")

    string = string.replace(",", " , ")
    tokens = string.split()
    cpp_type, _ = parse_type_from_tokens(0,tokens,typenames)
    return cpp_type



def parse_function(cur_line: str, typenames: dict[str, ClassDef]) -> tuple[str, list[tuple[CppType, str]]]:
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
def parse_property(cur_line: str, file: enumerate[str], typenames: dict[str, ClassDef]) -> Property:
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
    is_virtual = tokens[0]=="virtual"
    prop.is_virtual = is_virtual
    if is_virtual:
        cur_line = " ".join(tokens[1:])
    
    # Determine if function or variable
    assignment_sign = cur_line.find("=")
    l_paren = cur_line.find("(")
    r_paren = cur_line.find(")")
    if l_paren != -1 and r_paren != -1 and (r_paren < assignment_sign or assignment_sign == -1):
        # Function
        func_name, args = parse_function(cur_line, typenames)
        prop.new_type = CppType("",None, FUNCTION_TYPE)
        prop.name = func_name
        prop.func_args = args
        # Parse return type
        before_paren = cur_line[:l_paren].strip()
        tokens = before_paren.split()
        return_type_str = " ".join(tokens[:-1])
        prop.return_type = parse_type(return_type_str, typenames)
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
    return prop


def remove_comment_from_end(line:str)->str:
    find = line.find("//")
    if find != -1:
        return line[:find]
    return line
def get_comment_from_end(line:str)->str:
    find = line.find("//")
    if find != -1:
        return line[find+2:]
    return ""

def combine_comments(list:list[str]) -> str:
    out : str = ""
    for s in list:
        stripped= s.strip()
        if len(stripped)>0:
            out += s.strip() + "\\n"
    return out

def parse_file(file_path:str, typenames:dict[str,ClassDef]):
    #print(f"File: {file_name}")
    classes :list[ClassDef]= []
    additional_includes : list[str] = []

    current_class : ClassDef|None = None

    current_comments : list[str] = []

    with open(file_path,"r") as file:
        file_iter = enumerate(iter(file),start=1)
        line_num = 0
        orig_line = ""
        try:
            for line_num,orig_line in file_iter:
                line:str= orig_line.strip()
                if orig_line.startswith("class"): #because of inner classes, use orig_line...
                    if current_class != None:
                        classes.append(current_class)
                        current_class = None
                    current_class = find_class_in_typenames(line,file_iter,typenames)  # valid if current class is none
                    if current_class != None:
                        current_class.tooltip = combine_comments(current_comments)
                        current_comments.clear()
                elif orig_line.startswith("struct"):
                    if current_class != None:
                        classes.append(current_class)
                        current_class = None
                    current_class = find_struct_in_typenames(line,file_iter,typenames)  # valid if current class is none
                    if current_class != None:
                        current_class.tooltip = combine_comments(current_comments)
                        current_comments.clear()
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
                    outclass = parse_enum(enumname,file_iter,typenames)
                    classes.append(outclass)
                    assert(current_class==None)

                elif line.startswith("REFLECT") or line.startswith("REF"):
                    if current_class == None:
                        raise Exception("REFLECT seen before NEWCLASS")
                    current_prop = parse_property(line, file_iter, typenames)
                    current_class.properties.append(current_prop)

                    current_prop.tooltip = combine_comments(current_comments + [get_comment_from_end(orig_line)])
                    current_comments.clear()

                elif line.startswith("GENERATED_CLASS_INCLUDE"):
                    start_p = line.find("(")
                    end_p = line.find(")")
                    if start_p == -1 or end_p == -1:
                        raise Exception("expcted GENERATED_CLASS_INCLUDE(...)")
                    include_name : str = line[start_p+1:end_p].strip()
                    additional_includes.append(include_name)
                elif line.startswith("//"):
                    current_comments.append(line[2:])
                else:
                    current_comments.clear()

        except Exception as e:
            raise Exception("{}:{}:\"{}\"-{}".format(file_path, line_num, orig_line.strip(), e.args[0]))
    if current_class != None:
        classes.append(current_class)
    return (classes,additional_includes)


def should_skip_this(path : str, skip_dirs:list[str]) -> bool:
    path = path.replace("\\","/")
    for d in skip_dirs:
        if path.startswith(d):
            return True
    return False

def read_typenames_from_text(filetext:TextIOWrapper, filepath : str) -> dict[str,ClassDef]:
    typenames : dict[str,ClassDef] = {}
    file_iter = enumerate(filetext)
    for line_num, org_line in file_iter:
        line = org_line.strip()
        if line.startswith("NEWENUM"):
            start_p = line.find("(")
            end_p = line.find(")")
            comma = line.find(",")
            if start_p==-1 or end_p==-1:
                continue
            enumname = line[start_p+1:comma].strip()            
            c = ClassDef([enumname],ClassDef.TYPE_ENUM, False)
            c.source_file = filepath
            c.source_file_line = line_num
            typenames[enumname] = c
        elif line.startswith("struct"):
            c = parse_struct_from_start(line,file_iter)
            if c != None:
                c.source_file = filepath
                c.source_file_line = line_num
                typenames[c.classname] = c

        elif org_line.startswith("class"):
            c = parse_class_from_start(line,file_iter)
            if c != None:
                c.source_file = filepath
                c.source_file_line = line_num
                typenames[c.classname] = c
    return typenames

def read_typenames_from_files(skip_dirs:list[str]) -> dict[str,ClassDef]:
    typenames : dict[str,ClassDef] ={}
    for root, _, files in os.walk("."):
        if should_skip_this(root, skip_dirs):
            continue
        for file_name in files:
            if os.path.splitext(file_name)[-1] != ".h":
                continue
            with open(root+"/"+file_name,"r") as file:
                typenames_out = read_typenames_from_text(file,file_name)
                typenames = typenames | typenames_out
    return typenames

# read 

def get_source_files_to_build(path:str, skip_dirs:list[str], add_all_files:bool, time_to_check:float) -> list[tuple[str,str]]:
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
            #if not os.path.exists(generated_path) or add_all_files:
            #    source_files_to_build.append( (root,file_name) )
            #else:
            if add_all_files or os.path.getmtime(root+"/"+file_name) > time_to_check:
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

def parse_file_for_output(root:str,filename:str, typenames:dict[str,ClassDef]) -> ParseOutput|None:
    file_path : str= "{}/{}".format(root,filename)
    output = ParseOutput()
    output.classes,output.additional_includes = parse_file(file_path, typenames) 
    output.root = root
    output.filename = filename

    if len(output.classes) == 0:
        return None
    return output


