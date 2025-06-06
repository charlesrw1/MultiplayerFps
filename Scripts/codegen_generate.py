from codegen_lib import *

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


def output_macro_for_prop(cpptype:CppType,name:str,flags:str,offset:str,custom_type:str,hint:str,nameoverride:str,tooltip:str)->str:
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
        return f'make_struct_property({name_offset_flags}, "AssetPtr", {type_of_template}.classname)'
    elif type == STRING_TYPE:
        #return f"REG_STDSTRING_CUSTOM_TYPE({name},{flags},{custom_type})"
        return f"make_string_property({name_offset_flags},{custom_type})"
    elif type == HANDLE_PTR_TYPE:
        #return f"REG_ENTITY_PTR({name},{flags})"
        return f'make_struct_property({name_offset_flags}, "ObjPtr", {type_of_template}.classname)'
    elif type == FUNCTION_TYPE:
        name = nameoverride if len(nameoverride)!=0 else name
        name = f'"{name}"'
        if False:#fixme
            return f"REG_GETTER_FUNCTION({name},{name})"
        else:
            return f"REG_FUNCTION_EXPLICIT_NAME({name},{name})"
    elif type == MULTICAST_TYPE:
        return f"REG_MULTICAST_DELEGATE({name})"
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
        return f'make_struct_property({name_offset_flags}, "AssetPtr", {type_of_template}.classname)'
    elif type == CLASSTYPEINFO_TYPE:
        return f'make_struct_property({name_offset_flags}, "ClassTypePtr", {type_of_template}.classname)'
        #return f"REG_CLASSTYPE_PTR({name},{flags})"
    else:
        print(f"Unknown type {name} {type}")
        assert(0)
    return ""

# code gen to make macros to make templates to make code ...
def write_prop(prop : Property,newclass:ClassDef) -> str:
    prop.custom_type = f'"{prop.custom_type}"'
    prop.hint = f'"{prop.hint}"'
    prop.tooltip = f'"{prop.tooltip}"'
   
    offset_str = f"offsetof({newclass.classname}, {prop.name})"

    return output_macro_for_prop(prop.new_type,prop.name,prop.get_flags(),offset_str,prop.custom_type,prop.hint,prop.nameoverride, prop.tooltip)



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

def write_class_old(newclass : ClassDef)->str:
    if newclass.object_type==ClassDef.TYPE_CLASS and newclass.super_type_def == None:
        return ""

    output = ""
    if newclass.object_type==ClassDef.TYPE_CLASS:
        output += f"ClassTypeInfo {newclass.classname}::StaticType = ClassTypeInfo(\n \
                    \"{newclass.classname}\",\n \
                    &{newclass.supername}::StaticType,\n \
                    {newclass.classname}::get_props,\n \
                    default_class_create<{newclass.classname}>(),\n \
                    {newclass.classname}::CreateDefaultObject\n \
                );\n"
    elif newclass.object_type == ClassDef.TYPE_STRUCT:
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
        output += ");\n"


    if newclass.object_type == ClassDef.TYPE_CLASS or newclass.object_type == ClassDef.TYPE_STRUCT:
        output += f"const PropertyInfoList* {newclass.classname}::get_props()\n"
        output += "{\n"
        if len(newclass.properties) > 0:
            for p in newclass.properties:
                if p.new_type.type == ARRAY_TYPE:
                    prop_str = output_macro_for_prop(p.new_type.template_args[0],"","PROP_DEFAULT","0",'""','""','""','""')
                    arg = p.new_type.template_args[0].get_raw_type_string()
                    callback = f"static StdVectorCallback<{arg}> vecdefnew_{p.name}({prop_str});"
                    output += f"\t{callback}\n"


            output += f"    START_PROPS({newclass.classname})"
            for p in newclass.properties:
                output += "\n\t\t" + write_prop(p,newclass) + ","
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
            for c in classes:
                file.write(write_class_old(c))