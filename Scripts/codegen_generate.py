from numpy import typename
from codegen_lib import *

def write_headers(path:str, additional_includes:list[str]):
    out = f"// **** GENERATED SOURCE FILE version:{VERSION} ****\n"
    #out += f"#include \"{path}\"\n"
    out += "#include \"Framework/ReflectionProp.h\"\n"
    out += "#include \"Framework/ReflectionMacros.h\"\n"
    out += "#include \"Framework/PropertyMake.h\"\n"
    out += "#include \"Framework/EnumDefReflection.h\"\n"
    out += "#include \"Framework/InterfaceTypeInfo.h\"\n"
    out += "#include \"Scripting/ScriptFunctionCodegen.h\"\n"
    out += "#include \"Scripting/ScriptManager.h\"\n"
    for inc in additional_includes:
        out += f"#include {inc}\n"

    return out + "\n"


def write_if_changed(path: str, contents: str, label: str) -> None:
    """Avoid invalidating MSBuild outputs when reflection input is unchanged."""
    try:
        with open(path, "r", encoding="utf-8", newline="") as existing:
            if existing.read() == contents:
                return
    except FileNotFoundError:
        pass
    with open(path, "w", encoding="utf-8", newline="") as output:
        output.write(contents)
    print(f"Writing {label} {path}")


def escape_quote_characters(instr:str) -> str:
    outstr : str = ""
    for c in instr:
        if c== '"':
            outstr += "\\\""
        else:
            outstr += c
    return outstr

# make_property<> deduces the type infos from the member type (Framework/PropertyMake.h).
def write_prop(typenames:dict[str,ClassDef],prop : Property,newclass:ClassDef) -> str:
    custom_type = f'"{prop.custom_type}"'
    hint = f'"{prop.hint}"'
    tooltip = f'"{escape_quote_characters(prop.tooltip)}"'
    member = f"&{newclass.classname}::{prop.name}"
    offset = f"checked_offset<offsetof({newclass.classname}, {prop.name})>()"
    base = f'make_property<{member}, {offset}>("{prop.name}",{prop.get_flags()},{custom_type},{hint},{tooltip})'
    attrs = prop.emit_attrs_struct()
    if attrs:
        return f"apply_attrs({base},{attrs})"
    return base

def does_struct_have_serialize_function(newclass:ClassDef) -> bool:
    assert(newclass.object_type==ClassDef.TYPE_STRUCT)
    p = newclass.find_property_for_name("serialize")
    return p != None and p.new_type.type == FUNCTION_TYPE

def write_get_type_from_lua_func(newType:CppType, index:int) -> str:
    type_of_template = ""
    if len(newType.template_args)>0:
        type_of_template = newType.template_args[0].get_raw_type_string()

    if newType.type == BOOL_TYPE:
        return f"get_bool_from_lua(L,{index})"
    elif newType.type == INT_TYPE:
        return f"get_int_from_lua(L,{index})"
    elif newType.type == FLOAT_TYPE:
        return f"get_float_from_lua(L,{index})"
    elif newType.type == VEC3_TYPE:
        return f"get_lVec3_from_lua(L,{index})"
    elif newType.type == QUAT_TYPE:
        return f"get_lQuat_from_lua(L,{index})"
    elif newType.type == MAT4_TYPE:
        return f"get_mat4_from_lua(L,{index})"
    elif newType.type == STRING_TYPE:
        return f"get_std_string_from_lua(L,{index})"
    elif newType.type == ASSET_PTR_TYPE or newType.type == HANDLE_PTR_TYPE:
        return f"class_cast<{type_of_template}>(get_object_from_lua(L,{index}))"
    elif newType.type == OTHER_CLASS_TYPE:
        assert(newType.typename!=None)
        return f"class_cast<{newType.typename.classname}>(get_object_from_lua(L,{index}))"
    elif newType.type == ENUM_TYPE:
        assert(newType.typename!=None)
        return f"({newType.typename.classname})get_int_from_lua(L,{index})"
    elif newType.type == STRINGNAME_TYPE:
        return f"get_stringname_from_lua(L,{index})"
    elif newType.type == ARRAY_TYPE:
        assert(len(newType.template_args)==1)
        type_of_template = newType.template_args[0].get_raw_type_string()
        return f"get_std_vector_from_lua<{type_of_template}>(L,{index},[&]() -> {type_of_template} {{  return {write_get_type_from_lua_func(newType.template_args[0],-1)}; }})"
    elif newType.type == STRUCT_TYPE:
        assert(newType.typename!=None)
        return f"get_{newType.typename.classname}_from_lua(L,{index})"
    elif newType.type == OPTIONAL_TYPE:
        assert(len(newType.template_args)==1)
        type_of_template = newType.template_args[0].get_raw_type_string()
        return f"get_std_optional_from_lua<{type_of_template}>(L,{index},[&]() -> {type_of_template} {{ return {write_get_type_from_lua_func(newType.template_args[0],index)}; }})"
    elif newType.type == PAIR_TYPE:
        raise Exception("std::pair is only supported as a direct function return type (multiple lua return values), not as a parameter/field/nested type")
    else:
        raise Exception(f"cant get type from lua: {newType.get_raw_type_string()}")

