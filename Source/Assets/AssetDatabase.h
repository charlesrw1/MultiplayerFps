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

// ASSET EROR HANDLING:
// -when you request a resource pointer from a path, it wall always return something (with the IAsset* having its path set to the requested path)
//	if the load failed, you will get a bool in asset->get_failed() and the asset will return something "default" (likely just default constructed, but asset types can override this)
// -AssetPtr wraps an IAsset*, and its operator* and operator-> will actually treat failed loads like nullptrs but with
//	the benefit of memory saftey for resources that might be reloaded in editor builds (and have their reloads fail)
// -since resources are identified with both a path and a type, there is a small edge case where you request a resource thats already been mapped to
//	a different typed resource, in this case, it WILL return nullptr. However you will never have pointers trashed under you since that path never mapped to something
//	valid in the programs lifetime in the first place
// -each game system might have different ways of handling failed assets at runtime, 
//	maybe keeping it empty, or remapping to a default asset for debugging
// -For gameplay usage, checking AssetPtrs for validity has the same appearence as checking for nullptr's (just dont use get_unsafe())

// ASSET LOADING:
// -sync and async loading. only 1 asset can be loading at a time. Thus, a call to load sync will stall the pipeline until
//	no more assets are loading. ie if you have an async load going, and you call to sync load an asset, then the sync call will block
//	till the async call on the loader thread finishes (it will also flush the finished queue, calling everything that hasn't post_loaded()'d yet)
// -sync loading assets on async threads DOES WORK! it takes a different path, it will return a loaded asset that hasnt had post_load called


class AssetDatabaseImpl;
class PrimaryAssetLoadingInterface : public IAssetLoadingInterface
{
public:
	PrimaryAssetLoadingInterface(AssetDatabaseImpl& frontend);
	IAsset* load_asset(const ClassTypeInfo* type, string path) override;
	void touch_asset(const IAsset* asset) override;
private:
	AssetDatabaseImpl& impl;
};

class AssetDatabase
{
public:
	static IAssetLoadingInterface* loader;

	PrimaryAssetLoadingInterface get_interface();

	void init();

	// update any async resource requests that have finished, executes callbacks, calls post_load (ie to upload GPU resources)
	void tick_asyncs();	

	// wait for everything to finish, then tick_async
	void finish_all_jobs();

	// triggers uninstall's of assets that arent referenced anymore
	// call after unreference_this_channel() (or at any time, this causes a sweep over assets to check if they should be removed)
	void remove_unreferences();

	// appends an IAsset with a path to the global registry
	// will set the lifetime to global
	void install_system_asset(IAsset* assetPtr, const std::string& name);

	// (public/private) SYNC asset loading
	// This can be used to publically start a load job and privatley in a nested load job
	// If used in private, the lifetime_channel is unused and defaults to the high level load job's lifetime
	// The asset is ready to use by the time this function returns, but isn't guaranteed to have post_load called
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


	// (public) ASYNC asset loading
	// Use to kick off a high level loading job
	// The callback is called on the main thread inside tick_asyncs when finished
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



	// (public) reload an already loaded asset either sync or async
	// doesnt reload any sub-assets
	template<typename T>
	void reload_sync(AssetPtr<T> asset) {
		return reload_sync(asset.get_unsafe());
	}
	void reload_sync(IAsset* asset);
	void reload_async(IAsset* asset, std::function<void(GenericAssetPtr)> callback);

	// explicitly delete an asset, very unsafe, use for levels
	template<typename T>
	void explicit_asset_free(T*& asset) {
		IAsset* i = asset;
		explicit_asset_free(i);
		asset = nullptr;
	}
	void explicit_asset_free(IAsset*& asset);

#ifdef EDITOR_BUILD
	// checks for out of date assets and reloads them async
	void hot_reload_assets();
#endif

	void print_usage();
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

