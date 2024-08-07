#include "MeshComponent.h"
#include "Animation/Runtime/Animation.h"
#include "Animation/AnimationTreePublic.h"
#include "Render/DrawPublic.h"
#include "Render/RenderObj.h"
#include "Render/MaterialPublic.h"

CLASS_IMPL(MeshComponent);


MeshComponent::~MeshComponent()
{
	assert(!animator && !draw_handle.is_valid());
}
MeshComponent::MeshComponent() {}

void MeshComponent::set_model(const char* model_path)
{
	Model* modelnext = mods.find_or_load(model_path);
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
#ifndef RUNTIME
		REG_STDVECTOR(eMaterialOverride, PROP_DEFAULT | PROP_EDITOR_ONLY),
		REG_BOOL(eAnimateInEditor, PROP_DEFAULT | PROP_EDITOR_ONLY, "0"),
#endif // !RUNTIME

		REG_BOOL(simulate_physics, PROP_DEFAULT, "0"),
		REG_BOOL(is_skybox, PROP_DEFAULT, "0")
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
	if (!eMaterialOverride.empty())
		obj.mat_override = eMaterialOverride[0].get();
	idraw->get_scene()->update_obj(draw_handle, obj);
}

void MeshComponent::on_init()
{
	draw_handle = idraw->get_scene()->register_obj();
	if (model.get()) {
		if (model->get_skel() && animator_tree.get() && animator_tree->get_graph_is_valid()) {
			assert(animator_tree->get_script());
			assert(animator_tree->get_script()->get_native_class());
			assert(animator_tree->get_script()->get_native_class()->allocate);

			ClassBase* c = animator_tree->get_script()->get_native_class()->allocate();
			assert(c->is_a<AnimatorInstance>());
			animator.reset(c->cast_to<AnimatorInstance>());

			bool good = animator->initialize_animator(model.get(), animator_tree.get(), get_owner());
			if (!good) {
				sys_print("!!! couldnt initialize animator\n");
				animator.reset(nullptr);	// free animator
				animator_tree = nullptr;	// free tree reference
			}
		}

		Render_Object obj;
		obj.model = model.get();
		obj.visible = visible;
		obj.transform = get_ws_transform();
		obj.owner = this;
		obj.shadow_caster = cast_shadows;
		obj.is_skybox = is_skybox;

		if (!eMaterialOverride.empty())
			obj.mat_override = eMaterialOverride[0].get();

		idraw->get_scene()->update_obj(draw_handle, obj);
	}
}

void MeshComponent::on_changed_transform()
{
	update_handle();
}

void MeshComponent::on_tick()
{

}

void MeshComponent::on_deinit()
{
	idraw->get_scene()->remove_obj(draw_handle);
	animator.reset();
}
