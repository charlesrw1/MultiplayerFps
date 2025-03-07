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

#include "GameAnimationMgr.h"

GameAnimationMgr g_gameAnimationMgr;

#ifdef EDITOR_BUILD
const char* MeshComponent::get_editor_outliner_icon() const {
	return animator_tree.get() ? "eng/editor/anim_icon.png" : "eng/editor/mesh_icon.png";
}
#endif

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
		sync_render_data();
	}
}
void MeshComponent::set_model(Model* modelnext)
{
	if (modelnext != model.get()) {
		model = modelnext;
		sync_render_data();
	}
}

void MeshComponent::set_animation_graph(Animation_Tree_CFG* graph)
{
	if (graph != animator_tree.get()) {
		animator_tree.ptr = graph;
		update_animator_instance();
		sync_render_data();
	}
}

void MeshComponent::set_material_override(const MaterialInstance* mi)
{
	if (eMaterialOverride.empty())
		eMaterialOverride.push_back({ (MaterialInstance*)mi });
	else
		eMaterialOverride[0] = { (MaterialInstance*)mi };
	sync_render_data();
}

void MeshComponent::editor_on_change_property()
{
	sync_render_data();
}

void MeshComponent::on_sync_render_data()
{
	if(!draw_handle.is_valid())
		draw_handle = idraw->get_scene()->register_obj();
	Render_Object obj;
	obj.model = model.get();
	obj.visible = is_visible;
#ifdef  EDITOR_BUILD
	obj.visible &= !get_owner()->get_hidden_in_editor();
	obj.outline = get_owner()->get_is_any_selected_in_editor();
#endif //  EDITOR_BUILD

	obj.transform = get_ws_transform();
	obj.owner = this;
	obj.is_skybox = is_skybox;
	obj.shadow_caster = cast_shadows;
	if (animator)
		obj.animator_bone_ofs = animator->get_matrix_palette_offset();
	if (!eMaterialOverride.empty())
		obj.mat_override = eMaterialOverride[0].get();
	idraw->get_scene()->update_obj(draw_handle, obj);
}



void MeshComponent::update_animator_instance()
{
	auto pre_animator = animator.get();
	auto modToUse = (model.did_fail()) ? g_modelMgr.get_error_model() : model.get();
	{
		if (modToUse&&modToUse->get_skel() && animator_tree.get() && animator_tree->get_graph_is_valid()) {

			AnimatorInstance* c = animator_tree->allocate_animator_class();
			animator.reset(c);

			bool good = animator->initialize(modToUse, animator_tree.get(), get_owner());
			if (!good) {
				sys_print(Error, "couldnt initialize animator\n");
				animator.reset();
				animator_tree = nullptr;	// free tree reference
			}
		}
		else {
			animator.reset();
		}
	}
}

void MeshComponent::pre_start()
{
	get_owner()->set_cached_mesh_component(this);
	update_animator_instance();
	sync_render_data();
	set_ticking(false);
}

void MeshComponent::start()
{
}
#include "tracy/public/tracy/Tracy.hpp"
void MeshComponent::on_changed_transform()
{
	sync_render_data();
}

void MeshComponent::update()
{

}

void MeshComponent::end()
{
	get_owner()->set_cached_mesh_component(nullptr);
	idraw->get_scene()->remove_obj(draw_handle);
	animator.reset();
}

const Model* MeshComponent::get_model() const { return model.get(); }
const Animation_Tree_CFG* MeshComponent::get_animation_tree() const { return animator_tree.get(); }

const MaterialInstance* MeshComponent::get_material_override() const {
	return eMaterialOverride.empty() ? nullptr : eMaterialOverride[0].get();
}

static ConfigVar animation_bonemat_arena_size("animation_bonemat_arena_size", "15000", CVAR_DEV | CVAR_INTEGER | CVAR_READONLY, "arena size of animation bone matricies in matrix size (64)", 0, FLT_MAX);

GameAnimationMgr::GameAnimationMgr() : animating_meshcomponents(4) {

}
GameAnimationMgr::~GameAnimationMgr()
{
}
void GameAnimationMgr::init()
{
	matricies_allocated = animation_bonemat_arena_size.get_integer();
	matricies = new glm::mat4[matricies_allocated];
}

void GameAnimationMgr::remove_from_animating_set(AnimatorInstance* mc)
{
	//ASSERT(animating_meshcomponents.find(mc));
	animating_meshcomponents.remove(mc);
}
void GameAnimationMgr::add_to_animating_set(AnimatorInstance* mc)
{
	//ASSERT(!animating_meshcomponents.find(mc));
	animating_meshcomponents.insert(mc);
}
#include "Debug.h"
extern ConfigVar g_debug_skeletons;

static void draw_skeleton(const AnimatorInstance* a, float line_len, const glm::mat4& transform)
{
	auto& bones = a->get_global_bonemats();
	auto model = a->get_model();
	if (!model || !model->get_skel())
		return;

	auto skel = model->get_skel();
	for (int index = 0; index < skel->get_num_bones(); index++) {
		vec3 org = transform * bones[index][3];
		Color32 colors[] = { COLOR_RED,COLOR_GREEN,COLOR_BLUE };
		for (int i = 0; i < 3; i++) {
			vec3 dir = glm::mat3(transform) * bones[index][i];
			dir = normalize(dir);
			Debug::add_line(org, org + dir * line_len, colors[i], -1.f, false);
		}
		const int parent = skel->get_bone_parent(index);
		if (parent != -1) {
			vec3 parent_org = transform * bones[parent][3];
			Debug::add_line(org, parent_org, COLOR_PINK, -1.f, false);
		}
	}
}
#include "Framework/Jobs.h"
#include "tracy/public/tracy/Tracy.hpp"

void GameAnimationMgr::update_animating()
{
	ZoneScoped;

	matricies_used = 0;

	for (AnimatorInstance* ai : animating_meshcomponents) {
		if (ai) {
			if (matricies_used + ai->num_bones() > matricies_allocated)
				Fatalf("animator out of memory\n");
			ai->set_matrix_palette_offset(matricies_used);
			ai->update(eng->get_dt());
			matricies_used += ai->num_bones();

			if (ai->get_owner())
				ai->get_owner()->invalidate_transform(nullptr);

			if (g_debug_skeletons.get_bool()) {
				draw_skeleton(ai, 0.05, ai->get_owner()->get_ws_transform());
			}
		}
	}
}