#pragma once
#include "ReflectionProp.h"
#include "Framework/BinaryReadWrite.h"
void read_props_to_object_binary(ClassBase* dest_obj, const ClassTypeInfo* typeinfo, BinaryReader& in, ClassBase* userptr);
void write_properties_with_diff_binary(const PropertyInfoList& list, void* ptr, const void* diff_class, FileWriter& out, ClassBase* userptr);