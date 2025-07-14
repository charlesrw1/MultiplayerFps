#pragma once
#include "IAsset.h"

#include <functional>
#include <string>
#include "Game/SerializePtrHelpers.h"

struct PendingLoadJob;
using std::string;

class IAssetLoadingInterface
{
public:
	virtual IAsset* load_asset(const ClassTypeInfo* type, string path) = 0;
	virtual void touch_asset(const IAsset* asset) = 0;
};

class AssetDatabaseImpl;


class AssetDatabase
{
public:
	// loading interface for the public. this will do a sync load internally.
	// a different IAssetLoadingInterface is used on the loader thread, but you dont have to care about that.
	static IAssetLoadingInterface* loader;

	void init();
	void quit();

	void reset_testing();

	void clear_used_flags();
	void load_asset_bundle_sync();
	void load_asset_bundle_async();
	void tick_update(float max_time);	// if an async load is happening, updates it. also calls post loads. stays under max_time 


	// update any async resource requests that have finished, executes callbacks, calls post_load (ie to upload GPU resources)
	void tick_asyncs();	
	// wait for everything to finish, then tick_async
	void finish_all_jobs();
	// garbage collection. after marking unrefernces, use IAssetLoadingInterface to touch objects.
	// then remove unreferences will remove the unreferenced stuff (also will traverse asset dependencies to mark those too)
	void mark_unreferences();	// this unreferences objects
	void remove_unreferences();	// this removes unreferences objects

	// appends an IAsset with a path to the global registry
	// will set the lifetime to global until removed
	void install_system_asset(IAsset* assetPtr, const std::string& name);
	void remove_system_reference(IAsset* asset);
	bool is_asset_loaded(const std::string& path);

	// sync asset loading
	template<typename T>
	AssetPtr<T> find_global_sync(const std::string& path) {
		auto res = find_sync(path, &T::StaticType, true);
		return res ? res->cast_to<T>() : nullptr;
	}
	template<typename T>
	AssetPtr<T> find_sync(const std::string& path, bool system_asset=false) {
		auto res = find_sync(path, &T::StaticType, system_asset);
		return res ? res->cast_to<T>() : nullptr;
	}
	GenericAssetPtr find_sync(const std::string& path, const ClassTypeInfo* classType, bool system_asset = false);

	// async asset loading. callback is called in tick_async()
	template<typename T>
	void find_global_async(const std::string& path, std::function<void(GenericAssetPtr)> callback) {
		static_assert(std::is_base_of<IAsset, T>::value, "find_global_async type must derive from IAsset");
		find_async(path, &T::StaticType, std::move(callback), true);
	}
	template<typename T>
	void find_async(const std::string& path, std::function<void(GenericAssetPtr)> callback, bool is_system=false) {
		static_assert(std::is_base_of<IAsset, T>::value, "find_async type must derive from IAsset");
		find_async(path, &T::StaticType, std::move(callback), is_system);
	}
	void find_async(const std::string& path, const ClassTypeInfo* classType, std::function<void(GenericAssetPtr)> callback, bool is_system = false);


	template<typename T>
	void reload_sync(AssetPtr<T> asset) {
		return reload_sync(asset.get_unsafe());
	}
	void reload_sync(IAsset* asset);

	// reloads an asset and all dependent objects, then move constructs them into the original asset
	void reload_async(IAsset* asset, std::function<void(GenericAssetPtr)> callback);

	void print_usage();
	// this creates an asset bundle essentially
	void dump_loaded_assets_to_disk(const std::string& path);

#ifdef EDITOR_BUILD
	void get_assets_of_type(std::vector<IAsset*>& out, const ClassTypeInfo* type);
#endif

	AssetDatabase();
	~AssetDatabase();
private:
	std::unique_ptr<AssetDatabaseImpl> impl;
};

extern AssetDatabase g_assets;

// sync load to the default channel (usually 0)
template<typename T>
T* default_asset_load(const std::string& path) {
	auto res = g_assets.find_sync<T>(path);
	return res.get();
}
template<typename T>
T* find_global_asset_s(const std::string& path) {
	auto res = g_assets.find_global_sync<T>(path);
	return res.get();
}

