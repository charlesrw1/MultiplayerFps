#pragma once

#include "Framework/ReflectionProp.h"
#include "IAsset.h"
#include "Framework/ArrayReflection.h"
#include <unordered_map>

class Entity;
CLASS_H(SerializeEntityObjectContext,ClassBase)
	std::unordered_map<void*, uint64_t> to_serialize_index;

	Entity* entity_serialzing = nullptr;
	uint64_t serialzing_entity_index = 0;
};


template<typename T>
class AssetPtr
{
public:
	using TYPE = T;
	// work around
	~AssetPtr() {
		static_assert(std::is_base_of<IAsset, T>::value, "AssetPtr must derive from IAsset");
	}
	AssetPtr() = default;
	AssetPtr(T* p) :ptr(p) {}
	T* get() { return ptr; }
	T* operator->() { return ptr; }
	T& operator*() { return *ptr; }
	T* ptr = nullptr;
};


template<typename T>
inline PropertyInfo make_asset_ptr_property(const char* name, uint16_t offset, uint32_t flags, AssetPtr<T>* dummy) {
	return make_struct_property(name, offset, flags, "AssetPtr", T::StaticType.classname);
}
#define REG_ASSET_PTR(name, flags) make_asset_ptr_property(#name, offsetof(MyClassType,name),flags,&((MyClassType*)0)->name)


template<typename T>
struct GetAtomValueWrapper<AssetPtr<T>> {
	static PropertyInfo get() {
		return make_struct_property("", 0, PROP_DEFAULT, "AssetPtr", T::StaticType.classname);
	}
};

// A pointer to a ClassBase, this has serilization possibilities
template<typename T>
class ObjPtr
{
public:
	using TYPE = T;
	~ObjPtr() {
		static_assert(std::is_base_of<ClassBase, T>::value, "ObjPtr must derive from ClassBase");
	}
	ObjPtr() = default;
	ObjPtr(T* p) :ptr(p) {}
	T* get() { return ptr; }
	T* operator->() { return ptr; }
	T& operator*() { return *ptr; }
	T* ptr = nullptr;
};



template<typename T>
inline PropertyInfo make_object_ptr_property(const char* name, uint16_t offset, uint32_t flags, ObjPtr<T>* dummy) {
	return make_struct_property(name, offset, flags, "ObjectPointer", T::StaticType.classname);
}
#define REG_OBJECT_PTR(name, flags) make_object_ptr_property(#name, offsetof(MyClassType,name),flags,&((MyClassType*)0)->name)


class EntityComponent;
inline PropertyInfo make_entity_comp_ptr_property(const char* name, uint16_t offset, uint32_t flags, ObjPtr<EntityComponent>* dummy) {
	return make_struct_property(name, offset, flags, "EntityCompPtr", "");
}
#define REG_ENTITY_COMPONENT_PTR(name, flags) make_entity_comp_ptr_property(#name, offsetof(MyClassType,name),flags,&((MyClassType*)0)->name)

template<typename T>
struct GetAtomValueWrapper<ObjPtr<T>> {
	static PropertyInfo get() {
		return make_struct_property("", 0, PROP_DEFAULT, "ObjectPointer", T::StaticType.classname);
	}
};