def write_push_type_to_lua_func(newType:CppType, cppVarName:str) -> str:
    #type_of_template = ""
   # if len(newType.template_args)>0:
     #   type_of_template = newType.template_args[0].get_raw_type_string()
    if newType.type == BOOL_TYPE:
        return f"push_bool_to_lua(L,{cppVarName})"
    elif newType.type == INT_TYPE:
        return f"push_int_to_lua(L,{cppVarName})"
    elif newType.type == FLOAT_TYPE:
        return f"push_float_to_lua(L,{cppVarName})"
    elif newType.type == VEC3_TYPE:
        return f"push_lVec3_to_lua(L,{cppVarName})"
    elif newType.type == QUAT_TYPE:
        return f"push_lQuat_to_lua(L,{cppVarName})"
    elif newType.type == MAT4_TYPE:
        return f"push_mat4_to_lua(L,{cppVarName})"
    elif newType.type == STRING_TYPE:
        return f"push_std_string_to_lua(L,{cppVarName})"
    elif newType.type == ASSET_PTR_TYPE or newType.type == HANDLE_PTR_TYPE:
        return f"push_object_to_lua(L,{cppVarName})"
    elif newType.type == OTHER_CLASS_TYPE:
        assert(newType.typename!=None)
        return f"push_object_to_lua(L,{cppVarName})"
    elif newType.type == ENUM_TYPE:
        return f"push_int_to_lua(L,(int64_t){cppVarName})"
    elif newType.type == STRINGNAME_TYPE:
        return f"push_stringname_to_lua(L,{cppVarName})"
    elif newType.type == ARRAY_TYPE:
        assert(len(newType.template_args)==1)
        type_of_template = newType.template_args[0].get_raw_type_string()
        return f"push_std_vector_to_lua(L,{cppVarName},[&]({type_of_template} val) {{  {write_push_type_to_lua_func(newType.template_args[0],'val')}; }})"
    elif newType.type == STRUCT_TYPE:
        assert(newType.typename!=None)
        return f"push_{newType.typename.classname}_to_lua(L,{cppVarName})"
    elif newType.type == OPTIONAL_TYPE:
        assert(len(newType.template_args)==1)
        type_of_template = newType.template_args[0].get_raw_type_string()
        return f"push_std_optional_to_lua(L,{cppVarName},[&]({type_of_template} val) {{  {write_push_type_to_lua_func(newType.template_args[0],'val')}; }})"
    elif newType.type == PAIR_TYPE:
        raise Exception("std::pair is only supported as a direct function return type (multiple lua return values), not as a parameter/field/nested type")
    else:
        raise Exception(f"cant push type to lua: {newType.get_raw_type_string()} {cppVarName}")
    return ""

