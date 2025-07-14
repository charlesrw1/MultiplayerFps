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
#include "HackedReloader.h"
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

#include <chrono>
int64_t filetime_to_unix_seconds(uint64_t filetime) {
	// Epoch difference between Jan 1, 1601 and Jan 1, 1970 in 100-ns units
	constexpr uint64_t EPOCH_DIFF_100NS = 116444736000000000ULL;

	// Convert FILETIME (100-ns intervals) to seconds since Unix epoch
	uint64_t ticks_since_unix_epoch_100ns = filetime - EPOCH_DIFF_100NS;
	return static_cast<int64_t>(ticks_since_unix_epoch_100ns / 10'000'000ULL);
}
int64_t get_unix_time_seconds() {
	return std::chrono::duration_cast<std::chrono::seconds>(
		std::chrono::system_clock::now().time_since_epoch()
		).count();
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

	out += " --glob \"*.tmap\" ";
	out += " --glob \"*.pfb\" ";
	out += " --glob \"*.lua\" ";
	out += " --glob \"*.mm\" ";
	out += " --glob \"*.mi\" ";


	std::vector<const char*> exts = { "tmap","pfb","lua","mm","mi" };

	auto& types = AssetRegistrySystem::get().get_types();
	for (auto& ext : exts) {
		out += " --glob \"*." + std::string(ext) + "\" ";
	}
	return out;
}
static std::string get_asset_references_pattern() {
	std::string out;
	out += ".mis\\b|.tis\\b";
	auto& types = AssetRegistrySystem::get().get_types();
	std::vector<const char*> exts = { "tmap","pfb","lua","mm","mi","cmdl","dds","wav"};
	for (auto& ext : exts) {
		out += "|." + std::string(ext) + "\\b";
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

ConfigVar asset_registry_reindex_period("asset_registry_reindex_period", "20", CVAR_DEV | CVAR_FLOAT, "time in seconds of registry reindexing");

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




static HANDLE directoryChangeHandle = 0;

#include "Framework/StringUtils.h"
void AssetRegistrySystem::init()
{
	last_time_check = get_unix_time_seconds();

	directoryChangeHandle = FindFirstChangeNotificationA(
		".\\",		 // directory to watch 
		TRUE,                         //  watch subtree 
		FILE_NOTIFY_CHANGE_LAST_WRITE); 

	if (directoryChangeHandle == INVALID_HANDLE_VALUE) {
		Fatalf("ERROR: AssetRegistrySystem::init: FindFirstChangeNotificationA failed: %s\n", GetLastError());
	}


	reindex_all_assets();

	consoleCommands = ConsoleCmdGroup::create("");
	consoleCommands->add("sys.ls", SYS_LS_CMD);
	consoleCommands->add("sys.print_deps", SYS_PRINT_DEPS_CMD);
	consoleCommands->add("sys.print_refs", SYS_PRINT_REFS_CMD);
	consoleCommands->add("touch_asset", TOUCH_ASSET);
	consoleCommands->add("reload_asset", [this](const Cmd_Args& args) {
		if (args.size() != 2) {
			sys_print(Error, "expected: reload_asset <name>\n");
			return;
		}
		auto assetType = find_asset_type_for_ext(StringUtils::get_extension_no_dot(args.at(1)));
		if (!assetType) {
			sys_print(Error, "couldn't find asset type for asset\n");
			return;
		}
		const bool was_loaded = g_assets.is_asset_loaded(args.at(1));
		auto asset = g_assets.find_sync(args.at(1), assetType);
		if (was_loaded) {
			g_assets.reload_sync(asset);
		}
		});
}
#include "Game/LevelAssets.h"
#include "Sound/SoundPublic.h"
#include "Render/Texture.h"
#include "Render/MaterialPublic.h"
#include "Render/Model.h"
#include "Scripting/ScriptManager.h"
#include <future>
using ChangedPaths = std::vector<std::string>;
std::vector<std::string> async_launch_check_filesystem_changes(int64_t last_time_check) {
	std::vector<std::string> out;
	for (const string& file : FileSys::find_game_files()) {
		auto gamepath = FileSys::get_game_path_from_full_path(file);
		auto filePtr = FileSys::open_read_game(gamepath);
		if (!filePtr)
			continue;
		int64_t in_unix_time = filetime_to_unix_seconds(filePtr->get_timestamp());
		filePtr->close();
		if (in_unix_time >= last_time_check) {
			out.push_back(gamepath);
		}
	}
	return out;
}
static std::future<ChangedPaths> future_changed_paths;
static bool is_waiting_on_future = false;

bool update_on_changed_paths(ChangedPaths changes) {

	bool wants_reindex = false;
	// just do the stupid way
	for (string& gamepath : changes) {

		// reload
		sys_print(Info, "found new asset %s\n", gamepath.c_str());
		auto ext = StringUtils::get_extension_no_dot(gamepath);
		const bool prev = wants_reindex;
		wants_reindex = true;
		if (ext == "mm" || ext == "mi") {
			if (g_assets.is_asset_loaded(gamepath)) {
				auto asset = g_assets.find_sync<MaterialInstance>(gamepath);
				g_assets.reload_sync<MaterialInstance>(asset);
			}
		}
		else if (ext == "mis" || ext == "glb") {
			StringUtils::remove_extension(gamepath);
			gamepath += ".cmdl";
			if (g_assets.is_asset_loaded(gamepath)) {
				auto asset = g_assets.find_sync<Model>(gamepath);
				g_assets.reload_sync<Model>(asset);
			}
		}
		else if (ext == "png" || ext == "jpg" || ext == "tis") {
			StringUtils::remove_extension(gamepath);
			gamepath += ".dds";
			if (g_assets.is_asset_loaded(gamepath)) {
				auto asset = g_assets.find_sync<Texture>(gamepath);
				g_assets.reload_sync<Texture>(asset);
			}
		}
		else if (ext == "pfb") {
			if (g_assets.is_asset_loaded(gamepath)) {
				auto asset = g_assets.find_sync<PrefabAsset>(gamepath);
				g_assets.reload_sync<PrefabAsset>(asset);
			}
		}
		else if (ext == "wav") {
			if (g_assets.is_asset_loaded(gamepath)) {
				auto asset = g_assets.find_sync<SoundFile>(gamepath);
				g_assets.reload_sync<SoundFile>(asset);
			}
		}
		else if (ext == "lua") {
			ScriptManager::inst->reload_one_file(gamepath);
		}
		else if (ext == "tmap") {
			//...
		}
		else {
			wants_reindex = prev;
		}
	}
	return wants_reindex;
}

void AssetRegistrySystem::update()
{
	if (is_waiting_on_future) {
		if (future_changed_paths.wait_for(std::chrono::milliseconds(1)) == std::future_status::ready) {
			ChangedPaths changes = future_changed_paths.get();
			bool wants_reindex = update_on_changed_paths(changes);
			if (wants_reindex)
				reindex_all_assets();
			is_waiting_on_future = false;
		}
	}

	double time_now = TimeSinceStart();
	{
		auto status = WaitForMultipleObjects(1, &directoryChangeHandle, TRUE, 0);
		if (status == WAIT_TIMEOUT)
			return;
		if (status == WAIT_FAILED) {
			sys_print(Error, "WaitForMultipleObjects failed: %s\n", GetLastError());
			return;
		}
		sys_print(Debug, "status == WAIT_OBJECT_0 (%f) (%f)\n",float(time_now), float(last_reindex_time));
		const float period = 5.0;
		if ((time_now - last_reindex_time) <= period || is_waiting_on_future) {
			if (FindNextChangeNotification(directoryChangeHandle) == FALSE) {
				sys_print(Error, "FindNextChangeNotification failed: %d\n", GetLastError());
			}
			sys_print(Debug, "skip reindex %f %f\n", float(time_now), float(time_now - last_reindex_time));
			return;
		}

		ASSERT(status == WAIT_OBJECT_0);

		future_changed_paths = std::async(std::launch::async, async_launch_check_filesystem_changes, last_time_check);
		is_waiting_on_future = true;

		last_time_check = get_unix_time_seconds();
		last_reindex_time = time_now;
		if (FindNextChangeNotification(directoryChangeHandle) == FALSE) {
			sys_print(Error, "FindNextChangeNotification failed: %d\n", GetLastError());
		}

		sys_print(Debug, "AssetRegistrySystem::update: time %f\n", float(TimeSinceStart()-time_now));

	}
}


void AssetRegistrySystem::reindex_all_assets()
{
	using HA = HackedAsyncAssetRegReindex;
	if (!root.get()) 
		root = std::make_unique<AssetFilesystemNode>();
	HackedAsyncAssetRegReindex blahblah;
	double now = GetTime();
	blahblah.load_asset(nullptr,*root.get());
	blahblah.post_load();
	sys_print(Debug, "AssetRegistry: finished assset reindex in %f\n",float(GetTime()-now));
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


void HackedAsyncAssetRegReindex::post_load()  {
	AssetRegistrySystem::get().root = std::move(root);
}
#include "Framework/MapUtil.h"
bool HackedAsyncAssetRegReindex::load_asset(IAssetLoadingInterface*, AssetFilesystemNode& rootToClone)  {
	std::vector<AssetOnDisk> diskAssets;
	auto& all_assettypes = AssetRegistrySystem::get().all_assettypes;
	AssetMetadata* texMetadata = (AssetMetadata*)AssetRegistrySystem::get().find_for_classtype(ClassBase::find_class("Texture"));
	AssetMetadata* modelMeta = (AssetMetadata*)AssetRegistrySystem::get().find_for_classtype(ClassBase::find_class("Model"));
	AssetMetadata* pfbMeta = (AssetMetadata*)AssetRegistrySystem::get().find_for_classtype(ClassBase::find_class("PrefabAsset"));
	AssetMetadata* matMeta = (AssetMetadata*)AssetRegistrySystem::get().find_for_classtype(ClassBase::find_class("MaterialInstance"));
	AssetMetadata* mapMeta = (AssetMetadata*)AssetRegistrySystem::get().find_for_classtype(ClassBase::find_class("SceneAsset"));
	AssetMetadata* soundMeta = (AssetMetadata*)AssetRegistrySystem::get().find_for_classtype(ClassBase::find_class("SoundFile"));
	AssetMetadata* fontMeta = (AssetMetadata*)AssetRegistrySystem::get().find_for_classtype(ClassBase::find_class("GuiFont"));

	for (const auto& file : FileSys::find_game_files()) {
		auto ext = get_extension_no_dot(file);
		auto gamepath = FileSys::get_game_path_from_full_path(file);
		AssetOnDisk aod;
		aod.filename = std::move(gamepath);
		if (ext == "hdr" || ext == "dds") {
			aod.type = texMetadata;
			diskAssets.push_back(aod);
		}
		else if (ext == "tmap") {
			aod.type = mapMeta;
			diskAssets.push_back(aod);
		}
		else if (ext == "pfb") {
			aod.type = pfbMeta;
			diskAssets.push_back(aod);
		}
		else if (ext == "mm" || ext == "mi") {
			aod.type = matMeta;
			diskAssets.push_back(aod);
		}
		else if (ext == "cmdl") {
			aod.type = modelMeta;
			diskAssets.push_back(aod);
		}
		else if (ext == "wav") {
			aod.type = soundMeta;
			diskAssets.push_back(aod);
		}
		else if (ext == "fnt") {
			aod.type = fontMeta;
			diskAssets.push_back(aod);
		}
	}

	for (int i = 0; i < all_assettypes.size(); i++){
		auto type = all_assettypes[i].get();
		std::vector<std::string> extraAssets;
		type->fill_extra_assets(extraAssets);
		for (int j = 0; j < extraAssets.size(); j++) {
			AssetOnDisk aod;
			aod.filename = std::move(extraAssets[j]);
			aod.type = type;
			diskAssets.push_back(aod);
		}
	}

	root = std::make_unique< AssetFilesystemNode>(rootToClone);

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

#endif
