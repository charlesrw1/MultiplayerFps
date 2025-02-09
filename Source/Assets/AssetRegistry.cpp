#ifdef EDITOR_BUILD
#include "Assets/AssetRegistry.h"
#include "Framework/Files.h"
#include <algorithm>
#include <unordered_map>
#include <sstream>
#include "AssetCompile/Someutils.h"
#include <unordered_set>
#include <cassert>
#include "Framework/Config.h"
#include <windows.h>
#include "AssetDatabase.h"

AssetRegistrySystem& AssetRegistrySystem::get()
{
	static AssetRegistrySystem inst;
	return inst;
}

ConfigVar asset_registry_reindex_period("asset_registry_reindex_period", "2", CVAR_DEV | CVAR_INTEGER, "time in seconds of registry reindexing");

static std::vector<std::string> split(const std::string& str, char delimiter) {
	std::vector<std::string> tokens;
	std::stringstream ss(str);
	std::string token;

	while (std::getline(ss, token, delimiter)) {
		tokens.push_back(token);
	}

	return tokens;
}


// too lazy, just hack the asset async loader to do this haha
CLASS_H(HackedAsyncAssetRegReindex, IAsset)
public:
	void uninstall() override {

	}
	void move_construct(IAsset* other) override {
		this->root = std::move(((HackedAsyncAssetRegReindex*)other)->root);
	}
	void sweep_references() const override {
	}
	bool load_asset(ClassBase*&) override {
		std::vector<AssetOnDisk> diskAssets;
		diskAssets.clear();
		const int len = strlen(FileSys::get_game_path());
		auto& all_assettypes = AssetRegistrySystem::get().all_assettypes;
		for (int i = 0; i < all_assettypes.size(); i++)
		{
			auto type = all_assettypes[i].get();

			bool is_filenames = type->assets_are_filepaths();

			std::unordered_set<std::string> added_already;

			if (is_filenames) {
				for (const auto& file : FileSys::find_game_files()) {
					for (int j = 0; j < type->extensions.size(); j++) {
						if (type->extensions[j] == get_extension_no_dot(file)) {
							if (added_already.find(file) != added_already.end())
								continue;

							AssetOnDisk aod;
							aod.filename = file;
							if (is_filenames) {
								if (aod.filename.find(FileSys::get_game_path()) == 0)
									aod.filename = aod.filename.substr(len + 1);
							}
							aod.type = type;

							diskAssets.push_back(aod);

							added_already.insert(file);
						}
					}
					if (!type->pre_compilied_extension.empty()) {
						assert(type->extensions.size() != 0);
						if (type->pre_compilied_extension == get_extension_no_dot(file)) {
							auto path = strip_extension(file) + "." + type->extensions.at(0);	// unsafe
							if (added_already.find(path) == added_already.end()) {
								AssetOnDisk aod;
								aod.filename = path;
								if (is_filenames) {
									if (aod.filename.find(FileSys::get_game_path()) == 0)
										aod.filename = aod.filename.substr(len + 1);
								}
								aod.type = type;

								diskAssets.push_back(aod);

								added_already.insert(path);
							}
						}
					}
				}
			}
			std::vector<std::string> extraAssets;
			type->fill_extra_assets(extraAssets);
			for (int j = 0; j < extraAssets.size(); j++) {
				AssetOnDisk aod;
				aod.filename = std::move(extraAssets[j]);
				aod.type = type;
				diskAssets.push_back(aod);
			}
		}

		root.reset();
		root = std::make_unique<AssetFilesystemNode>("root");

		for (auto a : diskAssets) {
			auto& filename = a.filename;
			std::vector<std::string> path = split(filename, '/');
			root->addPath(a, path);
		}

		return true;
	}
	void post_load(ClassBase*) override {
		AssetRegistrySystem::get().root = std::move(root);

		sys_print(Info, "finished assset reindex\n");
	}

	std::unique_ptr<AssetFilesystemNode> root;
};
CLASS_IMPL(HackedAsyncAssetRegReindex);

static HANDLE directoryChangeHandle = 0;
static HackedAsyncAssetRegReindex* hackedAsset = nullptr;

void AssetRegistrySystem::init()
{
	directoryChangeHandle = FindFirstChangeNotificationA(
		".\\",		 // directory to watch 
		TRUE,                         //  watch subtree 
		FILE_NOTIFY_CHANGE_LAST_WRITE); 

	if (directoryChangeHandle == INVALID_HANDLE_VALUE) {
		Fatalf("ERROR: AssetRegistrySystem::init: FindFirstChangeNotificationA failed: %s\n", GetLastError());
	}
	hackedAsset = new HackedAsyncAssetRegReindex();
	g_assets.install_system_asset(hackedAsset, "_hackedAsset");

	reindex_all_assets();
}

void AssetRegistrySystem::update()
{
	double time_now = TimeSinceStart();
	{
		auto status = WaitForMultipleObjects(1, &directoryChangeHandle, TRUE, 0);
		if (status == WAIT_TIMEOUT)
			return;
		if (status == WAIT_FAILED) {
			sys_print(Error, "WaitForMultipleObjects failed: %s\n", GetLastError());
			return;
		}
		if (time_now - last_reindex_time <= (double)asset_registry_reindex_period.get_integer()) {
			if (FindNextChangeNotification(directoryChangeHandle) == FALSE) {
				sys_print(Error, "FindNextChangeNotification failed: %d\n", GetLastError());
			}
			sys_print(Debug, "skip reindex%f %f\n", time_now, time_now - last_reindex_time);
			return;

		}

		ASSERT(status == WAIT_OBJECT_0);
		sys_print(Debug, "reindexing assets %f %f\n",time_now, time_now-last_reindex_time);
		reindex_all_assets();
		g_assets.hot_reload_assets();
		last_reindex_time = time_now;

		if (FindNextChangeNotification(directoryChangeHandle) == FALSE) {
			sys_print(Error, "FindNextChangeNotification failed: %d\n", GetLastError());
		}

	}
}


void AssetRegistrySystem::reindex_all_assets()
{
	g_assets.reload_async(hackedAsset, [](GenericAssetPtr) {
		});
}


const ClassTypeInfo* AssetRegistrySystem::find_asset_type_for_ext(const std::string& ext)
{
	for (auto& type : all_assettypes) {
		for (auto& ext_ : type->extensions)
			if (ext_ == ext)
				return type->get_asset_class_type();
	}
	return nullptr;
}
#include "Framework/Config.h"
#include "Assets/AssetDatabase.h"
DECLARE_ENGINE_CMD(TOUCH_ASSET)
{
	if (args.size() != 2) {
		sys_print(Warning, "TOUCH_ASSET <asset path>\n");
		return;
	}
	auto type = AssetRegistrySystem::get().find_asset_type_for_ext(get_extension_no_dot(args.at(1)));
	if (type) {
		auto res = g_assets.find_sync(args.at(1), type, 0);
		if (!res)
			sys_print(Error, "TOUCH_ASSET failed\n");
	}
	else {
		sys_print(Error, "couldnt find type\n");
	}
}
#endif