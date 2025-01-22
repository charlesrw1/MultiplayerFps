#include "MeshComponent.h"
#include "Animation/Runtime/Animation.h"
#include "Animation/AnimationTreePublic.h"
#include "Render/DrawPublic.h"
#include "Render/RenderObj.h"
#include "Render/MaterialPublic.h"

#include "Physics/ChannelsAndPresets.h"
#include "Assets/AssetDatabase.h"
#include "GameEnginePublic.h"
#include "Game/Entity.h"
#include "Animation/SkeletonData.h"

CLASS_IMPL(MeshComponent);


MeshComponent::~MeshComponent()
{
	assert(!animator && !draw_handle.is_valid());
}
MeshComponent::MeshComponent() {
}
glm::mat4 MeshComponent::get_ls_transform_of_bone(StringName bonename) const
{
	auto mod = model.get();
	if (!mod || !mod->get_skel())
		return glm::mat4(1);
	auto& allbones = mod->get_skel()->get_all_bones();
	int index = 0;
	for (auto& bone : allbones) {
		if (bone.name == bonename) {
			break;
		}
		index++;
	}
	if (index == allbones.size())
		return glm::mat4(1);
	if (animator)
		return animator->get_global_bonemats().at(index);
	else
		return allbones.at(index).posematrix;
}
void MeshComponent::set_model(const char* model_path)
{
	Model* modelnext = GetAssets().find_sync<Model>(model_path).get();
	if (modelnext != model.get()) {
		model = modelnext;
		on_changed_transform();	//fixme
	}
}
void MeshComponent::set_model(Model* modelnext)
{
	if (modelnext != model.get()) {
		model = modelnext;
		on_changed_transform();	//fixme
	}
}
void MeshComponent::set_animation_graph(const char* graph)
{
	Animation_Tree_CFG* tree = GetAssets().find_assetptr_unsafe<Animation_Tree_CFG>(graph);
	if (tree != animator_tree.get()) {

		animator_tree.ptr = tree;
		//animator.reset();
		//if (animator_tree) {
		//	AnimatorInstance* c = animator_tree->allocate_animator_class();
		//	animator.reset(c);
		//
		//	bool good = animator->initialize_animator(model.get(), animator_tree.get(), get_owner());
		//	if (!good) {
		//		sys_print(Error, "couldnt initialize animator\n");
		//		animator.reset(nullptr);	// free animator
		//		animator_tree = nullptr;	// free tree reference
		//	}
		//	else
		//		set_ticking(true);	// start ticking the animator
		//}
	}
}

void MeshComponent::set_material_override(const MaterialInstance* mi)
{
	if (eMaterialOverride.empty())
		eMaterialOverride.push_back({ (MaterialInstance*)mi });
	else
		eMaterialOverride[0] = { (MaterialInstance*)mi };
	on_changed_transform();	//fixme
}

const PropertyInfoList* MeshComponent::get_props() {
#ifndef RUNTIME
	MAKE_VECTORCALLBACK_ATOM(AssetPtr<MaterialInstance>, eMaterialOverride);
#endif // !RUNTIME
	MAKE_VECTORCALLBACK_ATOM(AssetPtr<MaterialInstance>, MaterialOverride_compilied)

		auto t = &Model::StaticType.classname;
	const char* str = Model::StaticType.classname;
	START_PROPS(MeshComponent)
		REG_ASSET_PTR(model, PROP_DEFAULT),
		REG_ASSET_PTR(animator_tree, PROP_DEFAULT),
		REG_BOOL(cast_shadows, PROP_DEFAULT, "1"),
		REG_BOOL(is_skybox, PROP_DEFAULT, "0"),
#ifndef RUNTIME
		REG_STDVECTOR(eMaterialOverride, PROP_DEFAULT | PROP_EDITOR_ONLY),
		REG_BOOL(e_animate_in_editor, PROP_DEFAULT | PROP_EDITOR_ONLY, "0"),
#endif // !RUNTIME
	END_PROPS(MeshCompponent)
}

void MeshComponent::editor_on_change_property()
{
	update_handle();
}

void MeshComponent::update_handle()
{
	if (!model.get())
		return;

	Render_Object obj;
	obj.model = model.get();
	obj.visible = visible;
	obj.transform = get_ws_transform();
	obj.owner = this;
	obj.is_skybox = is_skybox;
	obj.shadow_caster = cast_shadows;
	obj.outline = get_owner()->is_selected_in_editor();
	if (animator)
		obj.animator = animator.get();
	if (!eMaterialOverride.empty())
		obj.mat_override = eMaterialOverride[0].get();
	idraw->get_scene()->update_obj(draw_handle, obj);
}

void MeshComponent::on_init()
{
	draw_handle = idraw->get_scene()->register_obj();

	get_owner()->set_cached_mesh_component(this);
	
	if (model.get()||model.did_fail()) {

		auto modToUse = (model.did_fail()) ? mods.get_error_model() : model.get();

		if (!eng->is_editor_level()) {
			if (modToUse->get_skel() && animator_tree.get() && animator_tree->get_graph_is_valid()) {

				AnimatorInstance* c = animator_tree->allocate_animator_class();
				animator.reset(c);

				bool good = animator->initialize_animator(modToUse, animator_tree.get(), get_owner());
				if (!good) {
					sys_print(Error, "couldnt initialize animator\n");
					animator.reset(nullptr);	// free animator
					animator_tree = nullptr;	// free tree reference
				}
				else
					set_ticking(true);	// start ticking the animator
			}
		}

		Render_Object obj;
		obj.model = modToUse;
		obj.visible = visible;
		obj.transform = get_ws_transform();
		obj.owner = this;
		obj.shadow_caster = cast_shadows;
		obj.is_skybox = is_skybox;
		if (animator)
			obj.animator = animator.get();

		if (!eMaterialOverride.empty())
			obj.mat_override = eMaterialOverride[0].get();

		idraw->get_scene()->update_obj(draw_handle, obj);
	}
}

void MeshComponent::on_changed_transform()
{
	if(draw_handle.is_valid())
		update_handle();

}

void MeshComponent::update()
{
	if (animator) {
		animator->tick_tree_new(eng->get_tick_interval());
		get_owner()->invalidate_transform(this);
	}
}

void MeshComponent::on_deinit()
{
	get_owner()->set_cached_mesh_component(nullptr);

	idraw->get_scene()->remove_obj(draw_handle);
	animator.reset();
}

const Model* MeshComponent::get_model() const { return model.get(); }