def write_script_function(newclass:ClassDef, funcProp : Property) -> str:
    assert(funcProp.new_type.type==FUNCTION_TYPE)
    my_obj_type = newclass.classname
    function_name = funcProp.name

    output = f"int lua_binding_{newclass.classname}_{funcProp.name}(lua_State* L)\n"
    output += "{\n"
    # get class object (or struct)
    calling_template = ""
    if not funcProp.is_static:
        output += "\tClassBase* obj = get_object_from_lua(L,1);\n"
        if newclass.object_type == ClassDef.TYPE_INTERFACE:
            output += f"\t{my_obj_type}* myObj = obj ? obj->cast_interface<{my_obj_type}>() : nullptr;\n"
        else:
            output += f"\t{my_obj_type}* myObj = obj ? obj->cast_to<{my_obj_type}>() : nullptr;\n"
        # assert it
        output += f'\tif(!myObj) {{  luaL_error(L,"null dereference for object calling {newclass.classname}::{funcProp.name}");  }}\n'# fixme
        # call into function
        calling_template = f"myObj->"
    else:
        calling_template = f"{my_obj_type}::"
    if funcProp.return_type.type!=NONE_TYPE:
        output += f"\tauto return_value = {calling_template}{function_name}(\n"
    else:
        output += f"\t{calling_template}{function_name}(\n"

    # get args
    argIndex = 1
    for argType, argName in funcProp.func_args:
        luaIndex = argIndex
        if not funcProp.is_static:
            luaIndex = argIndex + 1
        output += "\t\t" + write_get_type_from_lua_func(argType, luaIndex)
        if argIndex != len(funcProp.func_args):
            output += ","
        output += f" // {argName}\n"
        argIndex += 1
    output += "\t);\n"

    if funcProp.return_type.type == NONE_TYPE:
        output += "\treturn 0;\n"
    elif funcProp.return_type.type == PAIR_TYPE:
        assert(len(funcProp.return_type.template_args)==2)
        output += "\t" + write_push_type_to_lua_func(funcProp.return_type.template_args[0],"return_value.first") + ";\n"
        output += "\t" + write_push_type_to_lua_func(funcProp.return_type.template_args[1],"return_value.second") + ";\n"
        output += "\treturn 2;\n"
    else:
        output += "\t" + write_push_type_to_lua_func(funcProp.return_type,"return_value") + ";\n"
        output += "\treturn 1;\n"

    # return if it has return value
    output += "}\n"
    return output

def write_class_script_functions(newclass:ClassDef) -> str:
    output = ""
    for p in newclass.properties:
        if p.new_type.type == FUNCTION_TYPE:
            output += write_script_function(newclass,p)
    return output

def class_has_actual_properties(newclass : ClassDef) -> bool:
    for p in newclass.properties:
        if p.new_type.type != FUNCTION_TYPE and p.new_type.type != MULTICAST_TYPE:
            return True
    return False

def append_to_array_R(arr:list[Property],classdef:ClassDef):
    for f in classdef.properties:
        if f.new_type.type==FUNCTION_TYPE and f.is_virtual:
            arr.append(f)
    if classdef.super_type_def is not None:
        append_to_array_R(arr,classdef.super_type_def)
    for idef in classdef.interface_defs:
        for f in idef.properties:
            if f.new_type.type==FUNCTION_TYPE and f.is_virtual:
                if not any(existing.name == f.name for existing in arr):
                    arr.append(f)

