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
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "AssetDatabase.h"
#include "AssetRegistryLocal.h"
#include "HackedReloader.h"
#include "Framework/Config.h"
#include "Assets/AssetDatabase.h"
#include "AssetTools/AssetTemplates.h"

// Source audio extensions that get a .ais sidecar auto-created (mirrors the equivalent
// static helper in AssetTemplates.cpp, kept in sync manually since it's a tiny set).
static bool is_source_audio_ext(const std::string& ext) {
	return ext == "wav" || ext == "flac" || ext == "mp3" || ext == "aiff" || ext == "ogg";
}

void TOUCH_ASSET(const Cmd_Args& args) {
	if (args.size() != 2) {
		sys_print(Warning, "TOUCH_ASSET <asset path>\n");
		return;
	}
	auto type = AssetRegistrySystem::get().find_asset_type_for_ext(get_extension_no_dot(args.at(1)));
	if (type) {
		auto res = g_assets.generic_find(args.at(1), type);
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
#include "Assets/AssetReferenceQuery.h"

static void print_ref_hits(const std::vector<AssetRefHit>& hits) {
	for (auto& hit : hits)
		sys_print(Info, "%-50s %-20s x%d\n", hit.game_path.c_str(),
			hit.type ? hit.type->get_type_name().c_str() : "?", hit.count);
	sys_print(Info, "%zu reference(s)\n", hits.size());
}

// DECLARE_ENGINE_CMD_CAT("sys.", print_refs)
// Prints what references <asset_path> (backward references).
void SYS_PRINT_REFS_CMD(const Cmd_Args& args) {
	if (args.size() != 2) {
		sys_print(Error, "usage: sys.print_refs <asset_path>\n");
		return;
	}
	print_ref_hits(AssetReferenceQuery::find_backward_references(args.at(1)));
}

// DECLARE_ENGINE_CMD_CAT("sys.", print_deps)
// Prints what <asset_path> itself references (forward references).
void SYS_PRINT_DEPS_CMD(const Cmd_Args& args) {
	if (args.size() != 2) {
		sys_print(Error, "usage: sys.print_deps <asset_path>\n");
		return;
	}
	print_ref_hits(AssetReferenceQuery::find_forward_references(args.at(1)));
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
		Fatalf("AssetRegistrySystem: FileWatcher::init failed for dir '%s' (error %lu)\n", dir.c_str(), GetLastError());
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
		auto asset = g_assets.generic_find(args.at(1), assetType);
		if (was_loaded) {
			g_assets.reload(asset);
		}
	});
}
#include "Game/LevelAssets.h"
#include "Sound/SoundPublic.h"
#include "Render/Texture.h"
#include "Render/MaterialPublic.h"
#include "Render/Model.h"
#include "Scripting/ScriptManager.h"
#include "Game/Particles/ParticleAsset.h"
#include "Render/PostProcessSettings.h"
#include "Game/Prefab.h"
#include "Assets/ScriptableObject.h"
#ifdef EDITOR_BUILD
#include "Assets/AssetBrowser.h"
#endif

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

	auto* texMeta = (AssetMetadata*)find_for_classtype(ClassBase::find_class("Texture"));
	auto* modelMeta = (AssetMetadata*)find_for_classtype(ClassBase::find_class("Model"));
	auto* matMeta = (AssetMetadata*)find_for_classtype(ClassBase::find_class("MaterialInstance"));
	auto* soundMeta = (AssetMetadata*)find_for_classtype(ClassBase::find_class("SoundFile"));
	auto* fontMeta = (AssetMetadata*)find_for_classtype(ClassBase::find_class("GuiFont"));
	auto* mapMeta = (AssetMetadata*)find_for_classtype(ClassBase::find_class("SceneAsset"));
	auto* prefabMeta = (AssetMetadata*)find_type("Prefab");
	auto* particleMeta = (AssetMetadata*)find_for_classtype(ClassBase::find_class("ParticleAsset"));
	auto* ppsetMeta = (AssetMetadata*)find_for_classtype(ClassBase::find_class("PostProcessSettings"));
	auto* sobjMeta = (AssetMetadata*)find_for_classtype(ClassBase::find_class("ScriptableObject"));
	assert(prefabMeta);

	bool tree_dirty = false;
	bool lua_changed = false;

	// Helper: skip hot-reload when the watcher fires for a deletion. In-place
	// reload destroys the asset's old impl before calling load_asset(); on a
	// missing-file failure the asset is then left with a null impl, which any
	// downstream consumer would crash on. Leaving the stale data in memory until
	// the file reappears is strictly safer.
	auto file_exists = [](const std::string& rel) {
		auto f = FileSys::open_read_game(rel);
		const bool ok = (f != nullptr);
		if (f)
			f->close();
		return ok;
	};

	for (auto rel_path : changed) {
		if (rel_path.find(".thumbnails/") != std::string::npos)
			continue;
		sys_print(Info, "AssetRegistry: file changed: %s\n", rel_path.c_str());
		auto ext = StringUtils::get_extension_no_dot(rel_path);

		// Hot-reload in-memory assets
		if (ext == "mm" || ext == "mi") {
			if (g_assets.is_asset_loaded(rel_path) && file_exists(rel_path)) {
				auto asset = g_assets.find<MaterialInstance>(rel_path);
				g_assets.reload<MaterialInstance>(asset);
#ifdef EDITOR_BUILD
				if (AssetBrowser::inst)
					AssetBrowser::inst->thumbnails.invalidate_thumbnail(rel_path);
#endif
			}
		} else if (ext == "mis" || ext == "glb") {
			std::string cmdl = rel_path;
			StringUtils::remove_extension(cmdl);
			cmdl += ".cmdl";
			if (g_assets.is_asset_loaded(cmdl) && file_exists(rel_path)) {
				auto asset = g_assets.find<Model>(cmdl);
				g_assets.reload<Model>(asset);
#ifdef EDITOR_BUILD
				if (AssetBrowser::inst)
					AssetBrowser::inst->thumbnails.invalidate_thumbnail(cmdl);
#endif
			}
		} else if (ext == "png" || ext == "jpg" || ext == "tis") {
			if (ext == "png") {
				std::string tis = rel_path.substr(0, rel_path.size() - 3) + "tis";
				if (!file_exists(tis)) {
					auto created = AssetTemplates::create_tis_for_png(rel_path);
					if (created)
						sys_print(Info, "Auto-import: created %s for %s\n", created->c_str(), rel_path.c_str());
				}
			}
			std::string dds = rel_path;
			StringUtils::remove_extension(dds);
			dds += ".dds";
			if (g_assets.is_asset_loaded(dds) && file_exists(rel_path)) {
				auto asset = g_assets.find<Texture>(dds);
				g_assets.reload<Texture>(asset);
			}
		} else if (ext == "ais" || ext == "csnd") {
			std::string csnd = rel_path;
			StringUtils::remove_extension(csnd);
			csnd += ".csnd";
			if (g_assets.is_asset_loaded(csnd) && file_exists(rel_path)) {
				auto asset = g_assets.find<SoundFile>(csnd);
				g_assets.reload<SoundFile>(asset);
#ifdef EDITOR_BUILD
				if (AssetBrowser::inst)
					AssetBrowser::inst->thumbnails.invalidate_thumbnail(csnd);
#endif
			}
		} else if (is_source_audio_ext(ext)) {
			// Source audio changed live -- auto-import a .ais if this file doesn't have one yet
			// (mirrors the .png auto-import branch above), and hot-reload if already compiled.
			std::string ais = rel_path;
			StringUtils::remove_extension(ais);
			ais += ".ais";
			if (!file_exists(ais)) {
				auto created = AssetTemplates::create_ais_for_wav(rel_path);
				if (created)
					sys_print(Info, "Auto-import: created %s for %s\n", created->c_str(), rel_path.c_str());
			}
		} else if (ext == "lua") {
			ScriptManager::inst->reload_one_file(rel_path);
			lua_changed = true;
		} else if (ext == "particle") {
			if (g_assets.is_asset_loaded(rel_path) && file_exists(rel_path)) {
				auto asset = g_assets.find<ParticleAsset>(rel_path);
				g_assets.reload<ParticleAsset>(asset);
			}
		} else if (ext == "ppset") {
			if (g_assets.is_asset_loaded(rel_path) && file_exists(rel_path)) {
				auto asset = g_assets.find<PostProcessSettings>(rel_path);
				g_assets.reload<PostProcessSettings>(asset);
			}
		} else if (ext == "tprefab") {
			if (g_assets.is_asset_loaded(rel_path) && file_exists(rel_path)) {
				auto asset = g_assets.find<PrefabAsset>(rel_path);
				g_assets.reload<PrefabAsset>(asset);
			}
		} else if (ext == "sobj") {
			if (g_assets.is_asset_loaded(rel_path) && file_exists(rel_path)) {
				auto asset = g_assets.find<ScriptableObject>(rel_path);
				g_assets.reload<ScriptableObject>(asset);
			}
		}

		// Incremental tree update: resolve to the canonical asset entry.
		// UI textures (.tis with no .dds) are represented by their .png path in the browser.
		AssetOnDisk aod;
		aod.filename = rel_path;
		if (ext == "hdr" || ext == "dds") {
			aod.type = texMeta;

			// When a .dds appears or disappears, also reconcile the UI-texture .png entry
			// for the same stem — they are mutually exclusive in the browser.
			std::string stem = strip_extension(rel_path);
			std::string png_path = stem + ".png";
			std::string tis_path = stem + ".tis";
			bool dds_exists = file_exists(rel_path);
			bool png_exists = file_exists(png_path);
			bool tis_exists = file_exists(tis_path);

			if (dds_exists) {
				// .dds just appeared — remove any UI-texture .png entry for this stem
				remove_from_tree(*root, png_path);
			} else if (tis_exists && png_exists) {
				// .dds just disappeared — expose the UI-texture .png entry
				AssetOnDisk png_aod;
				png_aod.filename = png_path;
				png_aod.type = texMeta;
				root->add_path(png_aod, split(png_path, '/'));
			}
		} else if (ext == "tmap")
			aod.type = mapMeta;
		else if (ext == "mm" || ext == "mi")
			aod.type = matMeta;
		else if (ext == "csnd")
			aod.type = soundMeta;
		else if (ext == "fnt")
			aod.type = fontMeta;
		else if (ext == "cmdl")
			aod.type = modelMeta;
		else if (ext == "tprefab")
			aod.type = prefabMeta;
		else if (ext == "particle")
			aod.type = particleMeta;
		else if (ext == "ppset")
			aod.type = ppsetMeta;
		else if (ext == "sobj")
			aod.type = sobjMeta;
		else if (ext == "ais") {
			StringUtils::remove_extension(aod.filename);
			aod.filename += ".csnd";
			aod.type = soundMeta;
		} else if (ext == "mis") {
			StringUtils::remove_extension(aod.filename);
			aod.filename += ".cmdl";
			aod.type = modelMeta;
		} else if (ext == "tis" || ext == "png" || ext == "jpg") {
			// UI-texture browser entry: shown as .png when .tis exists but no .dds.
			std::string stem = strip_extension(rel_path);
			std::string png_path = stem + ".png";
			std::string tis_path = stem + ".tis";
			std::string dds_path = stem + ".dds";

			bool show_as_png = file_exists(tis_path) && file_exists(png_path) && !file_exists(dds_path);
			if (show_as_png) {
				AssetOnDisk png_aod;
				png_aod.filename = png_path;
				png_aod.type = texMeta;
				root->add_path(png_aod, split(png_path, '/'));
			} else {
				remove_from_tree(*root, png_path);
			}
			tree_dirty = true;
			continue; // aod.type is unset; handled above
		} else {
			continue; // not a tracked asset type; skip tree update
		}

		// Check whether the file actually exists now to distinguish add vs delete.
		// Use rel_path (the file the watcher actually saw), not aod.filename: for
		// "mis" the two differ (aod.filename is the not-yet-compiled .cmdl), and
		// checking the compiled output would wrongly prune a freshly-written .mis.
		const bool exists = file_exists(rel_path);

		if (exists) {
			root->add_path(aod, split(aod.filename, '/'));
			tree_dirty = true;
		} else {
			remove_from_tree(*root, aod.filename);
			tree_dirty = true;
		}
	}

	if (lua_changed) {
		tree_dirty = true;
		auto* ctype = find_type("Component-Entity");
		ASSERT(ctype);
		std::vector<std::string> extras;
		ctype->fill_extra_assets(extras);
		auto add = [&](AssetOnDisk aod) { root->add_path(aod, split(aod.filename, '/')); };
		for (auto& e : extras) {
			AssetOnDisk aod;
			aod.filename = std::move(e);
			aod.type = (AssetMetadata*)ctype;
			add(std::move(aod));
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

	auto* texMeta = (AssetMetadata*)find_for_classtype(ClassBase::find_class("Texture"));
	auto* modelMeta = (AssetMetadata*)find_for_classtype(ClassBase::find_class("Model"));
	auto* matMeta = (AssetMetadata*)find_for_classtype(ClassBase::find_class("MaterialInstance"));
	auto* mapMeta = (AssetMetadata*)find_for_classtype(ClassBase::find_class("SceneAsset"));
	auto* soundMeta = (AssetMetadata*)find_for_classtype(ClassBase::find_class("SoundFile"));
	auto* fontMeta = (AssetMetadata*)find_for_classtype(ClassBase::find_class("GuiFont"));
	auto* prefabMeta = (AssetMetadata*)find_type("Prefab");
	auto* particleMeta = (AssetMetadata*)find_for_classtype(ClassBase::find_class("ParticleAsset"));
	auto* ppsetMeta = (AssetMetadata*)find_for_classtype(ClassBase::find_class("PostProcessSettings"));
	auto* sobjMeta = (AssetMetadata*)find_for_classtype(ClassBase::find_class("ScriptableObject"));
	assert(prefabMeta);

	// Use a set to deduplicate model entries: both .cmdl and .mis map to the same .cmdl asset.
	std::unordered_set<std::string> model_paths;
	// Same idea for sound: both .csnd and .ais map to the same .csnd asset.
	std::unordered_set<std::string> sound_paths;

	auto add = [&](AssetOnDisk aod) { root->add_path(aod, split(aod.filename, '/')); };

	// tis_stems: stems that have a .tis but no .dds → shown as .png (UI textures).
	// Populated during the scan and resolved after all .dds entries are collected.
	std::unordered_set<std::string> dds_stems;
	std::vector<std::string>        tis_stems;

	for (const auto& full : FileSys::find_game_files()) {
		auto gp = FileSys::get_game_path_from_full_path(full);
		if (gp.find(".thumbnails/") != std::string::npos)
			continue;
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

		if (ext == "csnd" || ext == "ais") {
			// Both .csnd and .ais (.ais is import settings that produce a .csnd) resolve to the
			// same tree entry.  Collect and deduplicate before adding.
			if (ext == "ais") {
				StringUtils::remove_extension(gp);
				gp += ".csnd";
			}
			sound_paths.insert(std::move(gp));
			continue;
		}

		if (ext == "tis") {
			tis_stems.push_back(strip_extension(gp));
			continue;
		}

		AssetOnDisk aod;
		aod.filename = std::move(gp);
		if (ext == "hdr" || ext == "dds") {
			aod.type = texMeta;
			dds_stems.insert(strip_extension(aod.filename));
		} else if (ext == "tmap")
			aod.type = mapMeta;
		else if (ext == "mm" || ext == "mi")
			aod.type = matMeta;
		else if (ext == "fnt")
			aod.type = fontMeta;
		else if (ext == "tprefab")
			aod.type = prefabMeta;
		else if (ext == "particle")
			aod.type = particleMeta;
		else if (ext == "ppset")
			aod.type = ppsetMeta;
		else if (ext == "sobj")
			aod.type = sobjMeta;
		else
			continue;

		add(std::move(aod));
	}

	// UI textures: .tis exists but no compiled .dds — show as .png in the browser.
	// Game textures always produce a .dds, so they're already in the tree above.
	for (const auto& stem : tis_stems) {
		if (dds_stems.count(stem))
			continue; // game texture — already shown as .dds
		std::string png_path = stem + ".png";
		auto f = FileSys::open_read_game(png_path);
		if (!f) continue; // .png not present either
		f->close();
		AssetOnDisk aod;
		aod.filename = std::move(png_path);
		aod.type = texMeta;
		add(std::move(aod));
	}

	for (auto& m : model_paths) {
		AssetOnDisk aod;
		aod.filename = m;
		aod.type = modelMeta;
		add(std::move(aod));
	}

	for (auto& s : sound_paths) {
		AssetOnDisk aod;
		aod.filename = s;
		aod.type = soundMeta;
		add(std::move(aod));
	}

	// Let individual asset type handlers inject synthetic entries (e.g. procedural assets)
	for (auto& type : all_assettypes) {
		std::vector<std::string> extras;
		type->fill_extra_assets(extras);
		for (auto& e : extras) {
			AssetOnDisk aod;
			aod.filename = std::move(e);
			aod.type = type.get();
			add(std::move(aod));
		}
	}

	root->sort_R();
	root->set_parent_R();
	rebuild_linear_list_();

	sys_print(Debug, "AssetRegistry: reindexed %zu assets in %.1f ms\n", linear_list.size(), (GetTime() - t0) * 1000.0);
}

const ClassTypeInfo* AssetRegistrySystem::find_asset_type_for_ext(const std::string& ext) {

	for (auto& type : all_assettypes) {
		for (auto& ext_ : type->extensions)
			if (ext_ == ext)
				return type->get_asset_class_type();
	}
	return nullptr;
}

const AssetMetadata* AssetRegistrySystem::find_metadata_for_ext(const std::string& ext) const {
	for (auto& type : all_assettypes) {
		for (auto& ext_ : type->extensions)
			if (ext_ == ext)
				return type.get();
	}
	return nullptr;
}

#endif
