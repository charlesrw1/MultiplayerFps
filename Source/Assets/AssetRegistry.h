#pragma once
#include <vector>
#include <memory>
#include "IAsset.h"

// All assets that you want showing in the asset browser should be registered here

class IEditorTool;
class AssetMetadata
{
public:
	// typename to display
	virtual std::string get_type_name() const = 0;
	// color of type in browser
	virtual Color32 get_browser_color() const = 0;
	// append all filepaths/assets
	// if its a filepath, dont append full relative path, use "($root_filepath()) + <appended string>" filepath
	virtual void index_assets(std::vector<std::string>& filepaths) const = 0;
	// return the base filepath for indexed assets, like ./Data/Models
	virtual std::string root_filepath() const = 0;
	// if false, then asset names wont be treated like filepaths
	virtual bool assets_are_filepaths() const { return true; }
	// override this to add a new tool to the editor, used for maps, models, animations, everything
	virtual IEditorTool* tool_to_edit_me() const { return nullptr; }

	// return <AssetName>::StaticType
	virtual const ClassTypeInfo* get_asset_class_type() const { return nullptr; }

	uint32_t self_index = 0;
};

struct AssetOnDisk
{
	AssetMetadata* type = nullptr;
	std::string filename;
	size_t filesize = 0;
};

class AssetRegistrySystem
{
public:
	static AssetRegistrySystem& get();

	void register_asset_type(AssetMetadata* metadata) {
		metadata->self_index = all_assettypes.size();
		all_assettypes.push_back(std::unique_ptr<AssetMetadata>(metadata));
	}
	void reindex_all_assets();
	std::vector<AssetOnDisk>& get_all_assets() { return all_disk_assets; }
	const std::vector<std::unique_ptr<AssetMetadata>>& get_types() { return all_assettypes; }
	const AssetMetadata* find_type(const char* type_name) const {
		for (auto& a : all_assettypes) {
			if (a->get_type_name() == type_name) return a.get();
		}
		return nullptr;
	}
	const AssetMetadata* find_for_classtype(const ClassTypeInfo* ti) {
		for (int i = 0; i < all_assettypes.size(); i++) {
			if (all_assettypes[i]->get_asset_class_type() == ti)
				return all_assettypes[i].get();
		}
		return nullptr;
	}
private:
	size_t last_index_time = 0;
	std::vector<AssetOnDisk> all_disk_assets;
	std::vector<std::unique_ptr<AssetMetadata>> all_assettypes;
};

template<typename T>
struct AutoRegisterAsset
{
	AutoRegisterAsset() {
		AssetRegistrySystem::get().register_asset_type(new T);
	}
};
#define REGISTER_ASSETMETADATA_MACRO(Type) static AutoRegisterAsset<Type> autoregtype##Type;