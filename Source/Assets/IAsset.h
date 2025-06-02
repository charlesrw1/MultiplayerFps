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

class IAssetLoadingInterface;
CLASS_H(IAsset, ClassBase)
public:

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

#ifdef EDITOR_BUILD
	std::unordered_set<IAsset*> reload_dependents;	// when asset is reloaded, these will also get reloaded	
	void editor_set_newly_made_path(const std::string& path) {
		this->path = path;
	}
#endif

protected:

private:
	// you can allocate a UserStruct in load_asset and have access to it in post_load. It will get deleted for you if non nullptr
	virtual bool load_asset(IAssetLoadingInterface* loading) = 0; // called on loader thread
	virtual void post_load() = 0;	// called on main thread after load_asset is executed
	
	// called on the main thread to destroy and uninstall any used resources
	virtual void uninstall() = 0;

	// called when an object exists already, but the asset system is referencing it on a different channel and needs to 
	// mark all dependent objects (models mark materials, materials mark textures, etc.)
	// DONT modify anything in this object!!
	// this needs to be pure
	// this is like a GC sweep
	virtual void sweep_references(IAssetLoadingInterface* loading) const = 0;

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

	bool is_this_globally_referenced() const {
		return is_system;
	}
	// this is only called on the main thread
	void set_not_loaded_main_thread() {
		is_loaded = has_run_post_load = false;
	}

	// is_loaded is sometimes thread safe (inside a load job it is safe. when an asset is publically outside of the AssetDatabase, it is always safe.)
	// it is not safe inside the AssetDatabase when this asset's "has_run_post_load" is false (since an async might be writing to is_loaded)
	// has_run_post_load is always thread safe as its only called on the main thread

	std::string path;				// filepath or name of asset
	bool load_failed = false;		// did the asset fail to load
	bool is_system = false;
	bool is_loaded = false;		// is the asset's data in memory
	bool has_run_post_load = false; // has post_load been run
	bool is_from_disk = true;		// otherwise created at runtime
#ifdef EDITOR_BUILD
	uint64_t asset_load_time = 0;
#endif

	friend class AssetDatabaseImpl;
	friend class LoadJob;
	friend class AssetDatabase;
	friend class AssetBackend;
	friend class AssetRegistrySystem;
};
