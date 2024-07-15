#pragma once

#include "IAsset.h"
#include <unordered_map>


class AssetLoaderRegistry
{
public:
	static AssetLoaderRegistry& get() {
		static AssetLoaderRegistry inst;
		return inst;
	}
	void register_type(const std::string& typename_, IAssetLoader* theloader) {
		assert(asset_type_to_loader.find(typename_) == asset_type_to_loader.end());
		asset_type_to_loader.insert({ typename_,theloader });
	}
	IAssetLoader* get_loader_for_type_name(const std::string& typename_) {
		auto find = asset_type_to_loader.find(typename_);
		return (find == asset_type_to_loader.end()) ? nullptr : find->second;
	}
private:
	std::unordered_map<std::string, IAssetLoader*> asset_type_to_loader;
};

struct AutoRegisterAssetLoader
{
	AutoRegisterAssetLoader(const ClassTypeInfo* type, IAssetLoader* loader) {
		AssetLoaderRegistry::get().register_type(type->classname, loader);
	}
};
#define REGISTERASSETLOADER_MACRO(type, asset_loader) static AutoRegisterAssetLoader assetregload##type(&type::StaticType, asset_loader)
