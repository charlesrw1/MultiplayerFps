#pragma once

#include <string>
#include "Framework/Util.h"
#include "Framework/ClassBase.h"
#include "Framework/Reflection2.h"

#ifdef EDITOR_BUILD
#include <unordered_set>
#endif

// references:
// to know when to load unload assets reference counting is used. however, its not done per entity etc. its manually done
// load a map -> loads all the data but doesnt inc reference

// textures: special case
// textures are loaded after everything else, since they take up more memory
// so load_asset_bundle({model})
//			load_model()
				//load_mat()
//					load_tex() (queues till end)
//

// Base asset class
// Override to add a new resource that can be used by the game
// See AssetMetadata in AssetRegistry.h to allow for searching for the asset in the asset browser
class GcMarkingInterface;
class IAssetLoadingInterface;
class PrimaryAssetLoadingInterface;
class TestAssetLoadingInterface;
class IAsset : public ClassBase {
public:
	CLASS_BODY(IAsset);

	IAsset();
	virtual ~IAsset();


	const std::string& get_name() const { return path; }
	REF std::string get_name_l() { return path; }


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


	void editor_set_newly_made_path(const std::string& path) {
		this->path = path;
	}

	bool is_this_globally_referenced() const {
		return persistent_flag;
	}

	int get_reference_count() const {
		return internal_reference_count;
	}
	void inc_ref_count() {
		internal_reference_count += 1;
	}
	void dec_ref_count_and_uninstall_if_zero() {
		internal_reference_count -= 1;
		if (internal_reference_count <= 0) {
			uninstall();

		}
	}
	void dec_ref_count() {
		internal_reference_count -= 1;
	}
	
private:
	// you can allocate a UserStruct in load_asset and have access to it in post_load. It will get deleted for you if non nullptr
	virtual bool load_asset(IAssetLoadingInterface* loading) = 0; // called on loader thread
	virtual void post_load() = 0;	// called on main thread after load_asset is executed
	// called on the main thread to destroy and uninstall any used resources
	virtual void uninstall() = 0;
	virtual void move_construct(IAsset* other) = 0;


	// this is only called on the main thread
	void set_not_loaded_main_thread() {
		is_loaded = false;
	}

	// is_loaded is sometimes thread safe (inside a load job it is safe. when an asset is publically outside of the AssetDatabase, it is always safe.)
	// it is not safe inside the AssetDatabase when this asset's "has_run_post_load" is false (since an async might be writing to is_loaded)
	// has_run_post_load is always thread safe as its only called on the main thread

	int internal_reference_count = 0;
	std::string path;				// filepath or name of asset
	bool load_failed = false;		// did the asset fail to load
	bool is_loaded = false;		// is the asset's data in memory
	bool is_from_disk = true;		// otherwise created at runtime
	bool persistent_flag = false;
	bool used_flag = false;

#ifdef EDITOR_BUILD
	uint64_t asset_load_time = 0;
#endif

	friend class AssetDatabaseImpl;
	friend class LoadJob;
	friend class AssetDatabase;
	friend class AssetBackend;
	friend class AssetRegistrySystem;
	friend class GcMarkingInterface;
	friend class PrimaryAssetLoadingInterface;
	friend class TestAssetLoadingInterface;
};