# ls_slots: method -> LuaSharp slot call (codegen_luasharp.slot_call), or None when LuaSharp does not bind the class.
# LuaSharp objects set ScriptImpl::ls_slots; the Lua path runs only when it is null.
def write_scriptable_class(newclass : ClassDef, ls_slots:dict[str,str] | None = None) -> str:
    assert(newclass.scriptable)
    output = ""
    output += f"class ScriptImpl_{newclass.classname} : public {newclass.classname} " + " {\n"
    output += "public:\n"
    output += f"""
    const ClassTypeInfo* type = nullptr;
    const ClassTypeInfo& get_type() const final {{ return *type; }}
    """
    if ls_slots is not None:
        output += "void* const* ls_slots = nullptr; // LuaSharp overrides (LuaSharpEngine::create)\n"

    virtual_function_types : list[Property] = []
    append_to_array_R(virtual_function_types, newclass)


    for f in virtual_function_types:
            output += "\t" + f.return_type.get_raw_type_string() + f" {f.name}("
            for argType,argName in f.func_args:
                output += argType.get_raw_type_string() + " " + argName
                output += ", "
            if len(f.func_args)>0:
                output = output[:-2]
            output += ") final {"
            if ls_slots is not None:
                base_args = ", ".join(argName for _, argName in f.func_args)
                output += f"\n        if (ls_slots) {{ {ls_slots.get(f.name, '')} return {newclass.classname}::{f.name}({base_args}); }}"
            # get my table
            # find function
            # push args
            # get return value
            output+= f"""
        lua_State* L = ScriptManager::inst->get_lua_state();
        int myTable = get_table_registry_id();
        int top = lua_gettop(L);  // save stack
        lua_rawgeti(L, LUA_REGISTRYINDEX, myTable);
        lua_pushstring(L, \"{f.name}\");
        lua_rawget(L, -2); // use raw get to not look in __index
        bool is_func = lua_isfunction(L, -1);
        if(is_func) {{
            lua_pushvalue(L, -2);  // duplicate object table
            """
            for argType,argName in f.func_args:
                output += "\t\t\t"+write_push_type_to_lua_func(argType,argName) + ";\n"
            return_num = 0
            if f.return_type.type==PAIR_TYPE:
                assert(len(f.return_type.template_args)==2)
                return_num = 2
            elif f.return_type.type!=NONE_TYPE:
                return_num = 1
            arg_count = len(f.func_args)+1 # include self parameter
            
            error_str = f"\"During call {newclass.classname}::{f.name}\" + std::string(\"(luatype=\")+type->classname+\"):\""
            output+= f"""
            if (safe_pcall(L, {arg_count}, {return_num}) != LUA_OK) {{
                const char* error = lua_tostring(L, -1);
                lua_pop(L, 1);  // pop error
                lua_settop(L, top); // restore stack!
                throw LuaRuntimeError({error_str} + error);
            }}
            else {{\n"""
            if return_num == 2:
                t1 = f.return_type.template_args[0]
                t2 = f.return_type.template_args[1]
                output += f"\t\t\tauto value_first =  {write_get_type_from_lua_func(t1,-2)};\n"
                output += f"\t\t\tauto value_second =  {write_get_type_from_lua_func(t2,-1)};\n"
                output += f"\t\t\tlua_settop(L,top); // stack restore\n"
                output += f"\t\t\treturn std::make_pair(value_first, value_second);\n"

            elif return_num == 1:

                output += f"\t\t\tauto value =  {write_get_type_from_lua_func(f.return_type,-1)};\n"
                output += f"\t\t\tlua_settop(L,top); // stack restore\n"
                output += f"\t\t\treturn value;\n"

            else:
                output += f"\t\t\tlua_settop(L,top); // stack restore\n"

            output += "\t\t\t}\n"
            output += "\t\t}\n\t\telse{\n"
            output += "\t\t\tlua_pop(L,1);\n"
            output += "\t\t\tlua_settop(L,top); // stack restore\n"
            output += f"\t\t\treturn {newclass.classname}::{f.name}("
            for argType,argName in f.func_args:
                output += argName
                output += ", "
            if len(f.func_args)>0:
                output = output[:-2]
            output += ");\n\t\t}\n"

            output += "\t}\n"

    output += "};\n"
    return output

