#pragma once

#include "Framework/ReflectionProp.h"
#include "Assets/IAsset.h"
#include "Framework/ArrayReflection.h"
#include <unordered_map>

class Entity;
CLASS_H(SerializeEntityObjectContext,ClassBase)
public:
	std::unordered_map<ClassBase*, uint64_t> to_serialize_index;
	std::vector<Entity*> unserialized;

	Entity* entity_serialzing = nullptr;
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
	
	// Assets that failed to load are considered to be the same as nullptrs as far as gameplay is concerned
	// Make sure to check if the ptr is valid before using it!

	bool is_valid() const {
		return ptr && !ptr->did_load_fail();
	}

	// use this only when you know what youre doing!
	// likely going to be inside systems to have their own error handling in case of failed asset loads
	T* get_unsafe() const {
		return ptr;
	}

	T* get() const {
		return is_valid() ? ptr : nullptr; 
	}

	bool did_fail() {
		return ptr && ptr->did_load_fail();
	}

	operator bool() const {
		return is_valid();
	}
	T* operator->() { 
		assert(is_valid());
		return ptr; 
	}
	T& operator*() {
		assert(is_valid());
		return *ptr; 
	}

	// this is a fun one if you use this with a nullptr against a failed asset, I think it makes more sense to return true 
	// in that case  because the rest of the api is similar like that as well (except for get_unsafe)
	// just be mindful that this might not be exactly what you want!
	bool operator==(AssetPtr<T> other) const {
		return get() == other.get();
	}
	bool operator!=(AssetPtr<T> other) const {
		return get() != other.get();
	}

	template<typename K>
	AssetPtr<K> cast_to() {
		assert(is_valid());
		K* newPtr = ptr->cast_to<K>();
		return AssetPtr<K>(newPtr);
	}

	T* ptr = nullptr;
};
using GenericAssetPtr = AssetPtr<IAsset>;


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
	T* get() const { return ptr; }
	T* operator->() const { return ptr; }
	T& operator*() const { return *ptr; }
	T* ptr = nullptr;
};


#if 0
template<typename T>
inline PropertyInfo make_object_ptr_property(const char* name, uint16_t offset, uint32_t flags, ObjPtr<T>* dummy) {
	return make_struct_property(name, offset, flags, "ObjectPointer", T::StaticType.classname);
}
#define REG_OBJECT_PTR(name, flags) make_object_ptr_property(#name, offsetof(MyClassType,name),flags,&((MyClassType*)0)->name)
#endif

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


