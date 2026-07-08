#pragma once

#include "AllHeader.h"
#include <string>
#include <vector>

class DragDropPreview
{
public:
	void set_preview_model(Model* m, const glm::mat4& where);
	void set_preview_component(const ClassTypeInfo* t, const glm::mat4& where);
	// Ghost-preview a prefab: spawns one ghost entity per source entity that owns a
	// MeshComponent or a light component (no decals, no gameplay components -- those are
	// never spawned/started). `where` places the whole prefab; each ghost keeps its
	// authored relative offset within the prefab.
	void set_preview_prefab(const std::string& path, const glm::mat4& where);
	void tick();

private:
	void fixup_entity();
	void fixup_ghost_entity(Entity* root);
	void delete_obj();
	bool had_state_set = false;
	enum class State
	{
		None,
		PreviewModel,
		PreviewPrefab,
		PreviewComponent
	} state = State::None;
	Model* preview_model = nullptr;
	const ClassTypeInfo* preview_comp = nullptr;
	obj<Entity> obj_ptr;

	// PreviewPrefab state only: one ghost entity per qualifying source entity, plus its
	// authored local-space transform (composed up the prefab's source hierarchy) so it can be
	// repositioned each frame as `where * prefab_ghost_local_transforms[i]` without rebuilding.
	std::string preview_prefab_path;
	std::vector<obj<Entity>> prefab_ghosts;
	std::vector<glm::mat4> prefab_ghost_local_transforms;
};