def write_struct_getter_setter(newclass : ClassDef) -> str:
    # Generates two functions:
    # 1. push_struct_to_lua: pushes a C++ struct as a Lua table
    # 2. get_struct_from_lua: reads a Lua table and fills a C++ struct

    output = ""

    # Function to push struct to Lua
    output += f"void push_{newclass.classname}_to_lua(lua_State* L, const {newclass.classname}& value) {{\n"
    output += "\tlua_newtable(L);\n"
    for p in newclass.properties:
        if p.new_type.type != FUNCTION_TYPE and p.new_type.type != MULTICAST_TYPE:
            try:
                toappend = ""
                toappend += f"\tlua_pushstring(L, \"{p.name}\");\n"
                toappend += f"\t{write_push_type_to_lua_func(p.new_type, f'value.{p.name}')};\n"
                toappend += "\tlua_settable(L, -3);\n"
                output += toappend
            except Exception as e :
                print(f"Err: write_struct_getter_setter{newclass.classname}: " + str(e.args))

    # lVec3/lQuat get a metatable so their tables support operators/methods
    # (see Source/Scripting/LuaVecQuat.cpp) -- registered under "lVec3_mt"/"lQuat_mt".
    # Every other struct stays a plain metatable-less table.
    if newclass.classname in ("lVec3", "lQuat"):
        output += f"\tluaL_getmetatable(L, \"{newclass.classname}_mt\");\n"
        output += "\tlua_setmetatable(L, -2);\n"

    output += "}\n\n"

    # Function to get struct from Lua
    output += f"{newclass.classname} get_{newclass.classname}_from_lua(lua_State* L, int index) {{\n"
    output += f"\t{newclass.classname} result;\n"
    for p in newclass.properties:
        if p.new_type.type != FUNCTION_TYPE and p.new_type.type != MULTICAST_TYPE:
            try:
                toappend = ""
                toappend += f"\tlua_getfield(L, index, \"{p.name}\");\n"
                toappend += f"\tif (!lua_isnil(L, -1)) {{\n"
                toappend += f"\t\tresult.{p.name} = {write_get_type_from_lua_func(p.new_type, -1)};\n"
                toappend += f"\t}}\n"
                toappend += "\tlua_pop(L, 1);\n"
                output += toappend
            except Exception as e :
                print(f"Err: write_struct_getter_setter{newclass.classname}: " + str(e.args))
    output += "\treturn result;\n"
    output += "}\n\n"

    return output

def has_any_functions(newclass:ClassDef)->bool:
    for f in newclass.properties:
        if f.new_type.type==FUNCTION_TYPE:
            return True
    for idef in newclass.interface_defs:
        for f in idef.properties:
            if f.new_type.type==FUNCTION_TYPE:
                return True
    return False
def get_bool_str(b:bool)->str:
    if b:
        return "true"
    else:
        return "false"
def write_class_old(typenames:dict[str,ClassDef],newclass : ClassDef, script_slots:dict[str,str] | None = None)->str:


    super_typeinfo_str = "nullptr"
    if newclass.super_type_def!=None: # handles case for root ClassBase
        super_typeinfo_str = f"&{newclass.supername}::StaticType"

    output = ""
    if newclass.object_type==ClassDef.TYPE_CLASS:

        output += write_class_script_functions(newclass)
        function_str = "nullptr,0"
        if has_any_functions(newclass):
            output += f"FunctionInfo {newclass.classname}_function_list[] = {{"
            count = 0
            seen_names : set[str] = set()
            for f in newclass.properties:
                if f.new_type.type==FUNCTION_TYPE:
                    output += f"{{ \"{f.name}\",{get_bool_str(f.is_static)},{get_bool_str(f.is_virtual)}, lua_binding_{newclass.classname}_{f.name}  }},\n"
                    seen_names.add(f.name)
                    count += 1
            for idef in newclass.interface_defs:
                for f in idef.properties:
                    if f.new_type.type==FUNCTION_TYPE and f.name not in seen_names:
                        output += f"{{ \"{f.name}\",false,{get_bool_str(f.is_virtual)}, lua_binding_{idef.classname}_{f.name}  }},\n"
                        seen_names.add(f.name)
                        count += 1
            output = output[:-2]
            output += "};\n"
            function_str = f"{newclass.classname}_function_list,{count}"


        scriptable_alloc = "nullptr"
        if newclass.scriptable:
            output += write_scriptable_class(newclass, script_slots)
            scriptable_alloc = f"get_allocate_script_impl_internal<ScriptImpl_{newclass.classname}>()"

        spawnable_str = "true" if newclass.editor_spawnable else "false"
        output += f"ClassTypeInfo {newclass.classname}::StaticType = ClassTypeInfo(\n \
                    \"{newclass.classname}\",\n \
                    {super_typeinfo_str},\n \
                    {newclass.classname}::get_props,\n \
                    default_class_create<{newclass.classname}>(),\n \
                    {newclass.classname}::CreateDefaultObject,{function_str},{scriptable_alloc},false,{spawnable_str});\n"
        for idef in newclass.interface_defs:
            output += f"static struct Register_{newclass.classname}_{idef.classname} {{\n"
            output += f"\tRegister_{newclass.classname}_{idef.classname}() {{\n"
            output += f"\t\tint32_t offset = (int32_t)((char*)({idef.classname}*)(({newclass.classname}*)0x100) - (char*)(ClassBase*)(({newclass.classname}*)0x100));\n"
            output += f"\t\t{newclass.classname}::StaticType.add_interface({idef.classname}::StaticInterfaceType.id, offset);\n"
            output += f"\t}}\n"
            output += f"}} s_reg_{newclass.classname}_{idef.classname};\n"
    elif newclass.object_type == ClassDef.TYPE_STRUCT:
        # write a lua getter/pusher
        output += write_struct_getter_setter(newclass)

        has_serialize = does_struct_have_serialize_function(newclass)
        if has_serialize:
            output += f"static void {newclass.classname}_serialize_private(void* p, Serializer& s)\n"
            output += "{\n"
            output += f"\t(({newclass.classname}*)p)->serialize(s);\n"
            output += "}\n"

        output += f"StructTypeInfo {newclass.classname}::StructType = StructTypeInfo(\n \
                    \"{newclass.classname}\",\n \
                    {newclass.classname}::get_props(),\n"
        if has_serialize:
            output += f"{newclass.classname}_serialize_private\n"
        else:
            output += "nullptr\n"
        output += "\t);\n"


    if newclass.object_type == ClassDef.TYPE_INTERFACE:
        output += write_class_script_functions(newclass)
        output += f'InterfaceTypeInfo {newclass.classname}::StaticInterfaceType("{newclass.classname}");\n'
        return output

    if newclass.object_type == ClassDef.TYPE_CLASS or newclass.object_type == ClassDef.TYPE_STRUCT:

#lua_binding_{newclass.classname}_{funcProp.name}
        output += f"const PropertyInfoList* {newclass.classname}::get_props()\n"
        output += "{\n"
        if class_has_actual_properties(newclass):


            output += f"    START_PROPS({newclass.classname})"
            for p in newclass.properties:
                if p.new_type.type != FUNCTION_TYPE and p.new_type.type != MULTICAST_TYPE:
                    output += "\n\t\t" + write_prop(typenames,p,newclass) + ","
            output=output[:-1]
            output += f"\n    END_PROPS({newclass.classname})\n"
        else:
            output += "\treturn nullptr;\n"
        output += "}\n\n"
    elif newclass.object_type == ClassDef.TYPE_ENUM:
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

# Vec3/Quat are the metatable-bearing tables scripts should write going forward
# (see Source/Scripting/LuaVecQuat.cpp); lVec3/lQuat are the legacy plain-table
# shape, still readable everywhere but being phased out. Return types/fields are
# always annotated as the new name since every push now attaches the metatable;
# params additionally accept the deprecated plain-table form via for_param.
def _vec3_or_quat_type_name(classname:str, for_param:bool) -> str|None:
    if classname == "lVec3":
        return "Vec3|lVec3" if for_param else "Vec3"
    if classname == "lQuat":
        return "Quat|lQuat" if for_param else "Quat"
    return None

def get_lua_type_string(new_type:CppType, no_nil:bool=False, for_param:bool=False) -> str:
    output = ""
    nil_suffix = "" if no_nil else "|nil"
    type_of_template :str= ""
    if len(new_type.template_args)>0:
        type_of_template = new_type.template_args[0].get_raw_type_string()
    if new_type.type == BOOL_TYPE:
        output += "boolean"
    elif new_type.type == INT_TYPE:
        output += "integer"
    elif new_type.type==FLOAT_TYPE:
        output += "number"
    elif new_type.type == ENUM_TYPE:
        enumtype = new_type.typename
        assert(enumtype!=None)
        output += f"integer"
    elif new_type.type == STRUCT_TYPE:
        assert(new_type.typename!=None)
        vec_or_quat = _vec3_or_quat_type_name(new_type.typename.classname, for_param)
        output += vec_or_quat if vec_or_quat != None else new_type.typename.classname
    elif new_type.type == ASSET_PTR_TYPE or new_type.type == HANDLE_PTR_TYPE:
        output += f"{type_of_template}{nil_suffix}"
    elif new_type.type == STRING_TYPE:
        output += f"string"
    elif new_type.type == STRINGNAME_TYPE:
        output += f"string|integer"
    elif new_type.type == OTHER_CLASS_TYPE:
        assert(new_type.typename!=None)
        output += f"{new_type.typename.classname}{nil_suffix}"
    elif new_type.type == VEC3_TYPE:
        output += "Vec3|lVec3" if for_param else "Vec3"
    elif new_type.type == QUAT_TYPE:
        output += "Quat|lQuat" if for_param else "Quat"
    elif new_type.type == MAT4_TYPE:
        output += "Transform"
    elif new_type.type == ARRAY_TYPE:
        assert(len(new_type.template_args)==1)
        output += get_lua_type_string(new_type.template_args[0], no_nil)+"[]"
    elif new_type.type == OPTIONAL_TYPE:
        assert(len(new_type.template_args)==1)
        output += get_lua_type_string(new_type.template_args[0], no_nil=True, for_param=for_param)+"|nil"
    else:
        output += "any"
    return output

