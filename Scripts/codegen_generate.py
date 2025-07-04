from numpy import typename
from codegen_lib import *

from datetime import datetime


def write_headers(path:str, additional_includes:list[str]):

    now = datetime.now()
    timestamp_str = now.strftime("%Y-%m-%d %H:%M:%S")

    out = f"// **** GENERATED SOURCE FILE version:{VERSION} {timestamp_str} ****\n"
    out += f"#include \"{path}\"\n"
    out += "#include \"Framework/ReflectionProp.h\"\n"
    out += "#include \"Framework/ReflectionMacros.h\"\n"
    out += "#include \"Scripting/FunctionReflection.h\"\n"
    out += "#include \"Framework/VectorReflect2.h\"\n"
    out += "#include \"Framework/EnumDefReflection.h\"\n"
    out += "#include \"Scripting/ScriptFunctionCodegen.h\"\n"
    out += "#include \"Scripting/ScriptManager.h\"\n"
    for inc in additional_includes:
        out += f"#include {inc}\n"

    return out + "\n"


def output_macro_for_prop(typenames : dict[str,ClassDef],cpptype:CppType,name:str,flags:str,offset:str,custom_type:str,hint:str,nameoverride:str,tooltip:str)->str:
    name_with_quotes = f"\"{name}\""
    name_offset_flags = f"{name_with_quotes},{offset},{flags}"
    name_offset_flags_hint = f"{name_offset_flags},{hint}"
    type = cpptype.type
    raw_str = cpptype.get_raw_type_string()

    type_of_template :str= ""
    if len(cpptype.template_args)>0:
        type_of_template = f"{cpptype.template_args[0].get_raw_type_string()}::StaticType"

    if type == BOOL_TYPE:
        #return f"REG_BOOL_W_CUSTOM({name},{flags},{custom_type},{hint})"
        return f"make_bool_property_custom({name_offset_flags_hint},{custom_type})"

    elif type == FLOAT_TYPE:
        #return f"REG_FLOAT({name},{flags},{hint})"
        return f"make_float_property({name_offset_flags_hint})"

    elif type == INT_TYPE:
        return f"make_integer_property({name_offset_flags},sizeof({raw_str}),{hint})"
        #return f"REG_INT({name},{flags},{hint})"

    elif type == ASSET_PTR_TYPE:
        #return f"REG_ASSET_PTR({name},{flags})"
        #return f'make_struct_property({name_offset_flags}, "AssetPtr", {type_of_template}.classname)'
        return f'make_assetptr_property_new({name_offset_flags},{tooltip},&{type_of_template})'
    elif type == STRING_TYPE:
        #return f"REG_STDSTRING_CUSTOM_TYPE({name},{flags},{custom_type})"
        return f"make_string_property({name_offset_flags},{custom_type})"
    elif type == HANDLE_PTR_TYPE:
        #return f"REG_ENTITY_PTR({name},{flags})"
        #return f'make_struct_property({name_offset_flags}, "ObjPtr", {type_of_template}.classname)'
        return f'make_objhandleptr_property({name_offset_flags},{tooltip},&{type_of_template})'
    elif type == VEC3_TYPE:
        return f"make_vec3_property({name_offset_flags})"
        #return f"REG_VEC3({name},{flags})"
    elif type == QUAT_TYPE:
        return f"make_quat_property({name_offset_flags})"
        #return f"REG_QUAT({name},{flags})"
    elif type == ARRAY_TYPE:
        return f"make_new_array_type({name_offset_flags},{tooltip}, &vecdefnew_{name})"
        #return f"REG_STDVECTOR_NEW({name},{flags})"
    elif type == COLOR32_TYPE:
        #return f"REG_INT_W_CUSTOM({name}, {flags}, \"\", \"ColorUint\")"
        return f"make_integer_property({name_offset_flags},sizeof({raw_str}),{hint},\"ColorUint\")"
    elif type == OLD_VOID_STRUCT_TYPE:
        #return f"REG_STRUCT_CUSTOM_TYPE({name},{flags},{custom_type})"
        return f"make_struct_property({name_offset_flags},{custom_type})"

    elif type == STRUCT_TYPE:
        assert(not cpptype.is_pointer)
        structtype = cpptype.typename
        assert(structtype!=None)
        #assert(not prop.new_type.is_pointer)
        #structtype = prop.new_type.typename
        #assert(structtype!=None)
        return f"make_new_struct_type({name_offset_flags},{tooltip},&{structtype.classname}::StructType)"
    elif type == ENUM_TYPE:
        assert(not cpptype.is_pointer)
        enumtype = cpptype.typename
        assert(enumtype!=None)
        #name = prop.new_type.typename
        #assert(name!=None)
        #return f"REG_ENUM({name},{flags},{hint},{classname})"
        return f"make_enum_property({name_offset_flags},sizeof({raw_str}),&::EnumTrait<{enumtype.classname}>::StaticEnumType, {hint})"

    elif type == SOFTASSET_PTR_TYPE:
        #return f"REG_SOFT_ASSET_PTR({name},{flags})"
        return f'make_softassetptr_property_new({name_offset_flags},{tooltip},&{type_of_template})'
    elif type == CLASSTYPEINFO_TYPE:
        #return f'make_struct_property({name_offset_flags}, "ClassTypePtr", {type_of_template}.classname)'
        #return f"REG_CLASSTYPE_PTR({name},{flags})"
        return f'make_classtypeinfo_property({name_offset_flags},{tooltip},&{type_of_template})'
    elif type == STRINGNAME_TYPE:
         return f"make_stringname_property({name_offset_flags},{tooltip})"
    elif type == OTHER_CLASS_TYPE:
        base_typename = cpptype.typename
        if cpptype.is_pointer and (not base_typename is None) and ClassDef.is_self_derived_from(base_typename,typenames["IAsset"]):
            print(f"Found a raw IAsset derived pointer, reging prop as AssetPtr... ({base_typename.classname}, {name})")
            return f'make_assetptr_property_new({name_offset_flags},{tooltip},&{base_typename.classname}::StaticType)'


    print(f"Unknown type {name} {type}")
    assert(0)
    return ""

