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

using std::string;
using std::unordered_map;
using std::vector;
using std::function;
template<typename T>
using uptr = std::unique_ptr<T>;

struct AsyncQueuedJob
{
	void validate() {
		assert(is_hot_reload || (!path.empty()&&info));	// either has path or is hot reload
		assert(!(is_hot_reload && force_reload));// cant be both hot reload and force reload
	}

	const ClassTypeInfo* info = nullptr;
	string path;
	bool is_system_asset = false;
	bool force_reload = false;
	bool is_hot_reload = false;
	std::function<void(GenericAssetPtr)> callback;

	// output
	IAsset* out_object = nullptr;
	vector<IAsset*> other_assets;
};

class SubAssetLoadingInterface : public IAssetLoadingInterface
{
public:
	SubAssetLoadingInterface(AssetBackend& backend, AsyncQueuedJob* job) 
		: backend(backend),whatjob(job) {}
	IAsset* load_asset(const ClassTypeInfo* type, string path) final;
	void touch_asset(const IAsset* asset) final {

	}
private:
	AssetBackend& backend;
	AsyncQueuedJob* whatjob = nullptr;
};


class AssetBackend
{
public:
	AssetBackend(const unordered_map<string,IAsset*>& global) 
		: my_thread([this]() { loader_loop();}), global_assets(global)
	{
		my_thread.detach();
	}
	void push_job_to_queue(uptr<AsyncQueuedJob> job);
	void combine_asset_tables(unordered_map<string, IAsset*>& table);
	uptr<AsyncQueuedJob> pop_finished_job();
	void finish_all_jobs();
private:
	void loader_loop();
	void execute_job(AsyncQueuedJob* job);
	IAsset* find_or_create_and_load_asset(const string& path, const ClassTypeInfo* type, bool is_system, AsyncQueuedJob* job);

	// doesnt create
	IAsset* find_asset_in_local_or_global(const string& path) {
		auto f = local_asset_loads.find(path);
		if (f != local_asset_loads.end())
			return f->second;	
		auto f2 = global_assets.find(path);
		if (f2 != global_assets.end())
			return f2->second;
		return nullptr;
	}
	IAsset* create_asset(const string& path, const ClassTypeInfo* info) {
		auto a = info->allocate();
		assert(a->is_a<IAsset>());
		IAsset* as = (IAsset*)a;
		as->path = path;

#ifdef EDITOR_BUILD
		auto f = FileSys::open_read_game(path);
		if (f)
			as->asset_load_time = f->get_timestamp();
#endif

		local_asset_loads.insert({ path,as});
		return as;
	}

	bool is_in_job = false;
	bool stop_processing = false;
	vector<uptr<AsyncQueuedJob>> jobs;
	vector<uptr<AsyncQueuedJob>> finished_jobs;
	std::mutex job_mutex;
	std::condition_variable job_cv;
	std::thread my_thread;
	unordered_map<string, IAsset*> local_asset_loads;	// maintained during loading, then data is swapped to global assets in sync
	const unordered_map<string, IAsset*>& global_assets;
	std::mutex table_mutex;

	friend class SubAssetLoadingInterface;
};
IAsset* SubAssetLoadingInterface::load_asset(const ClassTypeInfo* type, string path)
{
	return backend.find_or_create_and_load_asset(path, type, false,whatjob);
}
IAsset* AssetBackend::find_or_create_and_load_asset(const string& path, const ClassTypeInfo* info, bool is_system, AsyncQueuedJob* job)
{
	IAsset* asset = find_asset_in_local_or_global(path);
	if (!asset)
	{
		asset = create_asset(path, info);
		SubAssetLoadingInterface loadinterface(*this,job);
		const bool success = asset->load_asset(&loadinterface);
		asset->is_system = is_system;
		asset->load_failed = !success;
		asset->is_loaded = true;

		job->other_assets.push_back(asset);
	}
	assert(asset->is_loaded);
	return asset;
}
uptr<AsyncQueuedJob> AssetBackend::pop_finished_job()
{
	std::lock_guard<std::mutex> lock(job_mutex);
	if (finished_jobs.empty()) 
		return nullptr;
	auto j = std::move(finished_jobs.back());
	finished_jobs.pop_back();
	return j;
}
void AssetBackend::finish_all_jobs()
{
	sys_print(Info, "start finish all jobs\n");
	std::unique_lock<std::mutex> lock(job_mutex);
	job_cv.wait(lock, [&] { return jobs.empty() && !is_in_job; });
	sys_print(Info, "/finish all jobs\n");

}

