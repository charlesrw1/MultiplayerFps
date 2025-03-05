#pragma once
#include "AssetRegistry.h"

struct AssetFilesystemNode {
	bool is_used = true;
	bool folder_is_open = false;
	AssetOnDisk asset;
	std::string name;
	std::unordered_map<std::string, AssetFilesystemNode> children;
	std::vector<AssetFilesystemNode*> sorted_list;
	AssetFilesystemNode() {}
	AssetFilesystemNode(const AssetFilesystemNode& other) {
		folder_is_open = other.folder_is_open;
		asset = other.asset;
		name = other.name;
		children = other.children;
		is_used = other.is_used;
	}
	bool is_folder() const {
		return !children.empty();
	}
	void set_is_used_to_false_R() {
		is_used = false;
		for (auto& c : children)
			c.second.set_is_used_to_false_R();
	}
	void remove_unused_R() {
		std::vector<std::string> deletes;
		for (auto& c : children) {
			if (!c.second.is_used)
				deletes.push_back(c.first);
		}
		for (auto& d : deletes)
			children.erase(d);
		for (auto& c : children)
			c.second.remove_unused_R();
	}
	void set_folder_open_R(bool b) {
		folder_is_open = b;
		for (auto& c : children)
			c.second.set_folder_open_R(b);
	}

	void sort_R();

	void add_path(const AssetOnDisk& a, const std::vector<std::string>& path, size_t index = 0) {
		is_used = true;
		if (index == path.size()) {
			asset = a;
			return;
		}

		const std::string& part = path[index];
		if (children.find(part) == children.end()) {
			children[part].name = part;
		}
		children[part].add_path(a, path, index + 1);
	}
};