def escape_quote_characters(instr:str) -> str:
    outstr : str = ""
    for c in instr:
        if c== '"':
            outstr += "\\\""
        else:
            outstr += c
    return outstr

# code gen to make macros to make templates to make code ...
def write_prop(typenames:dict[str,ClassDef],prop : Property,newclass:ClassDef) -> str:
    prop.custom_type = f'"{prop.custom_type}"'
    prop.hint = f'"{prop.hint}"'
    prop.tooltip = f'"{escape_quote_characters(prop.tooltip)}"'
   
    offset_str = f"offsetof({newclass.classname}, {prop.name})"

    return output_macro_for_prop(typenames,prop.new_type,prop.name,prop.get_flags(),offset_str,prop.custom_type,prop.hint,prop.nameoverride, prop.tooltip)



def write_class_property(newclass:ClassDef,p:Property)->str:
    offset_str = f"offsetof({newclass.classname}, {p.name})"
    name_str = f"\"{p.name}\""
    name_and_offset = name_str + "," + offset_str + ",\"" + p.tooltip + "\""
    name_offset_flags = name_and_offset + ","+p.get_flags()
    return write_one_property_generic(p.new_type,name_offset_flags,p.name)

def write_array_callback(newclass:ClassDef, p:Property)->str:
    assert(p.new_type.type==ARRAY_TYPE)
    name_offset_flags_tooltip = f"\"\",0,PROP_DEFAULT,\"{p.tooltip}\""
    prop_str = write_one_property_generic(p.new_type.template_args[0],name_offset_flags_tooltip,"")
    arg = p.new_type.template_args[0].get_raw_type_string()
    callback = f"static StdVectorCallback<{arg}> vectorcallback_{p.name}({prop_str});"
    return callback

