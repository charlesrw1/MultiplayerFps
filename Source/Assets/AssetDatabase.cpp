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


struct AsyncQueuedJob
{
	AsyncQueuedJob() {
		creation_start_time = GetTime();
	}

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
	// for debugging
	double creation_start_time = 0.0;
	double load_time = 0.0;

	// output
	IAsset* out_object = nullptr;
	vector<IAsset*> other_assets;
	vector<double> other_load_times;
	vector<IAsset*> reloaded_also;
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

class GcMarkingInterface : public IAssetLoadingInterface
{
public:
	GcMarkingInterface() {}
	IAsset* load_asset(const ClassTypeInfo* type, string path) final {
		assert(0);
		return nullptr;
	}
	void touch_asset(const IAsset* asset) final {
		if (!asset)
			return;
		if (asset->gc != IAsset::White)	// only non seen assets pass
			return;
		IAsset* mut = (IAsset*)asset;	// whoops
		mut->gc = IAsset::Gray; // mark it gray
		marklist.push_back(mut);
	}
	vector<IAsset*> marklist;
};

class AssetBackend
{
public:
	AssetBackend(const unordered_map<string,IAsset*>& global) 
		: my_thread([this]() { loader_loop();}), global_assets(global)
	{
		//my_thread.detach();
	}
	~AssetBackend() {
		
	}
	void signal_end_work();
	void push_job_to_queue(uptr<AsyncQueuedJob> job);
	void combine_asset_tables(unordered_map<string, IAsset*>& table);
	uptr<AsyncQueuedJob> pop_finished_job();
	void finish_all_jobs();
	bool is_in_reset_state();
private:
	void loader_loop();
	void execute_job(AsyncQueuedJob* job);
	IAsset* find_or_create_and_load_asset(const string& path, const ClassTypeInfo* type, bool is_system, AsyncQueuedJob* job);

	// doesnt create
	IAsset* find_asset_in_local_or_global(const string& path) {
		auto f = find_in_local(path);
		if (f)
			return f;
		f = find_in_global(path);
		if (f)
			return f;
		return nullptr;
	}
	IAsset* find_in_local(const std::string& path) {
		auto f = local_asset_loads.find(path);
		if (f != local_asset_loads.end())
			return f->second;
		return nullptr;
	}
	IAsset* find_in_global(const std::string& path) {
		auto f = global_assets.find(path);
		if (f != global_assets.end())
			return f->second;
		return nullptr;
	}

