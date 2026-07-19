#include "SpringBonePreviewComponent.h"
#include "SpringBoneManagerComponent.h"
#include "SpringBoneSetupUtil.h"
#include "MeshComponent.h"
#include "Game/Entity.h"
#include "Level.h"
#include "GameEnginePublic.h"
#include "Animation/AnimationSeqAsset.h"
#include "Animation/Runtime/RuntimeNodesNew2.h"
#include "Framework/Log.h"

#ifdef EDITOR_BUILD
#include "imgui.h"
#endif

void SpringBonePreviewComponent::teardown_preview() {
	if (preview_entity.get())
		preview_entity->destroy();
	preview_entity = obj<Entity>();
}

// Mirrors EntityTargetEditor::find_target()'s lookup (LevelEditor/PropertyEditors.cpp) -- an
// EntityTarget field stores the target's editor name, resolved by scanning the current level.
Entity* SpringBonePreviewComponent::get_source_entity() const {
	if (source_entity.empty())
		return nullptr;
	Level* level = eng->get_level();
	if (!level)
		return nullptr;
	for (auto* o : level->get_all_objects()) {
		if (auto* e = o->cast_to<Entity>()) {
			if (e->get_editor_name() == source_entity)
				return e;
		}
	}
	return nullptr;
}

void SpringBonePreviewComponent::rebuild_preview() {
	teardown_preview();

	Entity* src = get_source_entity();
	if (!src) {
		sys_print(Warning, "SpringBonePreviewComponent: source_entity not set\n");
		return;
	}
	auto* src_mesh = src->get_component<MeshComponent>();
	if (!src_mesh || !src_mesh->get_model()) {
		sys_print(Error, "SpringBonePreviewComponent: source_entity has no MeshComponent/model\n");
		return;
	}

	Entity* pe = eng->get_level()->spawn_entity();
	// dont_serialize (not dont_serialize_or_edit): the latter also makes an entity unselectable/
	// unclickable in the editor (SelectionMode.cpp/EditorDocInput.cpp both filter it out of picking),
	// which would defeat the whole point of a preview entity meant to be moved around to see the
	// spring bones react. dont_serialize alone still skips it on save.
	pe->dont_serialize = true;
	pe->set_ws_transform(get_owner()->get_ws_transform());

	auto* mesh = pe->create_component<MeshComponent>();
	mesh->set_model((Model*)src_mesh->get_model());

	// Same clip-source construction as AnimPreviewComponent::update_mesh_component -- looping clip
	// (bind + delta if additive) when an animation is set, otherwise just the bind pose.
	agBuilder builder;
	const AnimationSeqAsset* asset = preview_animation.get();
	if (asset) {
		auto loop = builder.alloc<agClipNode>();
		loop->set_clip(asset);
		loop->set_looping(true);
		if (asset->seq && asset->seq->is_additive_clip) {
			auto base = builder.alloc<agBindPose>();
			auto add = builder.alloc<agAddNode>();
			add->input0 = base;
			add->input1 = loop;
			add->alpha = 1.f;
			builder.set_root(add);
		} else {
			builder.set_root(loop);
		}
	} else {
		auto bind = builder.alloc<agBindPose>();
		builder.set_root(bind);
	}
	mesh->create_animator(&builder);

	auto* manager = pe->create_component<SpringBoneManagerComponent>();
	manager->set_mesh_component(mesh);
	build_spring_bones_from_setup(src, manager);

	preview_entity = pe;
}

#ifdef EDITOR_BUILD
void SpringBonePreviewComponent::on_inspector_imgui() {
	if (ImGui::Button("Reset"))
		rebuild_preview();
}
#endif
