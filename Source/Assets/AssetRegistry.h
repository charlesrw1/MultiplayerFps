#pragma once
#include <vector>
#include <memory>
#include <unordered_map>

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

	// if false, then asset names wont be treated like filepaths
	virtual bool assets_are_filepaths() const { return true; }
	// override this to add a new tool to the editor, used for maps, models, animations, everything
	virtual IEditorTool* tool_to_edit_me() const { return nullptr; }
	virtual const char* get_arg_for_editortool() const { return ""; }

	// return <AssetName>::StaticType
	virtual const ClassTypeInfo* get_asset_class_type() const { return nullptr; }

	// fills extra assets
	virtual void fill_extra_assets(std::vector<std::string>& out) const {}

	std::vector<std::string> extensions; // "dds" "cmdl" (no period)
	std::string pre_compilied_extension;
	uint32_t self_index = 0;
};

struct AssetOnDisk
{
	AssetMetadata* type = nullptr;
	std::string filename;
};

struct AssetFilesystemNode {
	bool folderIsOpen = true;
	AssetOnDisk asset;
	std::string name;
	std::unordered_map<std::string, AssetFilesystemNode*> children;
	AssetFilesystemNode(std::string name) : name(name) {}

	// Add a path to the tree
	void addPath(const AssetOnDisk& a, const std::vector<std::string>& path, size_t index = 0) {
		if (index == path.size()) {
			asset = a;
			return; // Base case: reached the end of the path
		}

		const std::string& part = path[index];
		if (children.find(part) == children.end()) {
			children[part] = new AssetFilesystemNode(part); // Create a new node if not present
		}
		children[part]->addPath(a,path, index + 1); // Recursively add the rest of the path
	}

	// Destructor to clean up memory
	~AssetFilesystemNode() {
		for (auto& pair : children) {
			delete pair.second;
		}
	}
};

class AssetRegistrySystem
{
public:
	static AssetRegistrySystem& get();

	void init();

	void register_asset_type(AssetMetadata* metadata) {
		metadata->self_index = all_assettypes.size();
		all_assettypes.push_back(std::unique_ptr<AssetMetadata>(metadata));
	}
	void reindex_all_assets();

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
	std::unique_ptr<AssetFilesystemNode> root;
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