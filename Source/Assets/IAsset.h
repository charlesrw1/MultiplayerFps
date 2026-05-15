#pragma once

#include <string>
#include "Framework/Util.h"
#include "Framework/ClassBase.h"
#include "Framework/Reflection2.h"
#include <cassert>

class IAsset : public ClassBase
{
public:
	CLASS_BODY(IAsset);

	IAsset();
	virtual ~IAsset();

	const std::string& get_name() const { return path; }
	REF std::string get_name_l() { return path; }

	bool is_loaded_in_memory() const { return is_loaded; }
	bool did_load_fail() const { return load_failed; }

	bool is_valid_to_use() const { return is_loaded && !load_failed; }

	bool get_is_loaded() const { return is_loaded; }

	void set_loaded_manually_unsafe(const std::string& path) {
		this->path = path;
		is_loaded = true;
		load_failed = false;
	}

	void editor_set_newly_made_path(const std::string& path) { this->path = path; }

protected:
	void set_is_loaded(bool b) { is_loaded = b; }

private:
	// Hot-reload is in-place: AssetDatabase::reload() runs uninstall() then load_asset()
	// then post_load() on the same instance. The IAsset* address stays stable across reload,
	// so anyone holding a raw pointer keeps a valid pointer — only the data inside changes.
	// On reload, uninstall() must release everything load_asset()/post_load() allocated;
	// load_asset() is called with internal state cleared.
	virtual bool load_asset() = 0;
	virtual void post_load() = 0;
	virtual void uninstall() = 0;

	// this is only called on the main thread
	void set_not_loaded_main_thread() { is_loaded = false; }

	std::string path;		  // filepath or name of asset
	bool load_failed = false; // did the asset fail to load
	bool is_loaded = false;	  // is the asset's data in memory
	bool is_from_disk = true; // otherwise created at runtime

	friend class AssetDatabaseImpl;
	friend class AssetDatabase;
	friend class AssetRegistrySystem;
};

template <typename T> class AssetPtr
{
public:
	using TYPE = T;
	// work around
	~AssetPtr() { static_assert(std::is_base_of<IAsset, T>::value, "AssetPtr must derive from IAsset"); }
	AssetPtr() = default;
	AssetPtr(T* p) : ptr(p) {}

	// Assets that failed to load are considered to be the same as nullptrs as far as gameplay is concerned
	// Make sure to check if the ptr is valid before using it!

	bool is_valid() const { return ptr && !ptr->did_load_fail(); }

	// use this only when you know what youre doing!
	// likely going to be inside systems to have their own error handling in case of failed asset loads
	T* get_unsafe() const { return ptr; }

	T* get() const { return is_valid() ? ptr : nullptr; }

	bool did_fail() { return ptr && ptr->did_load_fail(); }

	operator bool() const { return is_valid(); }
	T* operator->() const {
		assert(is_valid());
		return ptr;
	}
	T& operator*() const {
		assert(is_valid());
		return *ptr;
	}
	bool operator==(AssetPtr<T> other) const { return get() == other.get(); }
	bool operator!=(AssetPtr<T> other) const { return get() != other.get(); }

	template <typename K> AssetPtr<K> cast_to() {
		assert(is_valid());
		K* newPtr = ptr->cast_to<K>();
		return AssetPtr<K>(newPtr);
	}

	// implicit conversion to IAsset*
	operator const T*() const { return get(); }

	T* ptr = nullptr;
};
using GenericAssetPtr = AssetPtr<IAsset>;
static_assert(sizeof(GenericAssetPtr) == sizeof(IAsset*), "AssetPtr<> must be the same size as an IAsset*");
