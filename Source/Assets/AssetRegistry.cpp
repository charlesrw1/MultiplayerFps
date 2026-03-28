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

void TOUCH_ASSET(const Cmd_Args& args) {
	if (args.size() != 2) {
		sys_print(Warning, "TOUCH_ASSET <asset path>\n");
		return;
	}
	auto type = AssetRegistrySystem::get().find_asset_type_for_ext(get_extension_no_dot(args.at(1)));
	if (type) {
		auto res = g_assets.find_sync(args.at(1), type, 0);
		if (!res)
			sys_print(Error, "TOUCH_ASSET failed\n");
	} else {
		sys_print(Error, "couldnt find type\n");
	}
}


// Ill just put this code here
// DECLARE_ENGINE_CMD_CAT("sys.", ls)
void SYS_LS_CMD(const Cmd_Args& args) {
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

	out += " --glob \"*.mis\" "; // model import settings
	out += " --glob \"*.tis\" "; // texture import settings

	out += " --glob \"*.tmap\" ";
	out += " --glob \"*.lua\" ";
	out += " --glob \"*.mm\" ";
	out += " --glob \"*.mi\" ";

	std::vector<const char*> exts = {"tmap", "lua", "mm", "mi"};

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
	std::vector<const char*> exts = {"tmap", "lua", "mm", "mi", "cmdl", "dds", "wav"};
	for (auto& ext : exts) {
		out += "|." + std::string(ext) + "\\b";
	}
	return out;
}

// DECLARE_ENGINE_CMD_CAT("sys.", print_refs)
void SYS_PRINT_REFS_CMD(const Cmd_Args& args) {
	if (args.size() != 2) {
		sys_print(Error, "usage: sys.print_refs <asset_path>\n");
		return;
	}

	std::string parentDir = FileSys::get_full_path_from_game_path(args.at(1));
	auto findSlash = parentDir.rfind('/');
	if (findSlash != std::string::npos)
		parentDir = parentDir.substr(0, findSlash + 1);

	const std::string rg = "./x64/Debug/rg.exe ";
	std::string commandLine = rg + '\'' + std::string(args.at(1)) + "\' " + std::string(FileSys::get_game_path()) +
							  "/ " + get_valid_asset_types_glob();

	STARTUPINFOA si = {};
	PROCESS_INFORMATION out = {};
	commandLine = "powershell.exe -Command \"" + commandLine + "\"";
	// commandLine = "dir\n";
	sys_print(Info, "executing search: %s\n", commandLine.c_str());
	if (!CreateProcessA(nullptr, (char*)commandLine.c_str(), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si, &out)) {
		sys_print(Error, "couldn't create process\n");
		return;
	}
	WaitForSingleObject(out.hProcess, INFINITE);
	CloseHandle(out.hProcess);
	CloseHandle(out.hThread);
}

// DECLARE_ENGINE_CMD_CAT("sys.", print_deps)
void SYS_PRINT_DEPS_CMD(const Cmd_Args& args) {
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
	// commandLine = "dir\n";
	sys_print(Info, "executing search: %s\n", commandLine.c_str());
	if (!CreateProcessA(nullptr, (char*)commandLine.c_str(), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si, &out)) {
		sys_print(Error, "couldn't create process\n");
		return;
	}
	WaitForSingleObject(out.hProcess, INFINITE);
	CloseHandle(out.hProcess);
	CloseHandle(out.hThread);
}

AssetRegistrySystem& AssetRegistrySystem::get() {
	static AssetRegistrySystem inst;
	return inst;
}


static std::vector<std::string> split(const std::string& str, char delimiter) {
	std::vector<std::string> tokens;
	std::stringstream ss(str);
	std::string token;

	while (std::getline(ss, token, delimiter)) {
		tokens.push_back(token);
	}

	return tokens;
}

void AssetFilesystemNode::sort_R() {
	sorted_list.clear();
	for (auto& c : children) {
		sorted_list.push_back(&c.second);
	}
	std::sort(sorted_list.begin(), sorted_list.end(), [](AssetFilesystemNode* l, AssetFilesystemNode* r) {
		if (l->is_folder() != r->is_folder())
			return (int)l->is_folder() > (int)r->is_folder();
		return l->name < r->name;
	});
	for (auto& c : children)
		c.second.sort_R();
}

