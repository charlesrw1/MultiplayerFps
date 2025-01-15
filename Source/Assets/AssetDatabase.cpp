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

struct AsyncQueuedJob
{
	IAsset* what = nullptr;
	uint32_t referenceMask = 0;
	bool force_reload = false;
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

struct AssetDatabaseImpl
{
	void install_system_direct(IAsset* asset, const std::string& name) {
		asset->path = name;
		asset->set_both_reference_bitmasks_unsafe( IAsset::GLOBAL_REFERENCE_MASK );
		asset->is_loaded = true;
		asset->has_run_post_load = true;
		asset->is_from_disk = false;

		std::lock_guard<std::mutex> lock(assetHashmapMutex);
		allAssets.insert({ name, asset });
	}


	LoadJob* fetch_finished_job() {
		std::unique_lock<std::mutex> lock(finishedQueueMutex);
		if (finishedAsyncJobs.empty()) {
			return nullptr;
		}
		auto job = finishedAsyncJobs.front();
		finishedAsyncJobs.pop();
		return job;
	}

	LoadJob* fetch_finished_job_no_lock() {
		if (finishedAsyncJobs.empty()) {
			return nullptr;
		}
		auto job = finishedAsyncJobs.front();
		finishedAsyncJobs.pop();
		return job;
	}

	void remove_asset_direct(IAsset* asset) {
		std::lock_guard<std::mutex> lock(assetHashmapMutex);
		allAssets.erase(asset->path);
	}

	void tick_asyncs_standard() {
		// run through finished queue
		LoadJob* finished = fetch_finished_job();

		while (finished) {

			// calls delete on the job
			finalize_job_with_main_thread(finished);

			finished = fetch_finished_job();
		}
	}
	// tick asyncs but prevents new jobs from being added
	void tick_asyncs_blocking() {
		std::lock_guard<std::mutex> queueLock(finishedQueueMutex);

		// run through finished queue
		LoadJob* finished = fetch_finished_job_no_lock();

		while (finished) {

			// calls delete on the job
			finalize_job_with_main_thread(finished);

			finished = fetch_finished_job_no_lock();
		}
	}

	void add_to_finished_queue(LoadJob* job)
	{
		std::lock_guard<std::mutex> finishedLock(finishedQueueMutex);
		finishedAsyncJobs.push(job);
	}
	void load_shared(IAsset* asset, bool force_reload, uint32_t reference_mask, std::function<void(GenericAssetPtr)>* loadJobCallback)
	{
		if (!asset->is_loaded || force_reload) { /* we can use loaded_internal here, this is the most recent variable and its only written/read under the work lock*/
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
			asset_to_load->load_failed = !asset_to_load->load_asset(userStruct) /* not success */;
			if (asset_to_load->load_failed) {
				sys_print(Error, "failed to load %s asset %s\n", asset_to_load->get_type().classname, asset_to_load->path.c_str());
			}
			asset_to_load->reference_bitmask_internal = (force_reload) ? asset->reference_bitmask_internal : reference_mask;
			asset_to_load->is_loaded = true;
			job = init_new_job(asset_to_load, (force_reload) ? asset : nullptr, userStruct, loadJobCallback);
			add_to_finished_queue(job);

		}
		else if (!asset->is_mask_refererenced(reference_mask)) {
			asset->sweep_references();
			asset->reference_bitmask_internal |= reference_mask;

			auto job = init_callback_placeholder_job(asset, loadJobCallback);
			add_to_finished_queue(job);
		}
		else {
			// asset is loaded and is referenced
			// (do nothing)
			if (loadJobCallback) {
				auto job = init_callback_placeholder_job(asset, loadJobCallback);
				add_to_finished_queue(job);
			}
		}
	}

