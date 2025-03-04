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

static std::vector<std::string> split_path(const std::string& path)
{
	std::vector<std::string> out;
	int start = 0;
	auto find = path.find('/');
	while (find != std::string::npos) {
		out.push_back(path.substr(start, find));
		start = find+1;
		find = path.find('/',find+1);
	}
	if(start!=path.size())
		out.push_back(path.substr(start));
	return out;
}

// Ill just put this code here
DECLARE_ENGINE_CMD_CAT("sys.", ls)
{
	std::string dir = args.size() == 2 ? args.at(1) : "";
	auto tree = FileSys::find_game_files_path(dir);
	const int len = strlen(FileSys::get_game_path());
	for (auto f : tree) {
		if (f.find(FileSys::get_game_path()) == 0)
			f = f.substr(len + 1);
		auto type = AssetRegistrySystem::get().find_asset_type_for_ext(get_extension_no_dot(f));
		if (!type)
			continue;
		sys_print(Info, "%-40s %s\n", f.c_str(), type->classname);
	}
}
#include "Render/Texture.h"
#include "Render/Model.h"
#include "Sound/SoundPublic.h"
static std::string get_valid_asset_types_glob() {
	std::string out;

	out += " --glob \"*.mis\" ";	// model import settings
	out += " --glob \"*.tis\" ";	// texture import settings

	auto& types = AssetRegistrySystem::get().get_types();
	for (auto& t : types) {
		if (!t->assets_are_filepaths()) continue;
		// hacky stuff, these are binary formats
		if (t->get_asset_class_type() == &Texture::StaticType) continue;
		if (t->get_asset_class_type() == &Model::StaticType) continue;
		if (t->get_asset_class_type() == &SoundFile::StaticType) continue;
		for (auto& ext : t->extensions) {
			out += " --glob \"*." + ext + "\" ";
		}
	}
	return out;
}
static std::string get_asset_references_pattern() {
	std::string out;
	out += ".mis\\b|.tis\\b";
	auto& types = AssetRegistrySystem::get().get_types();
	for (auto& t : types) {
		if (!t->assets_are_filepaths()) continue;
		// hacky stuff, these are binary formats
		for (auto& ext : t->extensions) {
			out += "|."+ext+"\\b";
		}
	}
	return out;
}


DECLARE_ENGINE_CMD_CAT("sys.", print_refs)
{
	if (args.size() != 2) {
		sys_print(Error, "usage: sys.print_refs <asset_path>\n");
		return;
	}

	std::string parentDir = FileSys::get_full_path_from_game_path(args.at(1));
	auto findSlash = parentDir.rfind('/');
	if (findSlash != std::string::npos)
		parentDir = parentDir.substr(0, findSlash + 1);

	const std::string rg = "./x64/Debug/rg.exe ";
	std::string commandLine = rg + '\'' + std::string(args.at(1)) + "\' " + std::string(FileSys::get_game_path()) + "/ " + get_valid_asset_types_glob();

	STARTUPINFOA si = {};
	PROCESS_INFORMATION out = {};
	commandLine = "powershell.exe -Command \""+ commandLine+"\"";
	//commandLine = "dir\n";
	sys_print(Info, "executing search: %s\n", commandLine.c_str());
	if (!CreateProcessA(nullptr, (char*)commandLine.c_str(), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si, &out)) {
		sys_print(Error, "couldn't create process\n");
		return;
	}
	WaitForSingleObject(out.hProcess, INFINITE);
	CloseHandle(out.hProcess);
	CloseHandle(out.hThread);
}

DECLARE_ENGINE_CMD_CAT("sys.", print_deps)
{
	if (args.size() != 2) {
		sys_print(Error, "usage: sys.print_deps <asset_path>\n");
		return;
	}

	std::string full_path = FileSys::get_full_path_from_game_path(args.at(1));
	
	const std::string rg = "./x64/Debug/rg.exe ";
	std::string commandLine = rg + "\'" + get_asset_references_pattern() + "\' " + full_path;

	STARTUPINFOA si = {};
	PROCESS_INFORMATION out = {};
	commandLine = "powershell.exe -Command \"" + commandLine + "\"";
	//commandLine = "dir\n";
	sys_print(Info, "executing search: %s\n", commandLine.c_str());
	if (!CreateProcessA(nullptr, (char*)commandLine.c_str(), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si, &out)) {
		sys_print(Error, "couldn't create process\n");
		return;
	}
	WaitForSingleObject(out.hProcess, INFINITE);
	CloseHandle(out.hProcess);
	CloseHandle(out.hThread);
}


#endif
