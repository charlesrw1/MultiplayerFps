#include "AssetDatabase.h"
#include <vector>
#include <unordered_map>
#include "Framework/Config.h"
#include <algorithm>
#include "Framework/Files.h"
#include "Framework/MapUtil.h"

using std::string;
using std::unordered_map;
using std::vector;

ConfigVar log_all_asset_loads("log_all_asset_loads", "0", CVAR_BOOL, "");
class AssetDatabaseImpl
{
public:
	AssetDatabaseImpl() {}
	~AssetDatabaseImpl() {}

	void install_system_direct(IAsset* asset, const std::string& name) {
		asset->path = name;
		asset->is_loaded = true;
		asset->is_from_disk = false;
		std::shared_ptr<IAsset> sptr(asset);
		ASSERT(!MapUtil::contains(allAssets, name));
		allAssets.insert({name, std::move(sptr)});
	}

	std::shared_ptr<IAsset> load_asset_sync_sptr(const std::string& str, const ClassTypeInfo* type) {
		if (str.empty())
			return nullptr;
		auto existing = find_in_all_assets_sptr(str);
		if (existing) {
			return existing;
		}
		load_asset_sync(str, type);
		return find_in_all_assets_sptr(str);
	}

	IAsset* load_asset_sync(const std::string& str, const ClassTypeInfo* type) {
		assert(type->is_a(IAsset::StaticType));
		if (str.empty())
			return nullptr;

		auto existing = find_in_all_assets(str);
		if (existing && existing->is_loaded) {
			if (!existing->get_type().is_a(*type)) {
				sys_print(Error, "2 assets with same name but different type: %s\n", str.c_str());
				return nullptr;
			}
			return existing;
		} else {
			// asset doesnt exist
			if (!existing) {
				existing = (IAsset*)type->alloc();
				existing->path = str;
				std::shared_ptr<IAsset> sptr(existing);
				allAssets.insert({str, sptr});
			}
			existing->is_loaded = true;
			bool success = existing->load_asset();
			existing->load_failed = !success;
			if (success) {
				try {
					existing->post_load();
				}
				catch (...) {
					sys_print(Error, "post load failed\n");
					existing->load_failed = true;
				}
			}

			assert(find_in_all_assets(str) == existing);
			return existing;
		}
	}
	void reload_asset_sync(IAsset* asset) {
		if (!asset)
			return;
		auto asset2 = (IAsset*)asset->get_type().alloc();
		asset2->path = asset->path;
		asset2->is_loaded = true;
		bool success = asset2->load_asset();
		asset2->load_failed = !success;
		if (success) {
			try {
				//	asset2->post_load();
				asset->move_construct(asset2);
				asset->post_load();
				ASSERT(asset->get_is_loaded());
			}
			catch (...) {
				sys_print(Error, "post load reload failed\n");
			}
		}
		delete asset2;
		asset2 = nullptr;
	}

	void print_assets() {
		sys_print(Info, "%-32s|%-18s|%s|%s\n", "NAME", "TYPE", "F", "MASK");
		std::string usename;
		std::string usetype;
		for (auto& [name, type] : allAssets) { // structured bindings r cool
			if (!type->is_loaded)
				continue;
			usename = name;
			if (usename.size() > 32) {
				usename = usename.substr(0, 29) + "...";
			}
			usetype = type->get_type().classname;
			if (usetype.size() > 18) {
				usetype = usetype.substr(0, 15) + "...";
			}

			sys_print(Info, "%-32s|%-18s|%s|%s\n", usename.c_str(), usetype.c_str(), (type->load_failed) ? "X" : " ",
					  "");
		}
	}
	void dump_to_file(IFile* file) {
		auto get_i = [](IAsset* a) -> int {
			if (strcmp(a->get_type().classname, "Texture") == 0)
				return 0;
			else if (strcmp(a->get_type().classname, "MaterialInstance") == 0)
				return 1;
			else if (strcmp(a->get_type().classname, "Model") == 0)
				return 2;
			return 3;
		};
		std::vector<std::pair<IAsset*, int>> list;
		for (auto& [name, type] : allAssets) { // structured bindings r cool
			if (!type->is_loaded)
				continue;
			list.push_back({type.get(), get_i(type.get())});
		}
		// order them slightly
		std::sort(list.begin(), list.end(),
				  [](const std::pair<IAsset*, int>& a, const std::pair<IAsset*, int>& b) -> bool {
					  return a.second < b.second;
				  });
		for (auto& a : list) {
			string line = a.first->get_type().classname + string(" ") + a.first->get_name() + "\n";
			file->write(line.data(), line.size());
		}
	}

	bool is_asset_loaded(const string& path) { return MapUtil::contains(allAssets, path); }
	void get_assets_of_type(std::vector<IAsset*>& out, const ClassTypeInfo* type) {
		for (auto& [path, ptr] : allAssets) {
			if (ptr->get_type().is_a(*type))
				out.push_back(ptr.get());
		}
	}

private:
	IAsset* find_in_all_assets(const string& str) {
		auto f = allAssets.find(str);
		return f == allAssets.end() ? nullptr : f->second.get();
	}
	std::shared_ptr<IAsset> find_in_all_assets_sptr(const string& str) {
		auto f = allAssets.find(str);
		return f == allAssets.end() ? nullptr : f->second;
	}

	// maps a path to a loaded asset
	// this doesnt need a mutex to read
	unordered_map<string, std::shared_ptr<IAsset>> allAssets;
};

// reloading: actually allow multiple in memory? then old copy gets GCed.

// when an object references a

AssetDatabase::AssetDatabase() {}
AssetDatabase::~AssetDatabase() {}
AssetDatabase g_assets;

void AssetDatabase::init() {
	impl = new AssetDatabaseImpl; // dont make it a uptr because blah blah
}
bool AssetDatabase::is_asset_loaded(const std::string& path) {
	return impl->is_asset_loaded(path);
}
std::shared_ptr<IAsset> AssetDatabase::find_sync_sptr(const string& path, const ClassTypeInfo* classType) {
	return impl->load_asset_sync_sptr(path, classType);
}
void AssetDatabase::reload(IAsset* asset) {
	impl->reload_asset_sync(asset);
}

void AssetDatabase::install_system_asset(IAsset* assetPtr, const std::string& name) {
	impl->install_system_direct(assetPtr, name);
}
GenericAssetPtr AssetDatabase::generic_find(const std::string& path, const ClassTypeInfo* classType) {
	return impl->load_asset_sync(path, classType);
}

void AssetDatabase::print_usage() {
	impl->print_assets();
}
void AssetDatabase::dump_loaded_assets_to_disk(const std::string& path) {
	sys_print(Info, "AssetDatabase::dump_loaded_assets_to_disk: %s\n", path.c_str());
	auto file = FileSys::open_write_game(path);
	if (!file) {
		sys_print(Error, "AssetDatabase::dump_loaded_assets_to_disk: path couldn't open %s\n", path.c_str());
	}
	impl->dump_to_file(file.get());
}

void AssetDatabase::get_assets_of_type(std::vector<IAsset*>& out, const ClassTypeInfo* type) {
	impl->get_assets_of_type(out, type);
}