#pragma once

#include <string>
#include "Framework/Util.h"
#include "Framework/ClassBase.h"

//struct OnUpdatedCallback;

CLASS_H(IAsset, ClassBase)
public:
	IAsset();
	virtual ~IAsset();

	const std::string& get_name() const { return path; }
	bool is_loaded_in_memory() const { return is_loaded; }
	bool did_load_fail() const { return load_failed; }

	//OnUpdatedCallback* OnUpdate = nullptr;
protected:
	std::string path;				// filepath or name of asset
	uint64_t load_timestamp = 0;	// os timestamp of when asset was loaded
	bool load_failed = false;		// did the asset fail to load
	bool is_loaded = false;			// is the asset's data in memory
	bool is_referenced = false;		// does the asset have any references
	bool is_from_disk = false;		// otherwise created at runtime
	bool system_asset = false;		// system assets never get deleted

	// types: material, model, texture, anim graph, schema, level, curve, sound, particle, data-table
};

// have a path, want an IAsset
class IAssetLoader 
{
public:
	virtual IAsset* load_asset(const std::string& path) = 0;
};