def get_lua_return_annotation(return_type:CppType, no_nil:bool) -> str:
    """Emits the type list for a '---@return' line -- comma-separated for pair<T1,T2> (2 lua return values)."""
    if return_type.type == PAIR_TYPE:
        assert(len(return_type.template_args)==2)
        t1 = get_lua_type_string(return_type.template_args[0], no_nil)
        t2 = get_lua_type_string(return_type.template_args[1], no_nil)
        return f"{t1}, {t2}"
    return get_lua_type_string(return_type, no_nil)

def write_lua_class(newclass:ClassDef) -> str:
    output = ""

    if newclass.object_type == ClassDef.TYPE_INTERFACE:
        output += "---@class " + newclass.classname + "\n"
        output += newclass.classname + " = {}\n"
        output += newclass.classname + "Id = 0\n"
        for p in newclass.properties:
            if p.new_type.type == FUNCTION_TYPE:
                if p.return_type.type != NONE_TYPE:
                    output += "---@return " + get_lua_return_annotation(p.return_type, p.no_nil) + "\n"
                for argType, argName in p.func_args:
                    output += "---@param " + argName + " " + get_lua_type_string(argType, p.no_nil, for_param=True) + "\n"
                output += f"function {newclass.classname}:{p.name}("
                for _, argName in p.func_args:
                    output += argName + ","
                if len(p.func_args) > 0:
                    output = output[:-1]
                output += ") end\n"
        return output

    if newclass.object_type == ClassDef.TYPE_ENUM:
        index = 0
        for p in newclass.properties:
            output += (newclass.classname + "_" + p.name).upper()
            output += " = " + str(index) + "\n"
            index += 1
    elif newclass.object_type == ClassDef.TYPE_CLASS or newclass.object_type == ClassDef.TYPE_STRUCT:
        output += "---@class " + newclass.classname
        if newclass.object_type==ClassDef.TYPE_CLASS and newclass.super_type_def != None:
            output += " : " + newclass.super_type_def.classname
            for idef in newclass.interface_defs:
                output += ", " + idef.classname
        output += "\n"
        for p in newclass.properties:
            if p.new_type.type != FUNCTION_TYPE:
                output += f"---@field {p.name} "
                output += get_lua_type_string(p.new_type, p.no_nil)
                output += "\n"
        output +=  newclass.classname + " = {\n}\n"

        for p in newclass.properties:
            if p.new_type.type == FUNCTION_TYPE:
                if p.lua_generic and p.return_type.type != NONE_TYPE:
                    output += f"---@generic T : {get_lua_type_string(p.return_type, p.no_nil)}\n"
                if p.return_type.type != NONE_TYPE:
                    if p.lua_generic:
                        output += "---@return T\n"
                    else:
                        output += "---@return " + get_lua_return_annotation(p.return_type, p.no_nil) + "\n"
                for argType,argName in p.func_args:
                    if p.lua_generic and argType.type == OTHER_CLASS_TYPE and argType.typename != None and argType.typename.classname == "ClassTypeInfo":
                        output += f"---@param {argName} T\n"
                    else:
                        output += "---@param " + argName + " " + get_lua_type_string(argType, p.no_nil, for_param=True) + "\n"
                output += f"function {newclass.classname}"
                if p.is_static:
                    output += "."
                else:
                    output += ":"
                output += f"{p.name}("
                for _, argName in p.func_args:
                    output += argName + ","
                if len(p.func_args) > 0:
                    output = output[:-1]
                output += ") end\n"
            elif p.new_type.type == MULTICAST_TYPE:
                # write multicast delegate setters/getters
                # bind_{delegatename}
                # bind
                output += "---@param func function\n"
                output += f"function {newclass.classname}:bind_{p.name}(func) end\n"
                # remove
                output += "---@param func function\n"
                output += f"function {newclass.classname}:remove_{p.name}(func) end\n"
                # invoker
                count = 0
                for t in p.new_type.template_args:
                    output += f"---@param arg{count} {get_lua_type_string(t, p.no_nil, for_param=True)}\n"
                    count += 1
                output += f"function {newclass.classname}:invoke_{p.name}("
                for i in range(count):
                    output += f"arg{i}"
                    if i != count-1:
                        output += ","
                output += ") end\n"
            #else: # property type
            #    if p.script_readable:
            #        output += f"---@return {get_lua_type_string(p.new_type)}\n"
            #        output += f"function {newclass.classname}:get_{p.name}() end\n"
            #    if p.script_writable:
            #        output += f"---@param arg {get_lua_type_string(p.new_type)}\n"
            #        output += f"function {newclass.classname}:set_{p.name}(arg) end\n"


    return output
  

