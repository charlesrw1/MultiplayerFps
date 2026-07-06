#ifdef EDITOR_BUILD
#include "Commands.h"
#include "EditorDocLocal.h"
#include "GameEnginePublic.h"
#include "Framework/Log.h"
#include "Game/Entity.h"
#include "Game/Components/GroupComponent.h"
#include "glm/glm.hpp"

// ---------------------------------------------------------------------------
// ParentToCommand
// ---------------------------------------------------------------------------

ParentToCommand::ParentToCommand(EditorDoc& ed_doc, std::vector<Entity*> children, Entity* new_parent,
								 bool create_new_parent, bool clear_parent, StringName parent_bone)
	: ed_doc(ed_doc), create_new_parent(create_new_parent), clear_parent(clear_parent), parent_bone(parent_bone) {
	this->new_parent = EntityPtr(new_parent);
	for (auto* c : children) {
		if (!c)
			continue;
		SavedChild s;
		s.child = c->get_self_ptr();
		saved.push_back(s);
	}
}

bool ParentToCommand::is_valid() {
	if (!valid_flag)
		return false;
	// Parenting is a prefab-only feature — reject in level-edit mode.
	if (!ed_doc.is_editing_prefab())
		return false;
	if (saved.empty())
		return false;
	if (clear_parent)
		return true; // clearing needs no target
	if (create_new_parent)
		return true; // target is spawned on execute
	Entity* target = new_parent.get();
	if (!target)
		return false;
	// Can't parent an entity to itself.
	for (auto& s : saved)
		if (s.child.get() == target)
			return false;
	return true;
}

void ParentToCommand::execute() {
	// Snapshot each child's prior state so undo can restore it exactly.
	for (auto& s : saved) {
		Entity* child = s.child.get();
		if (!child)
			continue;
		s.prev_parent = EntityPtr(child->get_parent());
		s.prev_top_level = child->get_is_top_level();
		s.prev_bone = child->get_parent_bone();
		s.prev_ws_transform = child->get_ws_transform();
	}

	Entity* target = nullptr;
	if (clear_parent) {
		target = nullptr;
	} else if (create_new_parent) {
		// Spawn an empty group node at the centroid of the children and use it as the parent.
		glm::vec3 centroid(0.f);
		int count = 0;
		for (auto& s : saved) {
			if (Entity* child = s.child.get()) {
				centroid += child->get_ws_position();
				++count;
			}
		}
		if (count > 0)
			centroid /= float(count);

		Entity* empty = ed_doc.spawn_entity();
		ed_doc.attach_component(&GroupComponent::StaticType, empty);
		empty->set_ws_position(centroid);
		empty->set_editor_name("Empty");
		created_parent = empty->get_self_ptr();
		target = empty;
	} else {
		target = new_parent.get();
	}

	for (auto& s : saved) {
		Entity* child = s.child.get();
		if (!child)
			continue;
		const glm::mat4 ws = s.prev_ws_transform; // keep world transform across the reparent
		child->parent_to(target);
		// Always overwrite the bone so it reflects THIS command's intent; parent_to() leaves the
		// old parent_bone untouched, which would otherwise leave a stale bone attachment behind when
		// reparenting away from a bone or clearing the parent. Undo restores prev_bone symmetrically.
		child->set_parent_bone(clear_parent ? StringName() : parent_bone);
		child->set_ws_transform(ws);
	}

	ed_doc.post_node_changes.invoke();
}

void ParentToCommand::undo() {
	// Restore children first, THEN destroy any spawned empty parent — destroying the empty first
	// would cascade-destroy its (still-attached) children.
	for (auto& s : saved) {
		Entity* child = s.child.get();
		if (!child)
			continue;
		child->parent_to(s.prev_parent.get());
		child->set_parent_bone(s.prev_bone);
		child->set_is_top_level(s.prev_top_level);
		child->set_ws_transform(s.prev_ws_transform);
	}

	if (Entity* empty = created_parent.get()) {
		if (ed_doc.selection_state->is_entity_selected(empty))
			ed_doc.selection_state->remove_from_selection(empty);
		empty->destroy();
		created_parent = EntityPtr();
	}

	ed_doc.post_node_changes.invoke();
}

#endif // EDITOR_BUILD
