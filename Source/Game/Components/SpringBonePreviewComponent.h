#pragma once
#include "Game/EntityComponent.h"
#include "Game/EntityPtr.h"
#include "Assets/IAsset.h"
#include <string>

class AnimationSeqAsset;

// Editor-only preview driver: clones source_entity's mesh + its SpringBoneSetupComponent hierarchy
// (see SpringBoneSetupUtil.h) into a separate, transient (dont_serialize) preview entity carrying a
// live SpringBoneManagerComponent, so spring bones authored on source_entity can be tuned and watched
// jiggle without ever simulating source_entity itself. The preview entity IS selectable/movable in
// the editor (dont_serialize, not dont_serialize_or_edit -- see rebuild_preview()) -- move this
// component's owner (or the preview entity directly) around the scene to see the result.
// Distinct from the animator/skeleton's own baked spring bones (SpringBones.h) -- this is a
// component/entity-based system for bones that aren't part of the official skeleton.
class SpringBonePreviewComponent : public Component
{
public:
	CLASS_BODY(SpringBonePreviewComponent, spawnable);
	SpringBonePreviewComponent() { set_call_init_in_editor(true); }

	// Name (Entity::get_editor_name()) of the entity with a MeshComponent + SpringBoneSetupComponent
	// children -- an entity-name reference, matching the engine's other EntityTarget fields (e.g.
	// fpsSpawnPoint::other), rather than a serialized obj<Entity> handle.
	REFLECT(type = EntityTarget);
	std::string source_entity;

	REF AssetPtr<AnimationSeqAsset> preview_animation; // optional; loops on the preview mesh if set

	Entity* get_source_entity() const;

	void start() final { rebuild_preview(); }
	void stop() final { teardown_preview(); }
#ifdef EDITOR_BUILD
	void editor_start() final { start(); }
	void on_inspector_imgui() final;
	void editor_on_change_property() override { rebuild_preview(); }
#endif

	// Tears down any previous preview and rebuilds it from the current source_entity/setups. Safe to
	// call repeatedly (e.g. after editing a setup's params) -- this is what the "Reset" button does.
	void rebuild_preview();

	bool is_previewing() const { return preview_entity.get() != nullptr; }

private:
	obj<Entity> preview_entity; // transient, dont_serialize = true (selectable/movable, not saved)

	void teardown_preview();
};
