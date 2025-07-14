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
	
			//if (log_all_asset_loads.get_bool()) {
			//	const double time_since_creation = GetTime() - job->creation_start_time;
			//	sys_print(Debug, "finalize job %s resource %s (%f %f)\n", job->out_object->get_type().classname, job->out_object->get_name().c_str(), float(job->load_time), float(time_since_creation));
			//}
			//assert(allAssets.find(job->path) != allAssets.end());
			//assert(job->out_object);
		
			
			//if (log_all_asset_loads.get_bool() && job->other_assets.size() == job->other_load_times.size()) {
			//	vector<int> nums(job->other_assets.size());
			//	for (int i = 0; i < nums.size(); i++) nums[i] = i;
			//	std::sort(nums.begin(), nums.end(), [&](int l, int r) {
			//		return job->other_load_times[l] > job->other_load_times[r];
			//		});
			//	for (int i = 0; i < job->other_assets.size(); i++) {
			//		auto o = job->other_assets[nums[i]];
			//		double t = job->other_load_times[nums[i]];
			//		sys_print(Debug, "	subasset %s (%f)\n", o->get_name().c_str(), float(t));
			//	}
			//}

			//double pre_post_load = GetTime();
			//vector<std::pair<string, float>> timings;
			//for (auto o : job->other_assets) {
			//	if (o->load_failed) {
			//		sys_print(Error, "AssetDatabase: asset failed to load \"%s\" (type=%s) (FromJob: \"%s\")\n", o->get_name().c_str(),o->get_type().classname, job->out_object->get_name().c_str());
			//	}
			//	if (!o->load_failed && !o->has_run_post_load) {
			//		double now = GetTime();
			//		try {
			//			o->post_load();
			//			o->has_run_post_load = true;
			//		}
			//		catch (...) {
			//			sys_print(Error, "AssetDatabase: asset post load failed \"%s\" (type=%s) (FromJob: \"%s\")\n", o->get_name().c_str(), o->get_type().classname, job->out_object->get_name().c_str());
			//			o->load_failed = true;
			//		}
			//		if (log_all_asset_loads.get_bool()) {
			//			timings.push_back({ o->get_name(),float(GetTime() - now) });
			//		}
			//	}
			//}
			

			//if (log_all_asset_loads.get_bool()) {
			//	std::sort(timings.begin(), timings.end(), [](const std::pair<string, float>& l, const std::pair<string, float>& r) {
			//		return l.second > r.second;
			//		});
			//	for (auto& t : timings) {
			//		sys_print(Debug, "		PostLoad(%s) took %fs\n",t.first.c_str(), t.second);
			//	}
			//	sys_print(Debug, "	took %f to run post_loads\n", float(GetTime() - pre_post_load));
			//}

			//if(job->callback)
			//	job->callback(job->out_object);
			//job = backend.pop_finished_job();
	
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
void AssetDatabase::tick_asyncs() {

	impl->tick_asyncs_standard();
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
void AssetDatabase::reload_async(IAsset* asset, std::function<void(GenericAssetPtr)> callback)
{
	impl->reload_asset_sync(asset);
	callback(asset);
}
void AssetDatabase::install_system_asset(IAsset* assetPtr, const std::string& name)
{
	impl->install_system_direct(assetPtr, name);
}
GenericAssetPtr AssetDatabase::find_sync(const std::string& path, const ClassTypeInfo* classType, bool is_system)
{
	return impl->load_asset_sync(path,classType,is_system);
}
void AssetDatabase::find_async(const std::string& path, const ClassTypeInfo* classType, std::function<void(GenericAssetPtr)> callback, bool is_system)
{
	auto out = impl->load_asset_sync(path, classType, is_system);
	callback(out);
	//impl->load_asset_async(path, classType, is_system, callback);
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
}

IAssetLoadingInterface* AssetDatabase::loader=nullptr;