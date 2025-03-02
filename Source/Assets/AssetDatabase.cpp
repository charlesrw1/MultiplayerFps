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

struct AsyncQueuedJob
{
	bool is_prioritized = false;
	IAsset* what = nullptr;
	uint32_t referenceMask = 0;
	bool force_reload = false;
	bool is_hot_reload = false;
	std::function<void(GenericAssetPtr)>* loadJobCallback = nullptr;
};

class LoadJob
{
public:

	IAsset* thisAsset = nullptr;
	IAsset* moveIntoThis = nullptr;
	ClassBase* userPtr = nullptr;
	std::function<void(GenericAssetPtr)>* loadJobCallback = nullptr;
	bool skipPostLoad = false;

	friend class AssetDatabase;
	friend class AssetDatabaseImpl;
};

// THREAD LOCALS
// what job are we currently executing
thread_local AsyncQueuedJob* ACTIVE_THREAD_JOB = nullptr;
thread_local bool IS_LOADER_THREAD = false;
thread_local bool IS_MAIN_THREAD = false;

class AssetDatabaseImpl
{
public:
	AssetDatabaseImpl() {

	}
	~AssetDatabaseImpl() {

	}
	void init() {
		IS_MAIN_THREAD = true;

		loadThread =
			new AssetDatabaseImpl::LoadThreadAndSignal(
				std::thread(AssetDatabaseImpl::loaderThreadMain, 0, this));
		loadThread->myThread.detach();
	}

	void install_system_direct(IAsset* asset, const std::string& name) {
		asset->path = name;
		asset->set_both_reference_bitmasks_unsafe( IAsset::GLOBAL_REFERENCE_MASK );
		asset->is_loaded = true;
		asset->has_run_post_load = true;
		asset->is_from_disk = false;

		std::lock_guard<std::mutex> lock(job_mutex);
		allAssets.insert({ name, asset });
	}

	void remove_asset_direct(IAsset* asset) {
		finish_all_jobs();
		allAssets.erase(asset->path);
	}

	void tick_asyncs_standard() {
		ASSERT(!IS_LOADER_THREAD);
		ASSERT(IS_MAIN_THREAD);
		//
		auto fetch_finished_job = [&]() -> LoadJob* {
			std::unique_lock<std::mutex> lock(job_mutex);
			if (finishedAsyncJobs.empty()) {
				return nullptr;
			}
			auto job = finishedAsyncJobs.front();
			finishedAsyncJobs.pop();
			ASSERT(job);
			return job;
		};

		auto finalize_job_with_main_thread = [this](LoadJob* job) {
#ifdef _DEBUG
			sys_print(Debug, "finalize job %s resource %s\n", job->thisAsset->get_type().classname, job->thisAsset->get_name().c_str());
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

		// run through finished queue
		LoadJob* finished = fetch_finished_job();

		while (finished) {

			// calls delete on the job
			finalize_job_with_main_thread(finished);

			finished = fetch_finished_job();
		}
	}


	LoadJob* do_load_asset(IAsset* asset, bool force_reload, uint32_t reference_mask, bool is_hot_reload, std::function<void(GenericAssetPtr)>* loadJobCallback)
	{
		ASSERT(IS_LOADER_THREAD);

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
				return nullptr;
			}
			auto timestamp = f->get_timestamp();
			f->close();
			bool should_reload = asset->asset_load_time < timestamp;
			should_reload |= asset->check_import_files_for_out_of_data();

			if (!should_reload) {
				return nullptr;
			}
		}
#endif

