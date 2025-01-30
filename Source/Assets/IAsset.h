#pragma once

#include <string>
#include "Framework/Util.h"
#include "Framework/ClassBase.h"

#ifdef EDITOR_BUILD
#include <unordered_set>
#endif

// Base asset class
// Override to add a new resource that can be used by the game
// See AssetMetadata in AssetRegistry.h to allow for searching for the asset in the asset browser

CLASS_H(IAsset, ClassBase)
public:
	static const uint32_t GLOBAL_REFERENCE_MASK = (1ul << 31);
	static const uint32_t GLOBAL_REFERENCE_CHANNEL = 31;
	static const uint32_t EDITOR_REFERENCE_CHANNEL = 30;

	IAsset();
	virtual ~IAsset();


	const std::string& get_name() const { return path; }
	bool is_loaded_in_memory() const { return is_loaded; }
	bool did_load_fail() const { return load_failed; }

	bool is_valid_to_use() const {
		return is_loaded && !load_failed;
	}

	bool get_is_loaded() const {
		return is_loaded;
	}
	
	void set_loaded_manually_unsafe(const std::string& path) {
		this->path = path;
		is_loaded = true;
		load_failed = false;
	}

protected:

private:
	// you can allocate a UserStruct in load_asset and have access to it in post_load. It will get deleted for you if non nullptr
	virtual bool load_asset(ClassBase*& outUserStruct) = 0; // called on loader thread
	virtual void post_load(ClassBase* inUserStruct) = 0;	// called on main thread after load_asset is executed
	
	// called on the main thread to destroy and uninstall any used resources
	virtual void uninstall() = 0;

	// called when an object exists already, but the asset system is referencing it on a different channel and needs to 
	// mark all dependent objects (models mark materials, materials mark textures, etc.)
	// DONT modify anything in this object!!
	// this needs to be pure
	// this is like a GC sweep
	virtual void sweep_references() const = 0;

	// this is used for hot reloading of an existing asset
	// Execution:
	// asyncronous:
	// temp = default_object()
	// temp->load_asset(user) 
	// syncronous:
	// asset->move_construct(temp)
	// delete temp
	// asset->post_load(user)
	virtual void move_construct(IAsset* src) = 0;

	// for hot reloading:
	// if asset has an import file (models/textures)
	// then return "true" if the asset should be reloaded by inspection
	// other assets are handled automatically

	virtual bool check_import_files_for_out_of_data() const {
		return false;
	}
	
	// reference_bitmask is UNSAFE to access without having the active loader mutex
	bool is_this_referenced_by_anything() const {
		return reference_bitmask_internal != 0;
	}

	bool is_this_globally_referenced() const {
		return reference_bitmask_internal & GLOBAL_REFERENCE_MASK;
	}

	// if its referenced on the global channel, then its lifetime is considered referenced everywhere
	bool is_channel_referenced(uint32_t channel) const {
		return is_this_globally_referenced() || reference_bitmask_internal & (1ul << channel);
	}

	bool is_mask_refererenced(uint32_t mask) const {
		return is_this_globally_referenced() || (reference_bitmask_internal & mask) == mask;
	}


	std::string path;				// filepath or name of asset

	// bitmask: allows for 32 "things" to reference this object to allow for 32 independent lifetimes
	// "things" in this engine are levels, with the 31th bit being reserved for "system assets" (ie this bit never gets unloaded)
	// each level is mapped to an indexed bit, when the level is unloaded, assets unset the bit of the loaded level
	// anything with no references is removed when the asset system is called to do so
	// the 0th bit is the main level and used for default allocations with a "not infinite but not specific" lifetime
	// any allocations corresponding to a streamed level go in the specified bit
	// a quirk: if an asset is called for access while its loaded but on a different channel, then a load job is still incurred
	// the load job will be much simpler (just has to sweep references) but its not free. Thus its paramount to mark everything you might potentially need upfront inside the load job
	uint32_t reference_bitmask_internal = 0;
	uint32_t reference_bitmask_threadsafe = 0;
	void set_both_reference_bitmasks_unsafe(uint32_t b) {
		reference_bitmask_internal = reference_bitmask_threadsafe = b;
	}
	void move_internal_to_threadsafe_bitmask_unsafe() {
		reference_bitmask_threadsafe = reference_bitmask_internal;
	}

	bool load_failed = false;		// did the asset fail to load

	// this is only called on the main thread
	void set_not_loaded_main_thread() {
		is_loaded = has_run_post_load = false;
	}

	// is_loaded is sometimes thread safe (inside a load job it is safe. when an asset is publically outside of the AssetDatabase, it is always safe.)
	// it is not safe inside the AssetDatabase when this asset's "has_run_post_load" is false (since an async might be writing to is_loaded)
	// has_run_post_load is always thread safe as its only called on the main thread

	bool is_loaded = false;		// is the asset's data in memory
	bool has_run_post_load = false; // has post_load been run
	bool is_from_disk = true;		// otherwise created at runtime

#ifdef EDITOR_BUILD
	uint64_t asset_load_time = 0;
#endif

	friend class AssetDatabaseImpl;
	friend class LoadJob;
	friend class AssetDatabase;
	friend class AssetRegistrySystem;
};