# script_slots: the LuaSharp slot calls of each scriptable class (codegen_luasharp.Output.slots).
def write_output_file(LUA_GEN_DIR :str, GENERATED_DIR:str,filename:str,root:str,classes:list[ClassDef],additional_includes:list[str], typenames:dict[str,ClassDef], lua_stub_filename:str = "lua_stubs.lua", extra_source:str = "", script_slots:dict[str,dict[str,str]] | None = None):
    generated_path = root + "/" + os.path.splitext(filename)[0]
    if generated_path[0]=='.':
        generated_path=generated_path[2:]
    generated_path = GENERATED_DIR+generated_path+".gen.cpp"
    generated_path.replace("\\","/")
    os.makedirs(os.path.dirname(generated_path), exist_ok=True)
    generated = ""
    if len(classes)>0:
        generated += write_headers(root+"/"+filename, additional_includes)
        # lVec3/lQuat are defined (and their push/get_..._from_lua bodies generated) only by
        # whichever pass scans the file that declares them - but any split-out project's own
        # reflected methods can still take them as Lua-bound parameters. Forward-declare both
        # unconditionally so every pass's generated file compiles regardless of which pass
        # owns the definition; redeclaring a function prototype twice (including here, in the
        # owning pass) is harmless.
        output = "void push_lVec3_to_lua(lua_State* L, const lVec3& value);\n"
        output += "lVec3 get_lVec3_from_lua(lua_State* L, int index);\n"
        output += "void push_lQuat_to_lua(lua_State* L, const lQuat& value);\n"
        output += "lQuat get_lQuat_from_lua(lua_State* L, int index);\n"
        # write struct stubs first
        for c in classes:
            if c.object_type == ClassDef.TYPE_STRUCT and c.classname not in ("lVec3", "lQuat"):
                output += f"void push_{c.classname}_to_lua(lua_State* L, const {c.classname}& value);\n"
                output += f"{c.classname} get_{c.classname}_from_lua(lua_State* L, int index);\n"
        generated += output

        interfaces = [c for c in classes if c.object_type == ClassDef.TYPE_INTERFACE]
        non_interfaces = [c for c in classes if c.object_type != ClassDef.TYPE_INTERFACE]
        slots = script_slots or {}
        for c in interfaces:
            generated += write_class_old(typenames,c)
        for c in non_interfaces:
            generated += write_class_old(typenames,c,slots.get(c.classname))
        generated += extra_source

    write_if_changed(generated_path, generated, "generated source")


    lua_stub_path = LUA_GEN_DIR + "/" + lua_stub_filename
    lua_stubs = f"--- GENERATED LUA FILE FROM C++ CLASSES v{VERSION}\n"
    lua_stubs += "---@diagnostic disable\n"
    if len(classes)>0:
        for c in classes:
            lua_stubs += write_lua_class(c)
    write_if_changed(lua_stub_path, lua_stubs, "lua stubs")
