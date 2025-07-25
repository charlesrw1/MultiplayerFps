#pragma once

#include <string>
#include "Framework/Util.h"
#include "Framework/ClassBase.h"
#include "Framework/Reflection2.h"

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

	int internal_reference_count = 0;
	std::string path;				// filepath or name of asset
	bool load_failed = false;		// did the asset fail to load
	bool is_loaded = false;		// is the asset's data in memory
	bool is_from_disk = true;		// otherwise created at runtime
	bool persistent_flag = false;
	bool used_flag = false;


	friend class AssetDatabaseImpl;
	friend class LoadJob;
	friend class AssetDatabase;
	friend class AssetBackend;
	friend class AssetRegistrySystem;
	friend class GcMarkingInterface;
	friend class PrimaryAssetLoadingInterface;
	friend class TestAssetLoadingInterface;
};
