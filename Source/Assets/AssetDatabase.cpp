#include "AssetDatabase.h"
#include <vector>
#include <unordered_map>
#include "Framework/Config.h"
#include <algorithm>
#include "Framework/Files.h"
#include "Framework/MapUtil.h"
#include "Assets/ScriptableObject.h"

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
		asset->load_attempted = true;
		asset->load_failed = false;
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
		if (existing && existing->load_attempted) {
			if (!existing->get_type().is_a(*type)) {
				sys_print(Error, "2 assets with same name but different type: %s\n", str.c_str());
				return nullptr;
			}
			return existing;
		} else {
			// asset doesnt exist
			if (!existing) {
				// ScriptableObject subclasses all share the ".sobj" extension, so the caller's
				// static `type` is only ever ScriptableObject::StaticType (or a specific
				// subclass, e.g. AssetPtr<MyWeaponConfig> fields). The real concrete type lives
				// in the file's "__classname" key and isn't known until we peek it — read it
				// now, before the one-time alloc, and allocate that instead. Every other asset
				// kind is unaffected (peek is a no-op unless type->is_a(ScriptableObject)).
				const ClassTypeInfo* alloc_type = type;
				if (type->is_a(ScriptableObject::StaticType)) {
					const ClassTypeInfo* resolved = ScriptableObject::peek_concrete_type(str);
					if (!resolved) {
						sys_print(Error, "ScriptableObject: %s missing/unknown __classname\n", str.c_str());
						return nullptr;
					}
					if (!resolved->is_a(*type)) {
						sys_print(Error, "ScriptableObject: %s is a '%s', not a '%s'\n", str.c_str(),
								  resolved->classname, type->classname);
						return nullptr;
					}
					alloc_type = resolved;
				}
				existing = (IAsset*)alloc_type->alloc();
				existing->path = str;
				std::shared_ptr<IAsset> sptr(existing);
				allAssets.insert({str, sptr});
			}
			bool success = false;
			try {
				success = existing->load_asset();
			}
			catch (...) {
				sys_print(Error, "load_asset threw for %s\n", str.c_str());
				success = false;
			}
			existing->load_failed = !success;
			existing->load_attempted = true; // set AFTER load_asset (tombstone needs this true so second find returns same instance)
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
		// In-place reload: same instance keeps the same address so anyone holding
		// a raw IAsset* / Texture* / Model* / MaterialInstance* remains valid.
		asset->uninstall();
		asset->load_failed = false;
		// load_attempted stays whatever it was; we set it after load_asset returns.
		bool success = false;
		try {
			success = asset->load_asset();
		}
		catch (...) {
			sys_print(Error, "reload load_asset threw\n");
			success = false;
		}
		asset->load_failed = !success;
		asset->load_attempted = true;
		if (success) {
			try {
				asset->post_load();
			}
			catch (...) {
				sys_print(Error, "reload post_load threw\n");
				asset->load_failed = true;
			}
		}
	}

	void print_assets() {
		sys_print(Info, "%-32s|%-18s|%s|%s\n", "NAME", "TYPE", "F", "MASK");
		std::string usename;
		std::string usetype;
		for (auto& [name, type] : allAssets) { // structured bindings r cool
			if (!type->load_attempted)
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
			if (!type->load_attempted)
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