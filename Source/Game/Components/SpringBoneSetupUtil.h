#pragma once

class Entity;
class SpringBoneManagerComponent;

// Walks source_root's descendant entities for SpringBoneSetupComponents (any depth, DFS) and
// registers each one on `manager` via SpringBoneManagerComponent::add_spring_bone. `manager` must
// already have set_mesh_component() pointed at the entity that owns the actual simulated mesh (the
// preview's cloned MeshComponent, or source_root's own for a direct non-preview use). Reusable
// independent of SpringBonePreviewComponent so other code (e.g. a runtime spawn) can build a live
// manager from the same authored data.
//
// Parent resolution per setup entity `child`:
//   - if its Entity parent is source_root itself: bone-parented directly to source_root's skeleton
//     via child->get_parent_bone() (matches RagdollSetupComponent's one-level-deep bone-parenting
//     convention) -- a null bone name falls back to the manager owner's root transform.
//   - otherwise: parented to the nearest ancestor SpringBoneSetupComponent, named by that ancestor
//     entity's own get_editor_name().
// localPos/localRot come straight from child->get_ls_position()/get_ls_rotation(), since Entity's
// local transform is already relative to whatever it's parented to (bone or entity).
//
// Any child entity of a setup that does NOT itself carry a SpringBoneSetupComponent is treated as a
// visual attachment: if it has a MeshComponent with a model, a fresh free entity is spawned, given a
// MeshComponent with the same model, world-positioned to match the original attachment entity, and
// ad-hoc parented to that spring bone via manager->parent_entity_to_spring_bone -- destroyed by
// SpringBoneManagerComponent::stop() along with everything else it manages.
void build_spring_bones_from_setup(Entity* source_root, SpringBoneManagerComponent* manager);