void AssetBackend::combine_asset_tables(unordered_map<string, IAsset*>& table) {
	assert(&table == &global_assets);
	std::lock_guard<std::mutex> lock(table_mutex);
	for (auto& a : local_asset_loads) {
		if (table.find(a.first) == table.end())
			table.insert(a);
	}
	local_asset_loads.clear();
}
void AssetBackend::loader_loop()
{
	for (;;) {
		std::unique_lock<std::mutex> lock(job_mutex);
		job_cv.wait(lock, [this] { return !jobs.empty() || stop_processing; });
		if (stop_processing)
			break;
		uptr<AsyncQueuedJob> item = std::move(jobs.back());
		is_in_job = true;
		table_mutex.lock();
		jobs.pop_back();
		lock.unlock();  // Unlock before processing work
		execute_job(item.get());
		lock.lock();
		table_mutex.unlock();
		is_in_job = false;
		finished_jobs.push_back(std::move(item));
		job_cv.notify_all();	// notify if waiting for all to finish
	}
}
void AssetBackend::push_job_to_queue(uptr<AsyncQueuedJob> job)
{
	std::lock_guard<std::mutex> lock(job_mutex);
	jobs.push_back(std::move(job));
	job_cv.notify_one();
}
void AssetBackend::execute_job(AsyncQueuedJob* job)
{
	job->validate();

	if (job->is_hot_reload)
	{
		assert(0);
	}
	else if (job->force_reload)
	{
		assert(0);
	}
	else
	{
		job->out_object = find_or_create_and_load_asset(job->path, job->info, job->is_system_asset,job);
	}

	// input=path
	// 
	// if hot_reload_request:
	//		scans the hard disk for changes and then pushes reload jobs, done here just cuz
	// 
	// elif not_reload:
	//		if asset in local_loads or in global loads: # asset already got loaded
	//			skip
	//		asset = create() # puts it in local table
	//		asset->load(path)	
	//		add to finish queue
	// else: # asset is a reload
	//		if path in local loads:
	//			return	# already got reloaded, waiting
	//		const org_asset = find in global
	//		asset = create() # put in local table
	//		asset->load()
	//		reload dependents, dont return until all are reloaded and put in local table
	// 
	// case 1: input=path
	// case 2: 
#if 0
	auto init_new_job = [](
		IAsset* asset,
		IAsset* reloadAsset,
		ClassBase* userPtr,
		std::function<void(GenericAssetPtr)>* callback,
		bool skipPostLoad
		) -> LoadJob*
	{
		LoadJob* newJob = new LoadJob;
		newJob->thisAsset = asset;
		newJob->moveIntoThis = reloadAsset;
		newJob->userPtr = userPtr;
		newJob->loadJobCallback = callback;
		newJob->skipPostLoad = skipPostLoad;

		return newJob;
	};


#ifdef EDITOR_BUILD
	if (is_hot_reload) {
		auto& path = asset->path;
		auto f = FileSys::open_read_game(path);
		if (!f) {
			return;
		}
		auto timestamp = f->get_timestamp();
		f->close();
		bool should_reload = asset->asset_load_time < timestamp;
		should_reload |= asset->check_import_files_for_out_of_data();

		if (!should_reload) {
			return;
		}
	}
#endif
	SubAssetLoadingInterface loadinterface(*this);

	if (!asset->is_loaded || force_reload) { /*  can use loaded_internal here, this is the most recent variable and its only written/read under the work lock*/
		ClassBase* userStruct = nullptr;
		IAsset* copiedAsset = nullptr;
		LoadJob* job = nullptr;
		if (force_reload) {

			copiedAsset = (IAsset*)asset->get_type().allocate();
			copiedAsset->path = asset->path;
			copiedAsset->load_failed = false;
			//copiedAsset->set_both_reference_bitmasks_unsafe(0);
			copiedAsset->is_loaded = false;
			copiedAsset->has_run_post_load = false;
		}
		auto asset_to_load = (force_reload) ? copiedAsset : asset;
#ifdef EDITOR_BUILD
		{
			auto f = FileSys::open_read_game(asset_to_load->path);
			if (f)
				asset_to_load->asset_load_time = f->get_timestamp();
		}
#endif
		asset_to_load->load_failed = !asset_to_load->load_asset(&loadinterface) /* not success */;
		if (asset_to_load->load_failed) {
			sys_print(Error, "failed to load %s asset %s\n", asset_to_load->get_type().classname, asset_to_load->path.c_str());
		}
		//asset_to_load->reference_bitmask_internal = (force_reload) ? asset->reference_bitmask_internal : reference_mask;
		//job = init_new_job(asset_to_load, (force_reload) ? asset : nullptr, userStruct, loadJobCallback, false);
		//return job;

	}
	else if (!asset->is_mask_refererenced(reference_mask)) {
		asset->sweep_references();
		asset->reference_bitmask_internal |= reference_mask;

		auto job = init_new_job(asset, nullptr, nullptr, loadJobCallback, true);
		return job;
	}
	else {
		// asset is loaded and is referenced
		// (do nothing)
		if (loadJobCallback) {
			auto job = init_new_job(asset, nullptr, nullptr, loadJobCallback, true);
			return job;
		}
	}
	return nullptr;
#endif
}