extern ConfigVar g_project_base;
#include "Framework/StringUtils.h"
void AssetRegistrySystem::init() {
	string dir = g_project_base.get_string();

	if (!file_watcher_.init(dir)) {
		Fatalf("AssetRegistrySystem: FileWatcher::init failed for dir '%s' (error %lu)\n",
			dir.c_str(), GetLastError());
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

// Remove a leaf node from the tree by game path, pruning any now-empty parent directories.
static void remove_from_tree(AssetFilesystemNode& root, const std::string& game_path) {
	auto parts = split(game_path, '/');
	if (parts.empty())
		return;

	std::vector<AssetFilesystemNode*> stack;
	stack.reserve(parts.size());
	stack.push_back(&root);

	for (size_t i = 0; i + 1 < parts.size(); ++i) {
		auto it = stack.back()->children.find(parts[i]);
		if (it == stack.back()->children.end())
			return; // path not present in tree
		stack.push_back(&it->second);
	}

	stack.back()->children.erase(parts.back());

	// Prune now-empty ancestor directories bottom-up
	for (int i = static_cast<int>(stack.size()) - 1; i > 0; --i) {
		if (!stack[i]->children.empty())
			break;
		stack[i - 1]->children.erase(parts[i - 1]);
	}
}

void AssetRegistrySystem::rebuild_linear_list_() {
	linear_list.clear();
	auto recurse = [](auto&& self, std::vector<AssetFilesystemNode*>& out, AssetFilesystemNode* node) -> void {
		if (node->children.empty())
			out.push_back(node);
		for (auto* child : node->sorted_list)
			self(self, out, child);
	};
	recurse(recurse, linear_list, root.get());
}

void AssetRegistrySystem::update() {
	// FileWatcher gives us exactly the paths that changed — no full scan needed.
	auto changed = file_watcher_.poll(300 /*debounce ms*/);
	if (changed.empty())
		return;

	auto* texMeta   = (AssetMetadata*)find_for_classtype(ClassBase::find_class("Texture"));
	auto* modelMeta = (AssetMetadata*)find_for_classtype(ClassBase::find_class("Model"));
	auto* matMeta   = (AssetMetadata*)find_for_classtype(ClassBase::find_class("MaterialInstance"));
	auto* soundMeta = (AssetMetadata*)find_for_classtype(ClassBase::find_class("SoundFile"));
	auto* fontMeta  = (AssetMetadata*)find_for_classtype(ClassBase::find_class("GuiFont"));
	auto* mapMeta   = (AssetMetadata*)find_for_classtype(ClassBase::find_class("SceneAsset"));

	bool tree_dirty = false;

	for (auto rel_path : changed) {
		sys_print(Info, "AssetRegistry: file changed: %s\n", rel_path.c_str());
		auto ext = StringUtils::get_extension_no_dot(rel_path);

		// Hot-reload in-memory assets
		if (ext == "mm" || ext == "mi") {
			if (g_assets.is_asset_loaded(rel_path)) {
				auto asset = g_assets.find_sync<MaterialInstance>(rel_path);
				g_assets.reload_sync<MaterialInstance>(asset);
			}
		} else if (ext == "mis" || ext == "glb") {
			std::string cmdl = rel_path;
			StringUtils::remove_extension(cmdl);
			cmdl += ".cmdl";
			if (g_assets.is_asset_loaded(cmdl)) {
				auto asset = g_assets.find_sync<Model>(cmdl);
				g_assets.reload_sync<Model>(asset);
			}
		} else if (ext == "png" || ext == "jpg" || ext == "tis") {
			std::string dds = rel_path;
			StringUtils::remove_extension(dds);
			dds += ".dds";
			if (g_assets.is_asset_loaded(dds)) {
				auto asset = g_assets.find_sync<Texture>(dds);
				g_assets.reload_sync<Texture>(asset);
			}
		} else if (ext == "wav") {
			if (g_assets.is_asset_loaded(rel_path)) {
				auto asset = g_assets.find_sync<SoundFile>(rel_path);
				g_assets.reload_sync<SoundFile>(asset);
			}
		} else if (ext == "lua") {
			ScriptManager::inst->reload_one_file(rel_path);
		}

		// Incremental tree update: resolve to the canonical asset entry
		AssetOnDisk aod;
		aod.filename = rel_path;
		if (ext == "hdr" || ext == "dds")
			aod.type = texMeta;
		else if (ext == "tmap")
			aod.type = mapMeta;
		else if (ext == "mm" || ext == "mi")
			aod.type = matMeta;
		else if (ext == "wav")
			aod.type = soundMeta;
		else if (ext == "fnt")
			aod.type = fontMeta;
		else if (ext == "cmdl")
			aod.type = modelMeta;
		else if (ext == "mis") {
			StringUtils::remove_extension(aod.filename);
			aod.filename += ".cmdl";
			aod.type = modelMeta;
		} else {
			continue; // not a tracked asset type; skip tree update
		}

		// Check whether the file actually exists now to distinguish add vs delete
		auto f = FileSys::open_read_game(aod.filename);
		const bool exists = (f != nullptr);
		if (f)
			f->close();

		if (exists) {
			root->add_path(aod, split(aod.filename, '/'));
			tree_dirty = true;
		} else if (aod.type != modelMeta) {
			// Safe to remove non-model assets immediately.
			// Models are left in place: a deleted .cmdl is often about to be
			// recompiled, and we'd rather tolerate a brief stale entry than
			// flicker the asset browser.
			remove_from_tree(*root, aod.filename);
			tree_dirty = true;
		}
	}

	if (tree_dirty) {
		root->sort_R();
		root->set_parent_R();
		rebuild_linear_list_();
	}
}

void AssetRegistrySystem::reindex_all_assets() {
	double t0 = GetTime();

	root = std::make_unique<AssetFilesystemNode>();

	auto* texMeta   = (AssetMetadata*)find_for_classtype(ClassBase::find_class("Texture"));
	auto* modelMeta = (AssetMetadata*)find_for_classtype(ClassBase::find_class("Model"));
	auto* matMeta   = (AssetMetadata*)find_for_classtype(ClassBase::find_class("MaterialInstance"));
	auto* mapMeta   = (AssetMetadata*)find_for_classtype(ClassBase::find_class("SceneAsset"));
	auto* soundMeta = (AssetMetadata*)find_for_classtype(ClassBase::find_class("SoundFile"));
	auto* fontMeta  = (AssetMetadata*)find_for_classtype(ClassBase::find_class("GuiFont"));

	// Use a set to deduplicate model entries: both .cmdl and .mis map to the same .cmdl asset.
	std::unordered_set<std::string> model_paths;

	auto add = [&](AssetOnDisk aod) { root->add_path(aod, split(aod.filename, '/')); };

	for (const auto& full : FileSys::find_game_files()) {
		auto gp  = FileSys::get_game_path_from_full_path(full);
		auto ext = get_extension_no_dot(gp);

		if (ext == "cmdl" || ext == "mis") {
			// Both .cmdl and .mis (.mis is import settings that produce a .cmdl) resolve to the
			// same tree entry.  Collect and deduplicate before adding.
			if (ext == "mis") {
				StringUtils::remove_extension(gp);
				gp += ".cmdl";
			}
			model_paths.insert(std::move(gp));
			continue;
		}

		AssetOnDisk aod;
		aod.filename = std::move(gp);
		if      (ext == "hdr" || ext == "dds") aod.type = texMeta;
		else if (ext == "tmap")                aod.type = mapMeta;
		else if (ext == "mm" || ext == "mi")   aod.type = matMeta;
		else if (ext == "wav")                 aod.type = soundMeta;
		else if (ext == "fnt")                 aod.type = fontMeta;
		else continue;

		add(std::move(aod));
	}

	for (auto& m : model_paths) {
		AssetOnDisk aod;
		aod.filename = m;
		aod.type     = modelMeta;
		add(std::move(aod));
	}

	// Let individual asset type handlers inject synthetic entries (e.g. procedural assets)
	for (auto& type : all_assettypes) {
		std::vector<std::string> extras;
		type->fill_extra_assets(extras);
		for (auto& e : extras) {
			AssetOnDisk aod;
			aod.filename = std::move(e);
			aod.type     = type.get();
			add(std::move(aod));
		}
	}

	root->sort_R();
	root->set_parent_R();
	rebuild_linear_list_();

	sys_print(Debug, "AssetRegistry: reindexed %zu assets in %.1f ms\n",
	          linear_list.size(), (GetTime() - t0) * 1000.0);
}

const ClassTypeInfo* AssetRegistrySystem::find_asset_type_for_ext(const std::string& ext) {

	for (auto& type : all_assettypes) {
		for (auto& ext_ : type->extensions)
			if (ext_ == ext)
				return type->get_asset_class_type();
	}
	return nullptr;
}

void HackedAsyncAssetRegReindex::post_load() {
	AssetRegistrySystem::get().root = std::move(root);
	AssetRegistrySystem::get().linear_list = {};
	auto recurse = [](auto&& self, vector<AssetFilesystemNode*>& outlist, AssetFilesystemNode* node) -> void {
		if (node->children.empty())
			outlist.push_back(node);
		for (auto a : node->sorted_list)
			self(self, outlist, a);
	};
	recurse(recurse, AssetRegistrySystem::get().linear_list, AssetRegistrySystem::get().root.get());
}
#include <unordered_set>
#include "Framework/MapUtil.h"
bool HackedAsyncAssetRegReindex::load_asset(IAssetLoadingInterface*, AssetFilesystemNode& rootToClone) {
	std::vector<AssetOnDisk> diskAssets;
	auto& all_assettypes = AssetRegistrySystem::get().all_assettypes;
	AssetMetadata* texMetadata =
		(AssetMetadata*)AssetRegistrySystem::get().find_for_classtype(ClassBase::find_class("Texture"));
	AssetMetadata* modelMeta =
		(AssetMetadata*)AssetRegistrySystem::get().find_for_classtype(ClassBase::find_class("Model"));
	AssetMetadata* matMeta =
		(AssetMetadata*)AssetRegistrySystem::get().find_for_classtype(ClassBase::find_class("MaterialInstance"));
	AssetMetadata* mapMeta =
		(AssetMetadata*)AssetRegistrySystem::get().find_for_classtype(ClassBase::find_class("SceneAsset"));
	AssetMetadata* soundMeta =
		(AssetMetadata*)AssetRegistrySystem::get().find_for_classtype(ClassBase::find_class("SoundFile"));
	AssetMetadata* fontMeta =
		(AssetMetadata*)AssetRegistrySystem::get().find_for_classtype(ClassBase::find_class("GuiFont"));

	std::unordered_set<string> models;
	for (const auto& file : FileSys::find_game_files()) {
		auto ext = get_extension_no_dot(file);
		auto gamepath = FileSys::get_game_path_from_full_path(file);
		AssetOnDisk aod;
		aod.filename = std::move(gamepath);
		if (ext == "hdr" || ext == "dds") {
			aod.type = texMetadata;
			diskAssets.push_back(aod);
		} else if (ext == "tmap") {
			aod.type = mapMeta;
			diskAssets.push_back(aod);
		} else if (ext == "mm" || ext == "mi") {
			aod.type = matMeta;
			diskAssets.push_back(aod);
		} else if (ext == "cmdl") {
			models.insert(aod.filename);
		} else if (ext == "mis") {
			auto& file = aod.filename;
			StringUtils::remove_extension(file);
			file += ".cmdl";
			models.insert(file);
		} else if (ext == "wav") {
			aod.type = soundMeta;
			diskAssets.push_back(aod);
		} else if (ext == "fnt") {
			aod.type = fontMeta;
			diskAssets.push_back(aod);
		}
	}
	for (auto& m : models) {
		AssetOnDisk aod;
		aod.filename = std::move(m);
		aod.type = modelMeta;
		diskAssets.push_back(aod);
	}

	for (int i = 0; i < all_assettypes.size(); i++) {
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

	root = std::make_unique<AssetFilesystemNode>(rootToClone);

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