		if (!asset->is_loaded || force_reload) { /*  can use loaded_internal here, this is the most recent variable and its only written/read under the work lock*/
			ClassBase* userStruct = nullptr;
			IAsset* copiedAsset = nullptr;
			LoadJob* job = nullptr;
			if (force_reload) {

				copiedAsset = (IAsset*)asset->get_type().allocate();
				copiedAsset->path = asset->path;
				copiedAsset->load_failed = false;
				copiedAsset->set_both_reference_bitmasks_unsafe(0);
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
			asset_to_load->load_failed = !asset_to_load->load_asset(userStruct) /* not success */;
			if (asset_to_load->load_failed) {
				sys_print(Error, "failed to load %s asset %s\n", asset_to_load->get_type().classname, asset_to_load->path.c_str());
			}
			asset_to_load->reference_bitmask_internal = (force_reload) ? asset->reference_bitmask_internal : reference_mask;
			asset_to_load->is_loaded = true;
			job = init_new_job(asset_to_load, (force_reload) ? asset : nullptr, userStruct, loadJobCallback, false);
			return job;

		}
		else if (!asset->is_mask_refererenced(reference_mask)) {
			asset->sweep_references();
			asset->reference_bitmask_internal |= reference_mask;

			auto job = init_new_job(asset, nullptr,nullptr, loadJobCallback,true);
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
	}

	IAsset* find_existing_or_create(const std::string& path, const ClassTypeInfo* assetType)
	{
		std::unique_lock<std::mutex> assetTableLock(job_mutex);

		auto find1 = allAssets.find(path);

		if (find1 == allAssets.end()) {
			IAsset* createdAsset = (IAsset*)assetType->allocate();
			createdAsset->load_failed = false;
			createdAsset->is_loaded = false;
			createdAsset->has_run_post_load = false;
			createdAsset->set_both_reference_bitmasks_unsafe( 0 );
			createdAsset->path = path;
			allAssets.insert({ path,createdAsset });
			return createdAsset;
		}
		else {
			return find1->second;
		}
	}


	IAsset* sync_load_main_thread(const std::string& path, const ClassTypeInfo* assetType, bool force_reload, uint32_t mask) {
		assert(!ACTIVE_THREAD_JOB);
		
		auto asset = find_existing_or_create(path, assetType);

		// asset has run post load, matches bitmask, not a force reload; just return it (dont have to mess with more locks or atomics)
		if (asset->has_run_post_load && ((asset->reference_bitmask_threadsafe & mask) == mask) && !force_reload)
			return asset;

		AsyncQueuedJob myJob;
		myJob.what = asset;
		myJob.force_reload = force_reload;
		myJob.referenceMask = mask;
		myJob.loadJobCallback = nullptr;

		// asset is loaded, but the bitmask differs, queue an async job to reference it
		if (asset->has_run_post_load&&!force_reload) {
			queue_load_job(myJob);
			return asset;
		}
		queue_load_job_front_and_wait(myJob);
		//tick_asyncs_standard();
		return asset;
	}

	IAsset* sync_load_loader_thread(const std::string& path, const ClassTypeInfo* assetType)
	{
		ASSERT(IS_LOADER_THREAD);
		auto asset = find_existing_or_create(path, assetType);
		sync_load_asset(asset);
		return asset;
	}
	void sync_load_asset(const IAsset* asset)
	{
		ASSERT(IS_LOADER_THREAD);
		ASSERT(ACTIVE_THREAD_JOB);
		LoadJob* j = do_load_asset((IAsset*)asset /* const cast */, false, ACTIVE_THREAD_JOB->referenceMask,false, nullptr);
		if (j) {
			std::unique_lock<std::mutex> lock(job_mutex);
			finishedAsyncJobs.push(j);
		}
	}

	// reloads the asset right now
	void reload_asset_sync(IAsset* asset) {
		ASSERT(!IS_LOADER_THREAD);
		ASSERT(IS_MAIN_THREAD);
		assert(asset);

		if (!asset->is_loaded) {
			sys_print(Warning, "asset not loaded\n");
			return;
		}

		AsyncQueuedJob myJob;
		myJob.what = asset;
		myJob.force_reload = true;
		myJob.referenceMask = 0;
		myJob.loadJobCallback = nullptr;
		
		queue_load_job_front_and_wait(myJob);
		//finish_all_jobs();
	}

	void start_async_job_internal(IAsset* asset, bool reload,bool hot_reload, uint32_t mask, std::function<void(GenericAssetPtr)>& func) {
		AsyncQueuedJob myJob;
		myJob.what = asset;
		myJob.force_reload = reload;
		myJob.referenceMask = mask;
		myJob.is_hot_reload = hot_reload;
		myJob.loadJobCallback = new std::function<void(GenericAssetPtr)>(std::move(func));
		queue_load_job(myJob);
	}
	void load_asset_async(const std::string& str, const ClassTypeInfo* type, uint32_t mask, std::function<void(GenericAssetPtr)>& func) {
		auto asset = find_existing_or_create(str, type);
		start_async_job_internal(asset, false, false,mask, func);
	}

	// queues asset reload, move data will be called on main thread later
	void reload_asset_async(IAsset* asset, bool is_hot_reload, std::function<void(GenericAssetPtr)> loadJobCallback) {
		start_async_job_internal(asset, true, is_hot_reload, 0/* unused*/, loadJobCallback);
	}

	void unreference_this_mask(uint32_t mask)
	{
		std::lock_guard<std::mutex> workLock(job_mutex);
		for (auto& asset : allAssets)
		{
			asset.second->reference_bitmask_internal &= ~mask;
			asset.second->move_internal_to_threadsafe_bitmask_unsafe();

		}
	}

	void uninstall_unreferenced_assets()
	{
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
	}

	std::deque<AsyncQueuedJob> pendingAsyncJobs;	// jobs that are waiting to be executed
	std::queue<LoadJob*> finishedAsyncJobs; // finished jobs
	
	struct LoadThreadAndSignal
	{
		LoadThreadAndSignal(std::thread&& thread) : myThread(std::move(thread)) {}
		std::thread myThread;
	};

	LoadThreadAndSignal* loadThread = nullptr;

	bool has_initialized() {
		return loadThread != nullptr;
	}

	
	static void loaderThreadMain(int index, AssetDatabaseImpl* impl);

#ifdef EDITOR_BUILD
	void hot_reload_assets()
	{
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
	}
#endif


	void print_assets() {
		std::lock_guard<std::mutex> assetLock(job_mutex);
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
		ASSERT(!IS_LOADER_THREAD);

		sys_print(Info, "finish all jobs\n");

		// wait for jobs AND run post load (which MIGHT queue more jobs...)

		auto wait_for_all_jobs_to_finish = [&]() {
			std::unique_lock<std::mutex> lock(job_mutex);
			job_condition_var.wait(lock, [&] { return pendingAsyncJobs.empty() && !is_in_job; });
		};

		for (;;) {
			tick_asyncs_standard();
			wait_for_all_jobs_to_finish();
			{
				std::unique_lock<std::mutex> lock(job_mutex);
				if (finishedAsyncJobs.empty())
					break;
			}
		}
	}

private:
	
	void queue_load_job(AsyncQueuedJob j) {
		{
			std::unique_lock<std::mutex> lock(job_mutex);
			pendingAsyncJobs.push_back(j);
		}
		job_condition_var.notify_one();
	}
	void queue_load_job_front_and_wait(AsyncQueuedJob j) {
		ASSERT(!IS_LOADER_THREAD);
		sys_print(Info, "queue_load_job_front_and_wait\n");
		{
			std::unique_lock<std::mutex> lock(job_mutex);
			j.is_prioritized = true;
			pendingAsyncJobs.push_front(j);
			ASSERT(prioritized_job_done);
			prioritized_job_done = false;
		}
		job_condition_var.notify_one();

		{
			std::unique_lock<std::mutex> lock(job_mutex);
			job_condition_var.wait(lock, [&] { return prioritized_job_done.load(); });
		}
	}

	std::mutex job_mutex;
	std::condition_variable job_condition_var;
	bool is_in_job = false;
	std::atomic<bool> prioritized_job_done = true;


	std::unordered_map<IAsset*, LoadJob*> jobsInQueue;		// maps a path to an outstanding load job

	std::unordered_map<std::string, IAsset*> allAssets;		// maps a path to a loaded asset
};

void AssetDatabaseImpl::loaderThreadMain(int index, AssetDatabaseImpl* impl)
{
	IS_LOADER_THREAD = true;

	auto execute_job = [impl](AsyncQueuedJob* job) -> LoadJob* {
		ACTIVE_THREAD_JOB = job;
		LoadJob* j = impl->do_load_asset(job->what, job->force_reload, job->referenceMask, job->is_hot_reload, job->loadJobCallback);
		ACTIVE_THREAD_JOB = nullptr;
		return j;
	};


	while (1)
	{
		std::unique_lock<std::mutex> lock(impl->job_mutex);
		impl->job_condition_var.wait(lock, [&] {return !impl->pendingAsyncJobs.empty() || impl->is_in_job; });
		if(!impl->pendingAsyncJobs.empty())
		{
			impl->is_in_job = true;
			AsyncQueuedJob jobQueued = impl->pendingAsyncJobs.front();
			impl->pendingAsyncJobs.pop_front();
			lock.unlock();
			LoadJob* j = execute_job(&jobQueued);
			lock.lock();
			if (j) {
				impl->finishedAsyncJobs.push(j);
			}
			impl->is_in_job = false;

			if (jobQueued.is_prioritized) {
				impl->prioritized_job_done = true;
				impl->job_condition_var.notify_all();
			}
		}
		impl->job_condition_var.notify_all();
	}
}


AssetDatabase::AssetDatabase() {
	impl = std::make_unique<AssetDatabaseImpl>();
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
	impl->init();
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

GenericAssetPtr AssetDatabase::find_sync(const std::string& path, const ClassTypeInfo* classType, int lifetime_channel)
{
	if (!impl->has_initialized()) {
		Fatalf("tried find_sync before AssetDatabase was initialized. Use find_async for any jobs that you want queued at startup\n");
	}

	const uint32_t mask = (1ul << lifetime_channel);
	IAsset* out = nullptr;
	if (IS_LOADER_THREAD) {
		out = impl->sync_load_loader_thread(path, classType);
	}
	else {
		out = impl->sync_load_main_thread(path, classType, false, mask);
	}


	return out;
}
void AssetDatabase::find_async(const std::string& path, const ClassTypeInfo* classType, std::function<void(GenericAssetPtr)> callback, int lifetime_channel)
{
	const uint32_t mask = (1 << lifetime_channel);
	impl->load_asset_async(path, classType, mask, callback);
}

IAsset* AssetDatabase::find_assetptr_unsafe(const std::string& path, const ClassTypeInfo* ti)
{
	if (!impl->has_initialized()) {
		auto asset = impl->find_existing_or_create(path, ti);
		return asset;
	}

	if (IS_LOADER_THREAD)
		return impl->sync_load_loader_thread(path, ti);
	else
		return impl->sync_load_main_thread(path, ti, false, 1/*1<<0, the default "i want an asset with a long but not infinite lifetime (corresponds with level/editor state)"*/);
}
void AssetDatabase::touch_asset(const IAsset* a)
{
	if(a)
		impl->sync_load_asset(a);
}
void AssetDatabase::unreference_this_channel(uint32_t channel)
{
	uint32_t mask = (1ul << channel);
	impl->unreference_this_mask(mask);
}
void AssetDatabase::remove_unreferences()
{
	impl->uninstall_unreferenced_assets();
}
void AssetDatabase::print_usage()
{
	impl->print_assets();
}

DECLARE_ENGINE_CMD(print_assets)
{
	g_assets.print_usage();
}

#include "Test/Test.h"
ADD_TEST(AssetDatabaseTest, General)
{

}