	IAsset* find_existing_or_create(const std::string& path, const ClassTypeInfo* assetType)
	{
		std::unique_lock<std::mutex> assetTableLock(assetHashmapMutex);	// this locks any access to futures or assets

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

	void finalize_job_with_main_thread(LoadJob*& job)
	{
#ifdef _DEBUG
		sys_print(Debug,"finalize job %s resource %s\n", job->thisAsset->get_type().classname, job->thisAsset->get_name().c_str());
#endif

		if (!job->skipPostLoad) {
			if (job->moveIntoThis) {
				job->moveIntoThis->move_construct(job->thisAsset);
				job->moveIntoThis->load_failed = job->thisAsset->load_failed;
				delete job->thisAsset;
				job->thisAsset = job->moveIntoThis;
			}
			assert(job->thisAsset->is_loaded);
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

		delete job;
		job = nullptr;
	}



	void execute_job_shared(AsyncQueuedJob* job)
	{
		std::unique_lock<std::mutex> workLock(loadingJobMutex);
		ACTIVE_THREAD_JOB = job;
		load_shared(job->what, job->force_reload, job->referenceMask, job->loadJobCallback);
		ACTIVE_THREAD_JOB = nullptr;
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

		// else either the asset isnt loaded or the bitmask differs, run the job now
		execute_job_shared(&myJob);
		tick_asyncs_standard();

		return asset;
	}

	IAsset* sync_load_loader_thread(const std::string& path, const ClassTypeInfo* assetType)
	{
		auto asset = find_existing_or_create(path, assetType);
		auto job = ACTIVE_THREAD_JOB;
		assert(job);
		load_shared(asset, false, job->referenceMask, nullptr);
		return asset;
	}
	void touch_asset_loader(const IAsset* asset)
	{
		auto job = ACTIVE_THREAD_JOB;
		assert(job);
		load_shared((IAsset*)asset /* const cast */, false, job->referenceMask, nullptr);
	}

	// reloads the asset right now
	void reload_asset_sync(IAsset* asset) {
		assert(asset);

		AsyncQueuedJob myJob;
		myJob.what = asset;
		myJob.force_reload = true;
		myJob.referenceMask = 0;
		myJob.loadJobCallback = nullptr;

		execute_job_shared(&myJob);

		// causes our job to finalize itself
		// ticking the async queue might trigger jobs that arent from us to finalize too,
		// however that is a nessecary side effect as some of our assets might already depend on other assets being loaded
		// and they are queued to finalize before us
		tick_asyncs_blocking();
	}

	void start_async_job_internal(IAsset* asset, bool reload, uint32_t mask, std::function<void(GenericAssetPtr)>& func) {
		AsyncQueuedJob myJob;
		myJob.what = asset;
		myJob.force_reload = reload;
		myJob.referenceMask = mask;
		myJob.loadJobCallback = new std::function<void(GenericAssetPtr)>(std::move(func));
		queue_load_job(myJob);
	}
	void load_asset_async(const std::string& str, const ClassTypeInfo* type, uint32_t mask, std::function<void(GenericAssetPtr)>& func) {
		auto asset = find_existing_or_create(str, type);
		start_async_job_internal(asset, false, mask, func);
	}

	// queues asset reload, move data will be called on main thread later
	void reload_asset_async(IAsset* asset, std::function<void(GenericAssetPtr)>& loadJobCallback)
	{
		start_async_job_internal(asset, true, 0/* unused*/, loadJobCallback);
	}

	void unreference_this_mask(uint32_t mask)
	{
		std::lock_guard<std::mutex> workLock(loadingJobMutex);
		for (auto& asset : allAssets)
		{
			asset.second->reference_bitmask_internal &= ~mask;
			asset.second->move_internal_to_threadsafe_bitmask_unsafe();

		}
	}
	void uninstall_unreferenced_assets()
	{
		tick_asyncs_standard();	// clear the queue if anything is there
		std::lock_guard<std::mutex> workLock(loadingJobMutex);
		std::lock_guard<std::mutex> assetLock(assetHashmapMutex);
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

	std::queue<AsyncQueuedJob> pendingAsyncJobs;	// jobs that are waiting to be executed
	std::queue<LoadJob*> finishedAsyncJobs;	// jobs that have NO dependencies left waiting (outstandingFutures == 0)
	
	
	std::mutex queueMutex;
	std::mutex loadingJobMutex;
	std::mutex finishedQueueMutex;

	struct LoadThreadAndSignal
	{
		LoadThreadAndSignal(std::thread&& thread) : myThread(std::move(thread)) {}
		std::thread myThread;
	};

	LoadThreadAndSignal* loadThread = nullptr;

	bool has_initialized() {
		return loadThread != nullptr;
	}

	LoadJob* init_new_job(
		IAsset* asset,
		IAsset* reloadAsset,
		ClassBase* userPtr,
		std::function<void(GenericAssetPtr)>* callback
	)
	{
		LoadJob* newJob = new LoadJob;
		newJob->thisAsset = asset;
		newJob->moveIntoThis = reloadAsset;
		newJob->userPtr = userPtr;
		newJob->loadJobCallback = callback;

		return newJob;
	}
	LoadJob* init_callback_placeholder_job(
		IAsset* asset,
		std::function<void(GenericAssetPtr)>* callback
	)
	{
		LoadJob* newJob = new LoadJob;
		newJob->loadJobCallback = callback;
		newJob->skipPostLoad = true;
		newJob->thisAsset = asset;
		return newJob;
	}

	
	static void loaderThreadMain(int index, AssetDatabaseImpl* impl);

private:
	
	void queue_load_job(AsyncQueuedJob j) {
		{
			std::unique_lock<std::mutex> lock(queueMutex);
			pendingAsyncJobs.push(j);
		}
		condition.notify_one();
	}

	std::condition_variable condition;
	std::mutex assetHashmapMutex;	// guards accessing allAssets
	std::unordered_map<IAsset*, LoadJob*> jobsInQueue;		// maps a path to an outstanding load job
	std::unordered_map<std::string, IAsset*> allAssets;		// maps a path to a loaded asset
};

void AssetDatabaseImpl::loaderThreadMain(int index, AssetDatabaseImpl* impl)
{
	while (1)
	{
		AsyncQueuedJob jobQueued;
		{
			std::unique_lock<std::mutex> lock(impl->queueMutex);		// prevent read/write to the queue
			impl->condition.wait(lock, [&] { return !impl->pendingAsyncJobs.empty(); });
			jobQueued = impl->pendingAsyncJobs.front();
			impl->pendingAsyncJobs.pop();
		}
#ifdef _DEBUG
		printf("*** executing AsyncQueuedJob on loader thread %s\n", jobQueued.what->get_name().c_str());
#endif

		impl->execute_job_shared(&jobQueued);
	}
}


AssetDatabase::AssetDatabase() {
	impl = std::make_unique<AssetDatabaseImpl>();
}
AssetDatabase::~AssetDatabase() {}

void AssetDatabase::init() {
	// init the loader thread

	impl->loadThread =
			new AssetDatabaseImpl::LoadThreadAndSignal(
				std::thread(AssetDatabaseImpl::loaderThreadMain, 0, impl.get()));
	impl->loadThread->myThread.detach();
}

void AssetDatabase::tick_asyncs() {

	impl->tick_asyncs_standard();
}

void AssetDatabase::explicit_asset_free(IAsset*& asset)
{
	if (!asset)
		return;

	std::lock_guard<std::mutex> workMutex(impl->loadingJobMutex);
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
	impl->reload_asset_async(asset,callback);
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
	if (ACTIVE_THREAD_JOB) {
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

	if (ACTIVE_THREAD_JOB)
		return impl->sync_load_loader_thread(path, ti);
	else
		return impl->sync_load_main_thread(path, ti, false, 1/*1<<0, the default "i want an asset with a long but not infinite lifetime (corresponds with level/editor state)"*/);
}
void AssetDatabase::touch_asset(const IAsset* a)
{
	if(a)
		impl->touch_asset_loader(a);
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