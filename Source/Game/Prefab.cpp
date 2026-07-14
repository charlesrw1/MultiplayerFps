#include "Prefab.h"
#include "Framework/Files.h"
#include "Framework/Util.h"
#include "Framework/StringName.h"
#include "Game/Entity.h"
#include "Game/EntityComponent.h"
#include "Level.h"
#include "GameEnginePublic.h"
#include "Assets/AssetDatabase.h"

std::string PrefabFile::load_text(const std::string& game_relative_path) {
	auto file = FileSys::open_read_game(game_relative_path);
	if (!file) {
		sys_print(Warning, "Failed to open prefab file: %s\n", game_relative_path.c_str());
		return "";
	}

	size_t file_size = file->size();
	std::string text;
	text.resize(file_size);
	file->read(text.data(), file_size);
	file->close();

	return text;
}

bool PrefabFile::save_text(const std::string& game_relative_path, const std::string& text) {
	auto file = FileSys::open_write_game(game_relative_path);
	if (!file) {
		sys_print(Warning, "Failed to open prefab file for writing: %s\n", game_relative_path.c_str());
		return false;
	}

	if (!file->write(text.c_str(), text.size())) {
		sys_print(Warning, "Failed to write prefab file: %s\n", game_relative_path.c_str());
		file->close();
		return false;
	}

	file->close();
	return true;
}

bool PrefabAsset::load_asset() {
	cached_text = PrefabFile::load_text(get_name());
	return !cached_text.empty();
}

void PrefabAsset::wire_hierarchy(UnserializedSceneFile& scene) {
	// Mirrors Level::insert_unserialized_entities_into_level_internal's hierarchy application,
	// but standalone -- these entities are never inserted into a Level. Entity::parent_to() is
	// pure linked-list bookkeeping and doesn't touch Level, so this is safe to call here.
	for (auto& link : scene.hierarchy) {
		if (link.parent)
			link.child->parent_to(link.parent);
		if (link.is_top_level)
			link.child->set_is_top_level(true);
		if (link.has_bone)
			link.child->set_parent_bone(StringName(link.parent_bone.c_str()));
	}
}

void PrefabAsset::post_load() {
	static_data.delete_objs();
	root_entities_valid = false;

	if (did_load_fail())
		return;

	try {
		static_data = NewSerialization::unserialize_from_text("prefab_static", cached_text, false);
	}
	catch (const std::exception& e) {
		sys_print(Warning, "PrefabAsset: failed to parse prefab %s: %s\n", get_name().c_str(), e.what());
		return;
	}

	wire_hierarchy(static_data);

	const bool is_reload = first_post_load_done;
	first_post_load_done = true;

	if (is_reload) {
		Level* lvl = eng ? eng->get_level() : nullptr;
		if (lvl) {
			for (BaseUpdater* bu : lvl->get_all_objects()) {
				if (bu && bu->get_type().is_a(Component::StaticType)) {
					static_cast<Component*>(bu)->refresh_after_prefab_reload(this);
				}
			}
		}
	}
}

void PrefabAsset::uninstall() {
	static_data.delete_objs();
	cached_text.clear();
	root_entities.clear();
	root_entities_valid = false;
}

const std::vector<Entity*>& PrefabAsset::get_root_entities() const {
	if (!root_entities_valid) {
		root_entities.clear();
		for (BaseUpdater* bu : static_data.all_obj_vec) {
			if (auto* e = bu->cast_to<Entity>()) {
				if (!e->get_parent())
					root_entities.push_back(e);
			}
		}
		root_entities_valid = true;
	}
	return root_entities;
}

Component* PrefabAsset::find_component_by_type(const ClassTypeInfo* type) const {
	ASSERT(type);
	ASSERT(type->is_a(Component::StaticType));
	for (BaseUpdater* bu : static_data.all_obj_vec) {
		if (bu->get_type().is_a(*type))
			return static_cast<Component*>(bu);
	}
	return nullptr;
}

Entity* PrefabAsset::find_entity_by_name(const std::string& name) const {
	for (BaseUpdater* bu : static_data.all_obj_vec) {
		if (auto* e = bu->cast_to<Entity>()) {
			if (e->get_editor_name() == name)
				return e;
		}
	}
	return nullptr;
}

PrefabAsset* PrefabAsset::load(const std::string& name) {
	return g_assets.find<PrefabAsset>(name).get();
}

std::vector<EntityPtr> PrefabAsset::spawn(const std::string& prefab_path, const glm::mat4& transform,
										   Entity* parent_entity) {
	std::vector<EntityPtr> out;

	auto asset = g_assets.find<PrefabAsset>(prefab_path);
	if (!asset) {
		sys_print(Warning, "PrefabAsset::spawn: failed to load prefab: %s\n", prefab_path.c_str());
		return out;
	}

	try {
		// Deserialize a fresh, independent copy -- never spawn directly from the cached
		// static tree, since that's shared, read-only data for other consumers.
		UnserializedSceneFile unserialized =
			NewSerialization::unserialize_from_text("prefab_spawn", asset->get_text(), false);

		eng->get_level()->insert_unserialized_entities_into_level(unserialized);

		for (auto base_updater : unserialized.all_obj_vec) {
			if (auto* entity = base_updater->cast_to<Entity>()) {
				if (!entity->get_parent()) {
					// Place the whole prefab at `transform` first, while still unparented
					// (ws == ls here), so root entities keep their authored relative layout.
					entity->set_ws_transform(transform * entity->get_ls_transform());
					if (parent_entity) {
						// parent_to() preserves the *local* transform as-is (does not touch it),
						// so re-fix it up to the world placement just computed above -- otherwise
						// the entity would jump to parent_ws * (transform * ls) instead.
						const glm::mat4 placed_ws = entity->get_ws_transform();
						entity->parent_to(parent_entity);
						entity->set_ws_transform(placed_ws);
					}
					out.push_back(entity);
				}
			}
		}
	}
	catch (const std::exception& e) {
		sys_print(Warning, "PrefabAsset::spawn: failed to deserialize prefab %s: %s\n", prefab_path.c_str(),
				  e.what());
	}

	return out;
}

#ifdef EDITOR_BUILD

// Register .tprefab in the asset browser
static auto _register_prefab_metadata = []() {
	auto metadata = new PrefabAssetMetadata();
	AssetRegistrySystem::get().register_asset_type(metadata);
	return true;
}();

#endif  // EDITOR_BUILD
