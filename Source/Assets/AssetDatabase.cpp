#include "AssetDatabase.h"
#include <vector>
#include <unordered_map>
#include <thread>
#include "Framework/Config.h"
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>

#include "Framework/Hashset.h"
#include "Framework/Hashmap.h"
#include "Framework/Files.h"
#include "Framework/MapUtil.h"

using std::string;
using std::unordered_map;
using std::vector;
using std::function;
template<typename T>
using uptr = std::unique_ptr<T>;


ConfigVar log_all_asset_loads("log_all_asset_loads", "1", CVAR_BOOL, "");
ConfigVar log_finish_job_func("log_finish_job_func", "0", CVAR_BOOL, "");
void AssetDatabase::quit() {

}
class AssetDatabaseImpl
{
public:
	
	AssetDatabaseImpl() {
	
	}
	~AssetDatabaseImpl() {

	}


	void install_system_direct(IAsset* asset, const std::string& name) {
		asset->path = name;
		asset->persistent_flag = true;
		asset->is_loaded = true;
		asset->is_from_disk = false;
		allAssets.insert({ name, asset });
	}

	void tick_asyncs_standard() {
	}

	IAsset* load_asset_sync(const std::string& str, const ClassTypeInfo* type, bool is_system)
	{
		assert(type->is_a(IAsset::StaticType));
		if (str.empty())
			return nullptr;

		auto existing = find_in_all_assets(str);
		if (existing && existing->is_loaded) {
			if (!existing->get_type().is_a(*type)) {
				sys_print(Error, "2 assets with same name but different type: %s\n", str.c_str());
				return nullptr;
			}
			existing->persistent_flag |= is_system;
			return existing;
		}
		else {
			// asset doesnt exist
			if (!existing) {
				existing = (IAsset*)type->alloc();
				existing->path = str;
				allAssets.insert({ str,existing });
			}
			existing->persistent_flag |= is_system;
			existing->is_loaded = true;
			bool success = existing->load_asset(g_assets.loader);
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

			assert(find_in_all_assets(str)==existing);
			return existing;
		}
	}
	void reload_asset_sync(IAsset* asset) {
		if (!asset)
			return;
		auto asset2 = (IAsset*)asset->get_type().alloc();
		asset2->path = asset->path;
		asset2->is_loaded = true;
		bool success = asset2->load_asset(g_assets.loader);
		asset2->load_failed = !success;
		if (success) {
			try {
			//	asset2->post_load();
				asset->move_construct(asset2);
				asset->post_load();
			}
			catch (...) {
				sys_print(Error, "post load reload failed\n");
			}
		}
		delete asset2;
		asset2 = nullptr;
	}



	void print_assets() {
		sys_print(Info, "%-32s|%-18s|%s|%s\n","NAME", "TYPE", "F", "MASK");
		std::string usename;
		std::string usetype;
		for (auto& [name,type] : allAssets) {	// structured bindings r cool
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

			sys_print(Info, "%-32s|%-18s|%s|%s\n", usename.c_str(), usetype.c_str(), (type->load_failed) ? "X" : " ", "");
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
		std::vector<std::pair<IAsset*,int>> list;
		for (auto& [name, type] : allAssets) {	// structured bindings r cool
			if (!type->is_loaded)
				continue;
			list.push_back({ type,get_i(type) });
		}
		// order them slightly
		std::sort(list.begin(), list.end(), [](const std::pair<IAsset*, int>& a, const std::pair<IAsset*, int>& b) -> bool {
			return a.second < b.second;
			});
		for (auto& a : list) {
			string line = a.first->get_type().classname + string(" ") + a.first->get_name() + "\n";
			file->write(line.data(), line.size());
		}
	}

	void quit() {
		sys_print(Info, "quitting asset loader\n");
		//backend.signal_end_work();
	}
	bool is_asset_loaded(const string& path) {
		return MapUtil::contains(allAssets, path);
	}
	void get_assets_of_type(std::vector<IAsset*>& out, const ClassTypeInfo* type) {
		for (auto& [path, ptr] : allAssets) {
			if (ptr->get_type().is_a(*type))
				out.push_back(ptr);
		}
	}
private:

	IAsset* find_in_all_assets(const string& str) {
		auto f= allAssets.find(str);
		return f == allAssets.end() ? nullptr : f->second;
	}

	// maps a path to a loaded asset
	// this doesnt need a mutex to read
	unordered_map<string, IAsset*> allAssets;

};

// reloading: actually allow multiple in memory? then old copy gets GCed. 

// when an object references a 




AssetDatabase::AssetDatabase() {


}
AssetDatabase::~AssetDatabase() {}
AssetDatabase g_assets;

class PrimaryAssetLoadingInterface : public IAssetLoadingInterface
{
public:
	PrimaryAssetLoadingInterface(AssetDatabaseImpl& frontend);
	IAsset* load_asset(const ClassTypeInfo* type, string path) override;
	void touch_asset(const IAsset* asset) override;
private:
	AssetDatabaseImpl& impl;
};



void AssetDatabase::init() {
	// init the loader thread
	impl = std::make_unique<AssetDatabaseImpl>();
	AssetDatabase::loader = new PrimaryAssetLoadingInterface(*impl);
}
void AssetDatabase::reset_testing()
{

}
void AssetDatabase::finish_all_jobs()
{
	//impl->finish_all_jobs();
}

void AssetDatabase::remove_system_reference(IAsset* asset)
{
	//asset->is_system = false;
	//impl->remove_asset_direct(asset);
}
bool AssetDatabase::is_asset_loaded(const std::string& path)
{
	return impl->is_asset_loaded(path);
}
void AssetDatabase::mark_unreferences()
{
	//impl->mark_assets_as_unreferenced();
}
void AssetDatabase::reload_sync(IAsset* asset)
{
	impl->reload_asset_sync(asset);
}

void AssetDatabase::install_system_asset(IAsset* assetPtr, const std::string& name)
{
	impl->install_system_direct(assetPtr, name);
}
GenericAssetPtr AssetDatabase::find_sync(const std::string& path, const ClassTypeInfo* classType, bool is_system)
{
	return impl->load_asset_sync(path,classType,is_system);
}


void AssetDatabase::remove_unreferences()
{
	//impl->uninstall_unreferenced_assets();
}
void AssetDatabase::print_usage()
{
	impl->print_assets();
}
void AssetDatabase::dump_loaded_assets_to_disk(const std::string& path)
{
	sys_print(Info, "AssetDatabase::dump_loaded_assets_to_disk: %s\n", path.c_str());
	auto file = FileSys::open_write_game(path);
	if (!file) {
		sys_print(Error, "AssetDatabase::dump_loaded_assets_to_disk: path couldn't open %s\n", path.c_str());
	}
	impl->dump_to_file(file.get());
}
void AssetDatabase::get_assets_of_type(std::vector<IAsset*>& out, const ClassTypeInfo* type)
{
	impl->get_assets_of_type(out, type);
}
PrimaryAssetLoadingInterface::PrimaryAssetLoadingInterface(AssetDatabaseImpl& frontend) : impl(frontend) {
}
IAsset* PrimaryAssetLoadingInterface::load_asset(const ClassTypeInfo* type, string path)
{
	return impl.load_asset_sync(path, type, false);
}
void PrimaryAssetLoadingInterface::touch_asset(const IAsset* asset)
{
	assert(0);
	std::shared_ptr<IAsset> ptr;

}

IAssetLoadingInterface* AssetDatabase::loader=nullptr;