#pragma once
#include <string>
#include <vector>
#include "glm/glm.hpp"
#include "Assets/IAsset.h"
#include "LevelSerialization/SerializeNew.h"
#include "Game/EntityPtr.h"

class Entity;
class Component;
class ClassTypeInfo;

// PrefabFile — Simple file I/O for .tprefab files
// Not an IAsset — just string-based path and serialization text
class PrefabFile
{
public:
	// Load serialized entity text from a .tprefab file
	// Returns empty string on failure
	static std::string load_text(const std::string& game_relative_path);

	// Save serialized entity text to a .tprefab file
	// Returns true on success
	static bool save_text(const std::string& game_relative_path, const std::string& text);
};

// PrefabAsset — the .tprefab asset: a cached, hot-reloadable copy of the prefab's serialized
// text, plus a "static" tree of Entity/Component objects built from that text but never
// inserted into a Level (post_unserialization/start() are never called on them; see
// NewSerialization::unserialize_from_text with keepid=false). This static tree is one-way,
// read-only data for game/tool code to inspect (transforms, names, bone parents, component
// layout) without spawning anything into the world -- analogous to a Unity Prefab asset.
//
// To actually spawn a live instance into the current Level, use PrefabAsset::spawn().
class PrefabAsset : public IAsset
{
public:
	CLASS_BODY(PrefabAsset);

	bool load_asset() override;
	void post_load() override;
	void uninstall() override;

	const std::string& get_text() const { return cached_text; }

	// Root entities (no parent) of the static, never-started tree. Lazily built on first call
	// and cached; invalidated on reload.
	REF const std::vector<Entity*>& get_root_entities() const;

	// Searches every entity/component in the static, never-started tree (not just roots) for the
	// first one matching. Returns nullptr if none found. Use these to fetch a specific authored
	// node (e.g. a RagdollSetupComponent) off a prefab in order to call a REF'd function on it,
	// such as RagdollSetupComponent::create_ragdoll_entity().
	REF Component* find_component_by_type(const ClassTypeInfo* type) const;
	// Same as find_component_by_type but collects every match instead of just the first
	// (e.g. to gather every MeshComponent in the prefab for thumbnail rendering).
	std::vector<Component*> find_all_components_by_type(const ClassTypeInfo* type) const;
	REF Entity* find_entity_by_name(const std::string& name) const;

	// Loads (or returns the already-loaded) PrefabAsset for `name`, mirroring the
	// Asset::load(name) pattern used by other asset types (e.g. AnimationSeqAsset::load).
	REF static PrefabAsset* load(const std::string& name);

	// Spawns a live instance of this prefab into the current Level: deserializes a fresh,
	// independent copy of the prefab text (never touches the cached static tree above),
	// inserts+starts it, applies `transform` to root entities, and parents unparented roots to
	// `parent_entity` (may be null to leave them at the top level of the Level).
	static std::vector<EntityPtr> spawn(const std::string& prefab_path, const glm::mat4& transform,
										 Entity* parent_entity = nullptr);

	// Applies `scene.hierarchy` links (parent/child, top-level, bone-parent) directly to the
	// already-constructed Entity objects in `scene`, without inserting anything into a Level.
	// Entity::parent_to() is pure linked-list bookkeeping, so this is safe to call standalone.
	// Shared by post_load() (the cached static tree) and any other consumer that deserializes a
	// scratch copy of prefab text and needs the same hierarchy wired up (e.g. drag-drop preview).
	static void wire_hierarchy(UnserializedSceneFile& scene);

private:
	std::string cached_text;
	UnserializedSceneFile static_data;
	mutable std::vector<Entity*> root_entities;
	mutable bool root_entities_valid = false;
	bool first_post_load_done = false;
};

#ifdef EDITOR_BUILD
#include "Assets/AssetRegistry.h"
#include "Framework/Util.h"

// AssetMetadata for .tprefab files in the asset browser
class PrefabAssetMetadata : public AssetMetadata
{
public:
	PrefabAssetMetadata() { extensions.push_back("tprefab"); }
	std::string get_type_name() const override { return "Prefab"; }
	Color32 get_browser_color() const override { return {255, 200, 100}; }
	const ClassTypeInfo* get_asset_class_type() const override { return &PrefabAsset::StaticType; }
};

#endif // EDITOR_BUILD
