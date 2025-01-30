#pragma once
#include <cassert>
#include "Assets/IAsset.h"

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
