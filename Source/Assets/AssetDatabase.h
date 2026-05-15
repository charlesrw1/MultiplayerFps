#pragma once
#include "IAsset.h"

#include <string>

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
	static IAssetLoadingInterface* loader;

	void init();

	// appends an IAsset with a path to the global registry
	// will set the lifetime to global until removed
	void install_system_asset(IAsset* assetPtr, const std::string& name);
	bool is_asset_loaded(const std::string& path);

	// sync asset loading
	template <typename T> AssetPtr<T> find_global_sync(const std::string& path) {
		auto res = find_sync(path, &T::StaticType, true);
		return res ? res->cast_to<T>() : nullptr;
	}
	template <typename T> AssetPtr<T> find_sync(const std::string& path, bool system_asset = false) {
		auto res = find_sync(path, &T::StaticType, system_asset);
		return res ? res->cast_to<T>() : nullptr;
	}
	GenericAssetPtr find_sync(const std::string& path, const ClassTypeInfo* classType, bool system_asset = false);

	template <typename T> std::shared_ptr<T> find_sync_sptr(const string& path, bool system_asset = false) {
		auto ptr = find_sync_sptr(path, &T::StaticType, system_asset);
		if (ptr && ptr->cast_to<T>()) {

			return std::static_pointer_cast<T>(ptr);
		}
		return nullptr;
	}
	std::shared_ptr<IAsset> find_sync_sptr(const string& path, const ClassTypeInfo* classType,
										   bool system_asset = false);

	template <typename T> void reload_sync(AssetPtr<T> asset) { return reload_sync(asset.get_unsafe()); }
	void reload_sync(IAsset* asset);
	void print_usage();
	// this creates an asset bundle essentially
	void dump_loaded_assets_to_disk(const std::string& path);

	void get_assets_of_type(std::vector<IAsset*>& out, const ClassTypeInfo* type);

	AssetDatabase();
	~AssetDatabase();

private:
	AssetDatabaseImpl* impl = nullptr;
};

extern AssetDatabase g_assets;

// sync load to the default channel (usually 0)
template <typename T> T* default_asset_load(const std::string& path) {
	auto res = g_assets.find_sync<T>(path);
	return res.get();
}