def write_one_property_generic(type:CppType, name_offset_flags_tooltip:str, property_name : str)->str:
    sizeof_str = f"sizeof({type.type_string_raw})"
    
    type_of_template :str= ""
    if len(type.template_args)>0:
        type_of_template = f"&{type.template_args[0].get_raw_type_string()}::StaticType"
    noft_and_type = name_offset_flags_tooltip+","+type_of_template

    if type.type == BOOL_TYPE:
        return f"make_bool_property({name_offset_flags_tooltip})"
    elif type.type == INT_TYPE:
        return f"make_int_property({name_offset_flags_tooltip},{sizeof_str})"
    elif type.type == FLOAT_TYPE:
        return f"make_float_property({name_offset_flags_tooltip})"
    elif type.type == ENUM_TYPE:
        enumtype = type.typename
        assert(enumtype!=None)
        return f"make_enum_property({name_offset_flags_tooltip},{sizeof_str}, &::EnumTrait<{enumtype.classname}>::StaticEnumType)"
    elif type.type == STRUCT_TYPE:
        assert(not type.is_pointer)
        structtype = type.typename
        assert(structtype!=None)
        return f"make_struct_actual_property({name_offset_flags_tooltip},&{structtype.classname}::StructType)"
    elif type.type == ARRAY_TYPE:
        assert(len(property_name)>0)
        return f"make_array_property({name_offset_flags_tooltip}, &vectorcallback_{property_name})"
    elif type.type == VEC3_TYPE:
        return f"make_vec3_property({name_offset_flags_tooltip})"
    elif type.type == VEC2_TYPE:
        return f"make_vec2_property({name_offset_flags_tooltip})"
    elif type.type == QUAT_TYPE:
        return f"make_quat_property({name_offset_flags_tooltip})"
    elif type.type == STRING_TYPE:
        return f"make_string_property({name_offset_flags_tooltip})"
    elif type.type == UNIQUE_PTR_TYPE:
        return f"make_unique_ptr_property({noft_and_type})"
    elif type.type == ASSET_PTR_TYPE:
        return f"make_asset_ptr_property({noft_and_type})"
    elif type.type == SOFTASSET_PTR_TYPE:
        return f"make_soft_asset_ptr_property({noft_and_type})"
    elif type.type == RAW_PTR_TYPE:
        return f"make_raw_ptr_property()"
    elif type.type == HANDLE_PTR_TYPE:
        return f"make_handle_ptr_property({noft_and_type})"
    elif type.type == CLASSTYPEINFO_TYPE:
        return f"make_classtype_property({noft_and_type})"
    elif type.type == STRINGNAME_TYPE:
         return f"make_stringname_property({name_offset_flags_tooltip})"
    return ""

def write_properties(newclass:ClassDef) -> str:
    output = f"PropertyInfoList {newclass.classname}::get_props()\n"
    output += "{\n"
    if len(newclass.properties)==0:
        return output + "\treturn {nullptr, 0};\n}\n"

    for p in newclass.properties:
        if p.new_type.type == ARRAY_TYPE:
            output += "\t" + write_array_callback(newclass,p) + "\n"
    output += "\tPropertyInfo* properties[] = {\n"
    for p in newclass.properties:
        output += "\t\t" + write_class_property(newclass,p)+",\n"
    output = output[:-2]
    output +=  "\n\t};\n"
    output += "\treturn {properties, sizeof(properties)/sizeof(PropertyInfo)};\n"
    output += "}\n"
    return output

def does_struct_have_serialize_function(newclass:ClassDef) -> bool:
    assert(newclass.object_type==ClassDef.TYPE_STRUCT)
    p = newclass.find_property_for_name("serialize")
    return p != None and p.new_type.type == FUNCTION_TYPE

