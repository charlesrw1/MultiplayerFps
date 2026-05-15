#pragma once
#include "IAsset.h"

#include <string>

using std::string;

class AssetDatabaseImpl;

class AssetDatabase
{
public:
	void init();

	// appends an IAsset with a path to the global registry
	// will set the lifetime to global until removed
	void install_system_asset(IAsset* assetPtr, const std::string& name);
	bool is_asset_loaded(const std::string& path);

	// sync asset loading
	template <typename T> AssetPtr<T> find(const std::string& path) {
		auto res = generic_find(path, &T::StaticType);
		return res ? res->cast_to<T>() : nullptr;
	}
	GenericAssetPtr generic_find(const std::string& path, const ClassTypeInfo* classType);

	template <typename T> std::shared_ptr<T> find_sync_sptr(const string& path) {
		auto ptr = find_sync_sptr(path, &T::StaticType);
		if (ptr && ptr->cast_to<T>()) {

			return std::static_pointer_cast<T>(ptr);
		}
		return nullptr;
	}
	std::shared_ptr<IAsset> find_sync_sptr(const string& path, const ClassTypeInfo* classType);

	template <typename T> void reload(AssetPtr<T> asset) { return reload(asset.get_unsafe()); }
	void reload(IAsset* asset);
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
	auto res = g_assets.find<T>(path);
	return res.get();
}
