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
#include "Game/Components/MeshbuilderComponent.h"

#include "Framework/ArrayReflection.h"

#include "Game/AssetPtrMacro.h"
#include "Game/AssetPtrArrayMacro.h"
#include "Scripting/FunctionReflection.h"

#include "Framework/VectorReflect2.h"
#include "Render/ModelManager.h"

MeshComponent::~MeshComponent()
{
	assert(!animator && !draw_handle.is_valid());
}
MeshComponent::MeshComponent() {
	set_call_init_in_editor(true);
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

int MeshComponent::get_index_of_bone(StringName bonename) const
{
	auto mod = model.get();
	if (!mod || !mod->get_skel())
		return -1;
	auto& allbones = mod->get_skel()->get_all_bones();
	int index = 0;
	for (auto& bone : allbones) {
		if (bone.name == bonename) {
			return index;
		}
		index++;
	}
	return -1;
}

void MeshComponent::set_model_str(const char* model_path)
{
	Model* modelnext = g_assets.find_sync<Model>(model_path).get();
	if (modelnext != model.get()) {
		model = modelnext;
		update_handle();
	}
}
void MeshComponent::set_model(Model* modelnext)
{
	if (modelnext != model.get()) {
		model = modelnext;
		update_handle();
	}
}

void MeshComponent::set_animation_graph(Animation_Tree_CFG* graph)
{
	if (graph != animator_tree.get()) {
		animator_tree.ptr = graph;
		update_animator_instance();
	}
}

void MeshComponent::set_material_override(const MaterialInstance* mi)
{
	if (eMaterialOverride.empty())
		eMaterialOverride.push_back({ (MaterialInstance*)mi });
	else
		eMaterialOverride[0] = { (MaterialInstance*)mi };
	update_handle();	//fixme
}

void MeshComponent::editor_on_change_property()
{
	update_handle();
}

void MeshComponent::update_handle()
{
	if (!draw_handle.is_valid())
		return;
	Render_Object obj;
	obj.model = model.get();
	obj.visible = is_visible;
	obj.transform = get_ws_transform();
	obj.owner = this;
	obj.is_skybox = is_skybox;
	obj.shadow_caster = cast_shadows;
	obj.outline = get_owner()->is_selected_in_editor();
	if (animator)
		obj.animator = animator;
	if (!eMaterialOverride.empty())
		obj.mat_override = eMaterialOverride[0].get();
	idraw->get_scene()->update_obj(draw_handle, obj);
}

void MeshComponent::update_animator_instance()
{
	auto modToUse = (model.did_fail()) ? g_modelMgr.get_error_model() : model.get();
	bool should_set_ticking = false;
	if (!eng->is_editor_level()) {
		if (modToUse->get_skel() && animator_tree.get() && animator_tree->get_graph_is_valid()) {

			AnimatorInstance* c = animator_tree->allocate_animator_class();
			delete animator;
			animator = c;

			bool good = animator->initialize(modToUse, animator_tree.get(), get_owner());
			if (!good) {
				sys_print(Error, "couldnt initialize animator\n");
				delete animator;
				animator = nullptr;
				animator_tree = nullptr;	// free tree reference
			}
			else
				should_set_ticking = true;	// start ticking the animator
		}
		else {
			delete animator;
			animator = nullptr;
		}
	}
	ASSERT(should_set_ticking == (animator!=nullptr));
	set_ticking(should_set_ticking);
}

void MeshComponent::pre_start()
{
	draw_handle = idraw->get_scene()->register_obj();

	get_owner()->set_cached_mesh_component(this);
	
	update_animator_instance();
	update_handle();
}

void MeshComponent::start()
{
}

void MeshComponent::on_changed_transform()
{
	update_handle();
}

void MeshComponent::update()
{
	if (animator) {
		animator->update(eng->get_dt());

		get_owner()->invalidate_transform(this);
	}
	else {
		sys_print(Warning, "non-animated mesh component found ticking\n");
		set_ticking(false);	// shouldnt be ticking
	}
}

void MeshComponent::end()
{
	get_owner()->set_cached_mesh_component(nullptr);
	idraw->get_scene()->remove_obj(draw_handle);
	delete animator;
	animator = nullptr;
}

const Model* MeshComponent::get_model() const { return model.get(); }
const Animation_Tree_CFG* MeshComponent::get_animation_tree() const { return animator_tree.get(); }

const MaterialInstance* MeshComponent::get_material_override() const {
	return eMaterialOverride.empty() ? nullptr : eMaterialOverride[0].get();
}