	IAsset* create_asset(const string& path, const ClassTypeInfo* info) {
		auto a = info->allocate_this_type();
		assert(a->is_a<IAsset>());
		IAsset* as = (IAsset*)a;
		as->path = path;
		as->gc = IAsset::Gray;// keep it gray

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
bool AssetBackend::is_in_reset_state()
{
	std::lock_guard<std::mutex> l(job_mutex);
	return jobs.empty() && finished_jobs.empty() && local_asset_loads.empty();
}

IAsset* SubAssetLoadingInterface::load_asset(const ClassTypeInfo* type, string path)
{
	return backend.find_or_create_and_load_asset(path, type, false,whatjob);
}
IAsset* AssetBackend::find_or_create_and_load_asset(const string& path, const ClassTypeInfo* info, bool is_system, AsyncQueuedJob* job)
{
	IAsset* asset = find_asset_in_local_or_global(path);
	if (!asset)
	{
		double now = GetTime();

		asset = create_asset(path, info);
		asset->is_loaded = true;
		SubAssetLoadingInterface loadinterface(*this,job);
		const bool success = asset->load_asset(&loadinterface);
		asset->is_system |= is_system;
		asset->load_failed = !success;

		job->other_assets.push_back(asset);
		job->other_load_times.push_back(GetTime() - now);
	}
	asset->is_system |= is_system;
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
	if(log_finish_job_func.get_bool())
		sys_print(Debug, "AssetBackend::finish_all_jobs\n");
	std::unique_lock<std::mutex> lock(job_mutex);
	job_cv.wait(lock, [&] { return jobs.empty() && !is_in_job; });
	if (log_finish_job_func.get_bool())
		sys_print(Debug, "/AssetBackend::finish_all_jobs\n");

}
void AssetBackend::signal_end_work() {
	{
		std::unique_lock<std::mutex> lock(job_mutex);
		stop_processing = true;
		job_cv.notify_all();
	}
	my_thread.join();
}

void AssetBackend::combine_asset_tables(unordered_map<string, IAsset*>& table) {
	assert(&table == &global_assets);
	std::lock_guard<std::mutex> lock(table_mutex);
	for (auto& a : local_asset_loads) {
		if (table.find(a.first) == table.end()) {	// dont do reloaded objects here
			table.insert(a);
			assert(a.second->is_loaded);
		}
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
		IAsset* existing = find_in_global(job->path);
		assert(existing);	// has to be existing

		auto force_create_asset = [&](const std::string& path, const ClassTypeInfo* info, bool is_system_asset)->IAsset* {
			auto asset = create_asset(path, info);
			SubAssetLoadingInterface loadinterface(*this, job);
			const bool success = asset->load_asset(&loadinterface);
			asset->is_system |= is_system_asset;
			asset->load_failed = !success;
			asset->is_loaded = true;
			return asset;
		};

		// force create it
		job->out_object = force_create_asset(job->path,job->info,job->is_system_asset);
		job->reloaded_also.push_back(job->out_object);

		// do the dependents
		std::vector<IAsset*> reload_queue;
		auto add_dependents_to_reload_queue = [&](IAsset* a) {
			for (auto dep : a->reload_dependents)
				reload_queue.push_back(dep);
		};
		add_dependents_to_reload_queue(existing);
		while (!reload_queue.empty()) {
			auto a = reload_queue.front();
			reload_queue.erase(reload_queue.begin());
			add_dependents_to_reload_queue(a);
			auto out = force_create_asset(a->path, &a->get_type(),false);
			job->reloaded_also.push_back(out);
		}

		// by now, the reloaded object and all of its dependents are reloaded and added to the local table and the out objects
		// moveing is handled in main thread
		// note that reloaded dependent objects are dependent on the reloaded asset. so must use care in move function to solve this
	
		assert(job->out_object);
		//assert(job->other_assets.size()>=1);
		assert(!job->out_object->has_run_post_load);
	}
	else
	{
		double time_start = GetTime();
		job->out_object = find_or_create_and_load_asset(job->path, job->info, job->is_system_asset,job);
		job->load_time = GetTime() - time_start;
		assert(find_asset_in_local_or_global(job->path));
		assert(job->out_object->is_system || !job->is_system_asset);
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

	void reset_testing() {
		finish_all_jobs();
		tick_asyncs_standard();
		assert(backend.is_in_reset_state());
		for (auto& a : allAssets) {
			a.second->uninstall();
			delete a.second;
		}
		allAssets.clear();

		assert(allAssets.empty());
		assert(backend.is_in_reset_state());
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

	void post_move_construct(IAsset* existing, const IAsset* copy)
	{
		existing->asset_load_time = copy->asset_load_time;
		existing->is_system |= copy->is_system;
		existing->reload_dependents.clear();
		existing->is_loaded = true;
		for (auto e : copy->reload_dependents) {
			// find again in case its a local etc
			auto dependent_final = find_in_all_assets(e->path);
			existing->reload_dependents.insert(dependent_final);
		}
		existing->has_run_post_load = false;	// not yet
		existing->gc = IAsset::Gray;
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



		uptr<AsyncQueuedJob> job = backend.pop_finished_job();
		while (job) {
			backend.combine_asset_tables(allAssets);

			if (log_all_asset_loads.get_bool()) {
				const double time_since_creation = GetTime() - job->creation_start_time;
				sys_print(Debug, "finalize job %s resource %s (%f %f)\n", job->out_object->get_type().classname, job->out_object->get_name().c_str(), float(job->load_time), float(time_since_creation));
			}
			assert(allAssets.find(job->path) != allAssets.end());
			assert(job->out_object);
			if (job->force_reload) {
				// objects are added in order of dependence
				for (auto o : job->reloaded_also) {
					assert(o);
					assert(!o->has_run_post_load);
					auto existing = find_in_all_assets(o->path);
					assert(existing != o);
					if (!o->load_failed) {
						assert(existing);
						existing->move_construct(o);	// construct it, dont delete yet
					}
					post_move_construct(existing, o);
				}
				// now do deletes and move the existing into the finished so it runs post load
				for (int i = 0; i < job->reloaded_also.size(); i++) {
					auto o = job->reloaded_also[i];
					auto existing = find_in_all_assets(o->path);
					assert(existing);
					assert(existing != o);
					delete o;	// delete the reloaded copy
					job->other_assets.push_back(existing);
				}
				job->out_object = job->other_assets.at(0);	// first object
			}

			if (log_all_asset_loads.get_bool() && job->other_assets.size() == job->other_load_times.size()) {
				vector<int> nums(job->other_assets.size());
				for (int i = 0; i < nums.size(); i++) nums[i] = i;
				std::sort(nums.begin(), nums.end(), [&](int l, int r) {
					return job->other_load_times[l] > job->other_load_times[r];
					});
				for (int i = 0; i < job->other_assets.size(); i++) {
					auto o = job->other_assets[nums[i]];
					double t = job->other_load_times[nums[i]];
					sys_print(Debug, "	subasset %s (%f)\n", o->get_name().c_str(), float(t));
				}
			}

			double pre_post_load = GetTime();
			vector<std::pair<string, float>> timings;
			for (auto o : job->other_assets) {
				if (o->load_failed) {
					sys_print(Error, "AssetDatabase: asset failed to load \"%s\" (type=%s) (FromJob: \"%s\")\n", o->get_name().c_str(),o->get_type().classname, job->out_object->get_name().c_str());
				}
				if (!o->load_failed && !o->has_run_post_load) {
					double now = GetTime();
					try {
						o->post_load();
						o->has_run_post_load = true;
					}
					catch (...) {
						sys_print(Error, "AssetDatabase: asset post load failed \"%s\" (type=%s) (FromJob: \"%s\")\n", o->get_name().c_str(), o->get_type().classname, job->out_object->get_name().c_str());
						o->load_failed = true;
					}
					if (log_all_asset_loads.get_bool()) {
						timings.push_back({ o->get_name(),float(GetTime() - now) });
					}
				}
			}
			if (log_all_asset_loads.get_bool()) {
				std::sort(timings.begin(), timings.end(), [](const std::pair<string, float>& l, const std::pair<string, float>& r) {
					return l.second > r.second;
					});
				for (auto& t : timings) {
					sys_print(Debug, "		PostLoad(%s) took %fs\n",t.first.c_str(), t.second);
				}
				sys_print(Debug, "	took %f to run post_loads\n", float(GetTime() - pre_post_load));
			}

			if(job->callback)
				job->callback(job->out_object);
			job = backend.pop_finished_job();
		}
	}

	void reload_asset_async(IAsset* asset, bool is_system, std::function<void(GenericAssetPtr)>& func) {
		uptr<AsyncQueuedJob> job = std::make_unique<AsyncQueuedJob>();
		job->force_reload = true;
		job->path = asset->get_name();
		job->info = &asset->get_type();
		job->is_system_asset |= is_system;
		job->callback = std::move(func);
		job->validate();
		backend.push_job_to_queue(std::move(job));
	}
	void load_asset_async(const std::string& str, const ClassTypeInfo* type, bool is_system, std::function<void(GenericAssetPtr)>& func) {
		if (str.empty())
			return;
		
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
			job->is_system_asset |= is_system;
			job->callback = std::move(func);
			job->validate();
			backend.push_job_to_queue(std::move(job));
		}
	}
	IAsset* load_asset_sync(const std::string& str, const ClassTypeInfo* type, bool is_system)
	{
		if (str.empty())
			return nullptr;

		auto existing = find_in_all_assets(str);
		if (existing) {
			if (!existing->get_type().is_a(*type)) {
				sys_print(Error, "2 assets with same name but different type: %s\n", str.c_str());
				return nullptr;
			}
			existing->is_system |= is_system;
			return existing;
		}
		else {
			uptr<AsyncQueuedJob> job = std::make_unique<AsyncQueuedJob>();
			job->path = str;
			job->info = type;
			job->is_system_asset = is_system;
			job->validate();
			backend.push_job_to_queue(std::move(job));
			backend.finish_all_jobs();
			backend.combine_asset_tables(allAssets);
			// guaranteed that job is finished by now
			auto a = find_in_all_assets(str);
			assert(a);
			assert(!is_system || a->is_system);
			return a;
		}
	}
	void reload_asset_sync(IAsset* asset) {
		assert(asset);

		function<void(GenericAssetPtr)> func;
		reload_asset_async(asset, asset->is_system, func);
		backend.finish_all_jobs();
		backend.combine_asset_tables(allAssets);
		// guaranteed that job is finished by now
	}

	void mark_assets_as_unreferenced()
	{
		finish_all_jobs();
		tick_asyncs_standard();	// tick asyncs

		for (auto& o : allAssets) {
			if (o.second->is_system)
				o.second->gc = IAsset::Gray;
			else
				o.second->gc = IAsset::White;
		}
	}
	
	void uninstall_unreferenced_assets()
	{
		finish_all_jobs();
		tick_asyncs_standard();	// tick asyncs

		GcMarkingInterface marking;
		for (auto& asset : allAssets) {
			if (asset.second->gc == IAsset::Gray) {
				marking.marklist.push_back(asset.second);
			}
		}
		while (!marking.marklist.empty()) {
			IAsset* back = marking.marklist.back();
			marking.marklist.pop_back();
			back->sweep_references(&marking);
			back->gc = IAsset::Black;
		}
		std::vector<IAsset*>& remove_these = marking.marklist;	// reuse it
		remove_these.clear();
		for (auto& asset : allAssets) {
			if (asset.second->gc == IAsset::White) {
				remove_these.push_back(asset.second);
				if(log_all_asset_loads.get_bool())
					sys_print(Debug,"uninstalling %s resource %s\n", asset.second->get_type().classname, asset.second->get_name().c_str());
				asset.second->uninstall();
			}
		}
		for (auto i : remove_these) {
			allAssets.erase(i->get_name());
			delete i;
		}
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

	// wait for job queue to be flushed
	void finish_all_jobs() {
		backend.finish_all_jobs();
	}
	void quit() {
		sys_print(Info, "quitting asset loader\n");
		backend.signal_end_work();
	}
	bool is_asset_loaded(const string& path) {
		return MapUtil::contains(allAssets, path);
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


void AssetDatabase::quit() {
	impl->quit();
}

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
void AssetDatabase::reset_testing()
{
	impl->reset_testing();
}
void AssetDatabase::finish_all_jobs()
{
	impl->finish_all_jobs();
}
void AssetDatabase::tick_asyncs() {

	impl->tick_asyncs_standard();
}
void AssetDatabase::remove_system_reference(IAsset* asset)
{
	asset->is_system = false;
	impl->remove_asset_direct(asset);
}
bool AssetDatabase::is_asset_loaded(const std::string& path)
{
	return impl->is_asset_loaded(path);
}
void AssetDatabase::mark_unreferences()
{
	impl->mark_assets_as_unreferenced();
}
void AssetDatabase::reload_sync(IAsset* asset)
{
	impl->reload_asset_sync(asset);
}
void AssetDatabase::reload_async(IAsset* asset, std::function<void(GenericAssetPtr)> callback)
{
	impl->reload_asset_async(asset,false,callback);
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
void AssetDatabase::dump_loaded_assets_to_disk(const std::string& path)
{
	sys_print(Info, "AssetDatabase::dump_loaded_assets_to_disk: %s\n", path.c_str());
	auto file = FileSys::open_write_game(path);
	if (!file) {
		sys_print(Error, "AssetDatabase::dump_loaded_assets_to_disk: path couldn't open %s\n", path.c_str());
	}
	impl->dump_to_file(file.get());
}
PrimaryAssetLoadingInterface::PrimaryAssetLoadingInterface(AssetDatabaseImpl& frontend) : impl(frontend) {
}
IAsset* PrimaryAssetLoadingInterface::load_asset(const ClassTypeInfo* type, string path)
{
	return impl.load_asset_sync(path, type, false);
}
void PrimaryAssetLoadingInterface::touch_asset(const IAsset* asset)
{
	if (!asset)
		return;
	auto mut = (IAsset*)asset;
	mut->gc = IAsset::Gray;
}

IAssetLoadingInterface* AssetDatabase::loader=nullptr;