def write_class(newclass : ClassDef):
    output = ""

    if newclass.object_type==ClassDef.TYPE_CLASS:
        output += write_properties(newclass) + "\n"
        output += f"ClassTypeInfo {newclass.classname}::StaticType = ClassTypeInfo(\n \
                    \"{newclass.classname}\",\n \
                    &{newclass.supername}::StaticType,\n \
                    {newclass.classname}::get_props,\n \
                    default_class_create<{newclass.classname}>(),\n \
                    {newclass.classname}::CreateDefaultObject,\n \
                    \"{newclass.tooltip}\"\n\
                );\n"
        output += f"const ClassTypeInfo& {newclass.classname}::get_type() const"
        output += "{ return " + f"{newclass.classname}::StaticType;" + "}\n"

    elif newclass.object_type == ClassDef.TYPE_STRUCT:
        has_serialize = does_struct_have_serialize_function(newclass)
        if has_serialize:
            output += f"static void {newclass.classname}_serialize_private(void* p, Serializer& s)\n"
            output += "{\n"
            output += f"\t(({newclass.classname}*)p)->serialize(s);\n"
            output += "}\n"

        output += write_properties(newclass) + "\n"
        output += f"StructTypeInfo {newclass.classname}::StructType = StructTypeInfo(\n \
                \"{newclass.classname}\",\n"
        if has_serialize:
            output += f"{newclass.classname}_serialize_private\n"
        else:
            output += "nullptr\n"
        output += ");\n"
    return output

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
        return f"StringName(get_std_string_from_lua(L,{index}).c_str())"
    elif newType.type == ARRAY_TYPE:
        assert(len(newType.template_args)==1)
        type_of_template = newType.template_args[0].get_raw_type_string()
        return f"get_std_vector_from_lua<{type_of_template}>(L,{index},[&]() -> {type_of_template} {{  return {write_get_type_from_lua_func(newType.template_args[0],-1)}; }})"
    elif newType.type == STRUCT_TYPE:
        assert(newType.typename!=None)
        return f"get_{newType.typename.classname}_from_lua(L,{index})"
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
        return f"push_std_string_to_lua(L,{cppVarName}.get_c_str())"
    elif newType.type == ARRAY_TYPE:
        assert(len(newType.template_args)==1)
        type_of_template = newType.template_args[0].get_raw_type_string()
        return f"push_std_vector_to_lua(L,{cppVarName},[&]({type_of_template} val) {{  {write_push_type_to_lua_func(newType.template_args[0],'val')}; }})"
    elif newType.type == STRUCT_TYPE:
        assert(newType.typename!=None)
        return f"push_{newType.typename.classname}_to_lua(L,{cppVarName})"
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
        output += f"\t{my_obj_type}* myObj = obj ? obj->cast_to<{my_obj_type}>() : nullptr;\n"
        # assert it
        output += "\tif(!myObj) {  assert(0); };\n" # fixme
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


