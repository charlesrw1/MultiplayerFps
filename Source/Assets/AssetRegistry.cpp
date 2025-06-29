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
#include "AssetRegistryLocal.h"

#include "Framework/Config.h"
#include "Assets/AssetDatabase.h"
void TOUCH_ASSET(const Cmd_Args& args)
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



// Ill just put this code here
//DECLARE_ENGINE_CMD_CAT("sys.", ls)
void SYS_LS_CMD(const Cmd_Args& args)
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
			out += "|." + ext + "\\b";
		}
	}
	return out;
}


//DECLARE_ENGINE_CMD_CAT("sys.", print_refs)
void SYS_PRINT_REFS_CMD(const Cmd_Args& args)
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

//DECLARE_ENGINE_CMD_CAT("sys.", print_deps)
void SYS_PRINT_DEPS_CMD(const Cmd_Args& args)
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

void AssetFilesystemNode::sort_R()
{
	sorted_list.clear();
	for (auto& c : children) {
		sorted_list.push_back(&c.second);
	}
	std::sort(sorted_list.begin(), sorted_list.end(), [](AssetFilesystemNode* l, AssetFilesystemNode* r) {
		if(l->is_folder()!=r->is_folder())
			return (int)l->is_folder() > (int)r->is_folder();
		return l->name < r->name;
	});
	for (auto& c : children)
		c.second.sort_R();
}

// too lazy, just hack the asset async loader to do this haha
CLASS_H(HackedAsyncAssetRegReindex, IAsset)
public:
	void uninstall() override {

	}
	void move_construct(IAsset* other) override {
		this->root = std::move(((HackedAsyncAssetRegReindex*)other)->root);
	}
	void sweep_references(IAssetLoadingInterface*) const override {
	}
	bool load_asset(IAssetLoadingInterface*) override {
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

		assert(root_to_clone);
		root = std::make_unique< AssetFilesystemNode>(*root_to_clone.get());

		root->set_is_used_to_false_R();
		for (auto& a : diskAssets) {
			auto& filename = a.filename;
			std::vector<std::string> path = split(filename, '/');
			root->add_path(a, path);
		}
		root->remove_unused_R();
		root->sort_R();
		root->set_parent_R();

		return true;
	}
	void post_load() override {
		is_in_loading = false;
		AssetRegistrySystem::get().root = std::move(root);

		sys_print(Debug, "AssetRegistry: finished assset reindex\n");
	}
	std::unique_ptr<AssetFilesystemNode> root;

	static bool is_in_loading;
	static std::unique_ptr<AssetFilesystemNode> root_to_clone;
};
CLASS_IMPL(HackedAsyncAssetRegReindex);

bool HackedAsyncAssetRegReindex::is_in_loading = false;
std::unique_ptr<AssetFilesystemNode> HackedAsyncAssetRegReindex::root_to_clone;

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

	consoleCommands = ConsoleCmdGroup::create("");
	consoleCommands->add("sys.ls", SYS_LS_CMD);
	consoleCommands->add("sys.print_deps", SYS_PRINT_DEPS_CMD);
	consoleCommands->add("sys.print_refs", SYS_PRINT_REFS_CMD);
	consoleCommands->add("TOUCH_ASSET", TOUCH_ASSET);
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
	using HA = HackedAsyncAssetRegReindex;
	if (!HA::is_in_loading) {
		if (root.get())
			HA::root_to_clone = std::make_unique<AssetFilesystemNode>(*root.get());
		else
			HA::root_to_clone = std::make_unique<AssetFilesystemNode>();
		HA::is_in_loading = true;
		g_assets.reload_async(hackedAsset, [](GenericAssetPtr) {
			});
	}
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



#endif
