#pragma once

#include <string>
#include "Framework/Util.h"
#include "Framework/ClassBase.h"
#include "Framework/Reflection2.h"
#include <cassert>

// IAsset — base class for engine assets (Texture, Model, MaterialInstance, ...).
//
// Commitments of this asset system (read before changing AssetDatabase):
//
//   1. Load once, keep forever.  The asset map grows monotonically until shutdown.
//      No GC, no refcount, no eviction.  CPU-side asset structs are small; heavy
//      data (GPU pixels, vertex buffers) is managed by the renderer on its own
//      residency policy and is independent of asset lifetime.
//
//   2. Addresses are stable across reload.  Hot-reload runs uninstall() + load_asset()
//      + post_load() on the same instance.  Anyone holding a Texture* / Model* /
//      MaterialInstance* keeps a valid pointer across reload — only the data inside
//      changes.  Sub-objects exposed by accessors (e.g. Model::get_skel()) must
//      preserve this same guarantee.
//
//   3. Failed loads survive serialization round-trip.  When a path fails to load,
//      a tombstone instance is left in the AssetDatabase map with `path` set and
//      `did_load_fail() == true`.  Maps with broken references load, edit, and
//      save back byte-for-byte.
//
//   4. Sync only.  No async loading.  find<T>(path) blocks on the calling thread.
//
//   5. Reload cascades live in concrete post_load() (see MaterialInstance for
//      the pattern): if your asset type has dependents that need refreshing on
//      reload, gate the cascade on a "have we run post_load before?" flag so it
//      fires only on reload, not on initial load.  Runtime components that hold
//      sub-object pointers do NOT subscribe to delegates — the reload path walks
//      the scene and calls a passive refresh_after_<asset>_reload(...) method on
//      each affected component.
//
// If you violate any of (1)-(3), AssetPtr<T> users may get dangling pointers or
// silent serialization data loss — both have caused bugs before.

class IAsset : public ClassBase
{
public:
	CLASS_BODY(IAsset);

	IAsset();
	virtual ~IAsset();

	const std::string& get_name() const { return path; }
	REF std::string get_name_l() { return path; }

	bool was_load_attempted() const { return load_attempted; }
	bool did_load_fail() const { return load_failed; }
	bool is_valid_to_use() const { return load_attempted && !load_failed; }

	// Marks this IAsset as a runtime asset that is NOT owned by AssetDatabase.
	// Intended for the dynamic-pool path only (ModelMan dynamic models,
	// DynamicMaterialAllocator dynamic materials).
	// Sets `path` and `load_attempted = true`
	// so the asset reads as valid_to_use.  Asset-DB-managed assets go through
	// AssetDatabase::install_runtime instead.
	void init_runtime_unmanaged(const std::string& name) {
		this->path = name;
		this->load_attempted = true;
		this->load_failed = false;
	}

private:
	// Hot-reload is in-place: AssetDatabase::reload() runs uninstall() then load_asset()
	// then post_load() on the same instance. The IAsset* address stays stable across reload,
	// so anyone holding a raw pointer keeps a valid pointer — only the data inside changes.
	// On reload, uninstall() must release everything load_asset()/post_load() allocated;
	// load_asset() is called with internal state cleared.
	virtual bool load_asset() = 0;
	virtual void post_load() = 0;
	virtual void uninstall() = 0;

	std::string path;			  // filepath or name of asset; set on insert into AssetDatabase, never cleared
	bool load_attempted = false;  // true only after load_asset has returned (or thrown); never reverts
	bool load_failed = false;	  // did the most recent load_asset attempt fail

	friend class AssetDatabaseImpl;
	friend class AssetDatabase;
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