class AssetDatabaseImpl
{
public:
	AssetDatabaseImpl() : backend(allAssets) {
	}
	~AssetDatabaseImpl() {

	}

	void install_system_direct(IAsset* asset, const std::string& name) {
		asset->path = name;
		asset->is_system = true;
		asset->is_loaded = true;
		asset->has_run_post_load = true;
		asset->is_from_disk = false;
		allAssets.insert({ name, asset });
	}

	void remove_asset_direct(IAsset* asset) {
		finish_all_jobs();
		allAssets.erase(asset->path);
	}

	void tick_asyncs_standard() {
		
#if 0

		auto finalize_job_with_main_thread = [this](LoadJob* job) {
#ifdef _DEBUG
#endif
			const bool is_reload = job->thisAsset && job->moveIntoThis;

			if (!job->skipPostLoad) {
				if (job->moveIntoThis) {
					job->moveIntoThis->move_construct(job->thisAsset);
					job->moveIntoThis->load_failed = job->thisAsset->load_failed;
#ifdef EDITOR_BUILD
					job->moveIntoThis->asset_load_time = job->thisAsset->asset_load_time;
#endif
					delete job->thisAsset;
					job->thisAsset = job->moveIntoThis;
				}
				ASSERT(job->thisAsset);
				ASSERT(job->thisAsset->is_loaded);
				job->thisAsset->post_load(job->userPtr);
				job->thisAsset->has_run_post_load = true; // thread safe!
				if (job->userPtr)
					delete job->userPtr;
			}
			job->thisAsset->move_internal_to_threadsafe_bitmask_unsafe();
			if (job->loadJobCallback) {
				(*job->loadJobCallback)(job->thisAsset);
				delete job->loadJobCallback;
			}

#ifdef EDITOR_BUILD
			// now for a sucky bit: if theres dependencies, they have to be reloaded sync otherwise stale pointers etc
			if (is_reload) {
				auto reloads =std::move(job->thisAsset->reload_dependents);
				job->thisAsset->reload_dependents.clear();
				for (auto d : reloads)
					reload_asset_sync(d);
			}
#endif


			delete job;
			job = nullptr;
		};
#endif

		backend.combine_asset_tables(allAssets);
		uptr<AsyncQueuedJob> job = backend.pop_finished_job();
		while (job) {
			sys_print(Debug, "finalize job %s resource %s\n", job->out_object->get_type().classname, job->out_object->get_name().c_str());
			assert(allAssets.find(job->path) != allAssets.end());
			assert(job->out_object);
			for (auto o : job->other_assets) {
				if (!o->load_failed && !o->has_run_post_load) {
					o->post_load();
					o->has_run_post_load = true;
				}
			}
			if(job->callback)
				job->callback(job->out_object);
			job = backend.pop_finished_job();
		}
	}

	
	void load_asset_async(const std::string& str, const ClassTypeInfo* type, bool is_system, std::function<void(GenericAssetPtr)>& func) {
		auto existing = find_in_all_assets(str);
		if (existing) {
			assert(existing->get_type().is_a(*type));
			existing->is_system |= is_system;
			func(existing);
		}
		else {
			uptr<AsyncQueuedJob> job = std::make_unique<AsyncQueuedJob>();
			job->path = str;
			job->info = type;
			job->is_system_asset = true;
			job->callback = std::move(func);
			job->validate();
			backend.push_job_to_queue(std::move(job));
		}
	}
	IAsset* load_asset_sync(const std::string& str, const ClassTypeInfo* type, bool is_system)
	{
		auto existing = find_in_all_assets(str);
		if (existing) {
			assert(existing->get_type().is_a(*type));
			existing->is_system |= is_system;
			return existing;
		}
		else {
			uptr<AsyncQueuedJob> job = std::make_unique<AsyncQueuedJob>();
			job->path = str;
			job->info = type;
			job->is_system_asset = true;
			job->validate();
			backend.push_job_to_queue(std::move(job));
			backend.finish_all_jobs();
			backend.combine_asset_tables(allAssets);
			// guaranteed that job is finished by now
			auto a = find_in_all_assets(str);
			assert(a);
			return a;
		}
	}

	
	void uninstall_unreferenced_assets()
	{
		return;
#if  0


		finish_all_jobs();
		ASSERT(!is_in_job && finishedAsyncJobs.empty() && pendingAsyncJobs.empty());

		for (auto& asset : allAssets)
		{
			if (asset.second->reference_bitmask_threadsafe == 0 && asset.second->is_loaded) {

#ifdef _DEBUG
			sys_print(Debug,"uninstalling %s resource %s\n", asset.second->get_type().classname, asset.second->get_name().c_str());
#endif
				asset.second->uninstall();
				asset.second->set_not_loaded_main_thread();
			}
		}
#endif //  0
	}

#ifdef EDITOR_BUILD
	void hot_reload_assets()
	{
#if 0
		auto scenetype = ClassBase::find_class("SceneAsset");
		assert(scenetype);

		std::vector<IAsset*> toreload;
		{
			std::lock_guard<std::mutex> assetLock(job_mutex);
			toreload.reserve(allAssets.size());
			for (auto& asset : allAssets)
			{
				if (!asset.second->get_is_loaded())
					continue;
				if (asset.second->get_type().is_a(*scenetype))
					continue;
				toreload.push_back(asset.second);
			}
		}
		for(auto i : toreload)
			reload_asset_async(i, true, [](GenericAssetPtr) {});
#endif
	}
#endif


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