def write_scriptable_class(newclass : ClassDef) -> str:
    assert(newclass.scriptable)
    output = ""
    output += f"class ScriptImpl_{newclass.classname} : public {newclass.classname} " + " {\n"
    output += "public:\n"
    output += f"""
    const ClassTypeInfo* type = nullptr;
    const ClassTypeInfo& get_type() const final {{ return *type; }}
    """
    for f in newclass.properties:
        if f.new_type.type==FUNCTION_TYPE and f.is_virtual:
            output += "\t" + f.return_type.get_raw_type_string() + f" {f.name}("
            for argType,argName in f.func_args:
                output += argType.get_raw_type_string() + " " + argName
                output += ", "
            if len(f.func_args)>0:
                output = output[:-2]
            output += ") final {"
            # get my table
            # find function
            # push args
            # get return value
            output+= f"""
        lua_State* L = ScriptManager::inst->get_lua_state();
        int myTable = get_table_registry_id();
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
            if f.return_type.type!=NONE_TYPE:
                return_num = 1
            arg_count = len(f.func_args)+1 # include self parameter
            
            error_str = f"\"During call {newclass.classname}::{f.name}\" + std::string(\"(luatype=\")+type->classname+\"):\""
            output+= f"""
            if (lua_pcall(L, {arg_count}, {return_num}, 0) != LUA_OK) {{
                const char* error = lua_tostring(L, -1);
                printf("Lua error: %s\\n", error);
                lua_pop(L, 1);  // pop error

                throw LuaRuntimeError({error_str} + error);
            }}
            else {{\n"""
            if return_num == 1:
                output += f"\t\t\treturn {write_get_type_from_lua_func(f.return_type,-1)};\n"
            else:
                pass
            output += "\t\t\t}\n"
            output += "\t\t}\n\t\telse{\n"
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
    return False
def get_bool_str(b:bool)->str:
    if b:
        return "true"
    else:
        return "false"
def write_class_old(typenames:dict[str,ClassDef],newclass : ClassDef)->str:


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
            for f in newclass.properties:
                if f.new_type.type==FUNCTION_TYPE:
                    output += f"{{ \"{f.name}\",{get_bool_str(f.is_static)},{get_bool_str(f.is_virtual)}, lua_binding_{newclass.classname}_{f.name}  }},\n"
                    count += 1
            output = output[:-2]
            output += "};\n"
            function_str = f"{newclass.classname}_function_list,{count}"


        scriptable_alloc = "nullptr"
        if newclass.scriptable:
            output += write_scriptable_class(newclass)
            scriptable_alloc = f"get_allocate_script_impl_internal<ScriptImpl_{newclass.classname}>()"

        output += f"ClassTypeInfo {newclass.classname}::StaticType = ClassTypeInfo(\n \
                    \"{newclass.classname}\",\n \
                    {super_typeinfo_str},\n \
                    {newclass.classname}::get_props,\n \
                    default_class_create<{newclass.classname}>(),\n \
                    {newclass.classname}::CreateDefaultObject,{function_str},{scriptable_alloc});\n"
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


    if newclass.object_type == ClassDef.TYPE_CLASS or newclass.object_type == ClassDef.TYPE_STRUCT:

#lua_binding_{newclass.classname}_{funcProp.name}
        output += f"const PropertyInfoList* {newclass.classname}::get_props()\n"
        output += "{\n"
        if class_has_actual_properties(newclass):
            for p in newclass.properties:
                if p.new_type.type == ARRAY_TYPE:
                    prop_str = output_macro_for_prop(typenames,p.new_type.template_args[0],"","PROP_DEFAULT","0",'""','""','""','""')
                    arg = p.new_type.template_args[0].get_raw_type_string()
                    callback = f"static StdVectorCallback<{arg}> vecdefnew_{p.name}({prop_str});"
                    output += f"\t{callback}\n"


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

def get_lua_type_string(new_type:CppType) -> str:
    output = ""
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
        output += f"{new_type.typename.classname}"
    elif new_type.type == ASSET_PTR_TYPE or new_type.type == HANDLE_PTR_TYPE:
        output += f"{type_of_template}|nil"
    elif new_type.type == STRING_TYPE or new_type.type == STRINGNAME_TYPE:
        output += f"string"
    elif new_type.type == OTHER_CLASS_TYPE:
        assert(new_type.typename!=None)
        output += f"{new_type.typename.classname}|nil"
    elif new_type.type == VEC3_TYPE:
        output += "lVec3"
    elif new_type.type == QUAT_TYPE:
        output += "lQuat"
    else:
        output += "any"
    return output

def write_lua_class(newclass:ClassDef) -> str:
    output = ""

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
        output += "\n"
        for p in newclass.properties:
            if p.new_type.type != FUNCTION_TYPE:
                output += f"---@field {p.name} "
                output += get_lua_type_string(p.new_type)           
                output += "\n"
        output +=  newclass.classname + " = {\n}\n"

        for p in newclass.properties:
            if p.new_type.type == FUNCTION_TYPE:
                if p.lua_generic and p.return_type.type != NONE_TYPE:
                    output += f"---@generic T : {get_lua_type_string(p.return_type)}\n"
                if p.return_type.type != NONE_TYPE:
                    if p.lua_generic:
                        output += "---@return T\n"
                    else:               
                        output += "---@return " + get_lua_type_string(p.return_type) + "\n"
                for argType,argName in p.func_args:
                    if p.lua_generic and argType.type == OTHER_CLASS_TYPE and argType.typename != None and argType.typename.classname == "ClassTypeInfo":
                        output += f"---@param {argName} T\n"
                    else:
                        output += "---@param " + argName + " " + get_lua_type_string(argType) + "\n"
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
                    output += f"---@param arg{count} {get_lua_type_string(t)}\n"
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
  

def write_output_file(GENERATED_DIR:str,filename:str,root:str,classes:list[ClassDef],additional_includes:list[str], typenames:dict[str,ClassDef]):
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
            # write struct stubs first
            output = ""
            for c in classes:
                if c.object_type == ClassDef.TYPE_STRUCT:
                    output += f"void push_{c.classname}_to_lua(lua_State* L, const {c.classname}& value);\n"
                    output += f"{c.classname} get_{c.classname}_from_lua(lua_State* L, int index);\n"
            file.write(output)

            for c in classes:
                file.write(write_class_old(typenames,c))
    generated_path = generated_path[:-4] + ".lua"
    with open(generated_path,"w") as luaFile:
        print(f"Writing lua stubs {generated_path}")

        time_now = datetime.now()
        timestamp_str = time_now.strftime("%Y-%m-%d %H:%M:%S")
        luaFile.write(f"--- GENERATED LUA FILE FROM C++ CLASSES v{VERSION} {timestamp_str}\n")

        if len(classes)>0:
            for c in classes:
                luaFile.write(write_lua_class(c))