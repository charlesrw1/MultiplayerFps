#include "SpringBoneSetupUtil.h"
#include "SpringBoneSetupComponent.h"
#include "SpringBoneManagerComponent.h"
#include "MeshComponent.h"
#include "Game/Entity.h"
#include "GameEnginePublic.h"

namespace {
void visit(Entity* parent, Entity* source_root, SpringBoneManagerComponent* manager) {
	for (Entity* child : parent->get_children()) {
		auto* setup = child->get_component<SpringBoneSetupComponent>();
		if (!setup)
			continue;

		StringName child_name(child->get_editor_name().c_str());
		StringName parentName;
		bool parent_is_skeleton_bone = false;
		if (parent == source_root) {
			parentName = child->get_parent_bone();
			parent_is_skeleton_bone = !parentName.is_null();
		} else {
			parentName = StringName(parent->get_editor_name().c_str());
		}

		manager->add_spring_bone(child_name, parentName, parent_is_skeleton_bone, child->get_ls_position(),
								  child->get_ls_rotation(), setup->get_params());

		// This setup's own non-setup children are visual attachments -- clone their MeshComponent
		// onto a fresh free entity and ad-hoc parent it to the spring bone we just registered.
		for (Entity* grand : child->get_children()) {
			if (grand->get_component<SpringBoneSetupComponent>())
				continue;
			auto* src_mesh = grand->get_component<MeshComponent>();
			if (!src_mesh || !src_mesh->get_model())
				continue;
			Entity* att = eng->get_level()->spawn_entity();
			att->dont_serialize_or_edit = true;
			// parent_entity_to_spring_bone derives its stored offset as
			// inverse(bone's CURRENT SpringBoneManualState::worldTransform) * att->get_ws_transform().
			// We're calling this immediately after add_spring_bone, before the manager's very first
			// late_update() has ever run, so that state is still its default-constructed identity --
			// meaning the offset it captures is simply att's ws_transform as-is. Set att's ws_transform
			// to exactly the local offset we want (grand's local transform relative to `child`, which
			// IS this spring bone) rather than grand's absolute world transform in the SOURCE scene
			// (which lands wherever the source rig happens to sit, not relative to the bone at all --
			// that mismatch was placing the preview attachment meters away from the simulated bone).
			att->set_ws_transform(grand->get_ls_transform());
			auto* mesh = att->create_component<MeshComponent>();
			mesh->set_model((Model*)src_mesh->get_model());
			manager->parent_entity_to_spring_bone(child_name, att);
		}

		visit(child, source_root, manager);
	}
}
} // namespace

void build_spring_bones_from_setup(Entity* source_root, SpringBoneManagerComponent* manager) {
	if (!source_root || !manager)
		return;
	visit(source_root, source_root, manager);
}