	// wait for job queue to be flushed
	void finish_all_jobs() {
		backend.finish_all_jobs();
	}

private:
	IAsset* find_in_all_assets(const string& str) {
		auto f= allAssets.find(str);
		return f == allAssets.end() ? nullptr : f->second;
	}

	// maps a path to a loaded asset
	// this doesnt need a mutex to read
	unordered_map<string, IAsset*> allAssets;

	AssetBackend backend;
};

// reloading: actually allow multiple in memory? then old copy gets GCed. 

// when an object references a 





AssetDatabase::AssetDatabase() {
}
AssetDatabase::~AssetDatabase() {}
AssetDatabase g_assets;


#ifdef EDITOR_BUILD
void AssetDatabase::hot_reload_assets()
{
	impl->hot_reload_assets();
}
#endif
void AssetDatabase::init() {
	// init the loader thread
	impl = std::make_unique<AssetDatabaseImpl>();
	AssetDatabase::loader = new PrimaryAssetLoadingInterface(*impl);
}
void AssetDatabase::finish_all_jobs()
{
	impl->finish_all_jobs();
}
void AssetDatabase::tick_asyncs() {

	impl->tick_asyncs_standard();
}
void AssetDatabase::explicit_asset_free(IAsset*& asset)
{
	if (!asset)
		return;

	impl->remove_asset_direct(asset);
	asset->uninstall();
	delete asset;
	asset = nullptr;
}
void AssetDatabase::reload_sync(IAsset* asset)
{
	//impl->reload_asset_sync(asset);
}
void AssetDatabase::reload_async(IAsset* asset, std::function<void(GenericAssetPtr)> callback)
{
	//impl->reload_asset_async(asset,false,callback);
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
	impl->load_asset_async(path, classType, is_system, callback);
}

void AssetDatabase::remove_unreferences()
{
	impl->uninstall_unreferenced_assets();
}
void AssetDatabase::print_usage()
{
	impl->print_assets();
}
PrimaryAssetLoadingInterface::PrimaryAssetLoadingInterface(AssetDatabaseImpl& frontend) : impl(frontend) {
}
IAsset* PrimaryAssetLoadingInterface::load_asset(const ClassTypeInfo* type, string path)
{
	return impl.load_asset_sync(path, type, false);
}
void PrimaryAssetLoadingInterface::touch_asset(const IAsset* asset)
{

}
PrimaryAssetLoadingInterface AssetDatabase::get_interface()
{
	return PrimaryAssetLoadingInterface(*impl);
}

DECLARE_ENGINE_CMD(print_assets)
{
	g_assets.print_usage();
}
IAssetLoadingInterface* AssetDatabase::loader=nullptr;