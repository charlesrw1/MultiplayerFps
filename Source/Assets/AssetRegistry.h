#pragma once
#ifdef EDITOR_BUILD
#include <vector>
#include <memory>
#include <unordered_map>
#include "Framework/ConsoleCmdGroup.h"
#include "IAsset.h"
#include "EngineEditorState.h"

// All assets that you want showing in the asset browser should be registered here


class IEditorTool;
class AssetMetadata
{
public:
	// typename to display
	virtual std::string get_type_name() const = 0;
	// color of type in browser
	virtual Color32 get_browser_color() const = 0;

	// return <AssetName>::StaticType
	virtual const ClassTypeInfo* get_asset_class_type() const { return nullptr; }

	virtual uptr<CreateEditorAsync> create_create_tool_to_edit(opt<string> assetPath) const { return nullptr; }

	virtual void draw_browser_menu(const string& assetPath) const {

	}

	// fills extra assets
	virtual void fill_extra_assets(std::vector<std::string>& out) const {}

	std::vector<std::string> extensions; // "dds" "cmdl" (no period)
	uint32_t self_index = 0;
};

struct AssetOnDisk
{
	AssetMetadata* type = nullptr;
	std::string filename;
};

struct AssetFilesystemNode;
class HackedAsyncAssetRegReindex;
class AssetRegistrySystem
{
public:
	static AssetRegistrySystem& get();

	void init();

	void update();

	void register_asset_type(AssetMetadata* metadata) {
		metadata->self_index = all_assettypes.size();
		all_assettypes.push_back(std::unique_ptr<AssetMetadata>(metadata));
	}

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

	AssetFilesystemNode* get_root_files() const { return root.get(); }

	const ClassTypeInfo* find_asset_type_for_ext(const std::string& ext);
private:
	uptr<ConsoleCmdGroup> consoleCommands;
	void reindex_all_assets();
	std::unique_ptr<AssetFilesystemNode> root;
	std::vector<std::unique_ptr<AssetMetadata>> all_assettypes;
	double last_reindex_time = 0.f;
	int64_t last_time_check = 0;
	friend class HackedAsyncAssetRegReindex;
};

template<typename T>
struct AutoRegisterAsset
{
	AutoRegisterAsset() {
		AssetRegistrySystem::get().register_asset_type(new T);
	}
};
#define REGISTER_ASSETMETADATA_MACRO(Type) static AutoRegisterAsset<Type> autoregtype##Type;

#endif