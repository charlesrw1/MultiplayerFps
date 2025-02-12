#include "BillboardComponent.h"
#include "Render/Texture.h"
#include "Render/MaterialPublic.h"
#include "Render/DrawPublic.h"
#include "Render/RenderObj.h"
#include "Render/Model.h"
#include "Game/Entity.h"
#include "Game/AssetPtrMacro.h"
#include "Framework/ReflectionMacros.h"
#include "Render/ModelManager.h"

CLASS_IMPL(BillboardComponent);

BillboardComponent::BillboardComponent() {
	set_call_init_in_editor(true);
}
BillboardComponent::~BillboardComponent() {
	
	//assert(dynamicMaterial == nullptr);
	assert(!handle.is_valid());
}

void BillboardComponent::start() {
	dynamicMaterial = imaterials->create_dynmaic_material(imaterials->get_default_billboard());
	dynamicMaterial->set_tex_parameter(NAME("Sprite"), texture.get());

	handle = idraw->get_scene()->register_obj();

	Render_Object obj;
	fill_out_render_obj(obj);
	idraw->get_scene()->update_obj(handle, obj);
}
void BillboardComponent::end() {
	idraw->get_scene()->remove_obj(handle);
}
void BillboardComponent::on_changed_transform() {
	Render_Object obj;
	fill_out_render_obj(obj);
	idraw->get_scene()->update_obj(handle, obj);
}
void BillboardComponent::editor_on_change_property() {

	dynamicMaterial->set_tex_parameter(NAME("Sprite"), texture.get());
	Render_Object obj;
	fill_out_render_obj(obj);
	idraw->get_scene()->update_obj(handle, obj);
}
void BillboardComponent::set_texture(const Texture* tex) {
	if (tex == texture.get())
		return;
	texture.ptr = (Texture*)tex; // FIXME
	if (!handle.is_valid())	// could be initialization setting
		return;


	dynamicMaterial->set_tex_parameter(NAME("Sprite"), texture.get());
	Render_Object obj;
	fill_out_render_obj(obj);
	idraw->get_scene()->update_obj(handle, obj);
}

void BillboardComponent::fill_out_render_obj(Render_Object& obj)
{
	obj.visible = visible;
	obj.model = g_modelMgr.get_default_plane_model();
	ASSERT(dynamicMaterial.get());
	obj.mat_override = dynamicMaterial.get();
	obj.transform = glm::translate(glm::mat4(1), get_ws_position());
	obj.shadow_caster = false;
	obj.owner = this;
	obj.outline = get_owner()->is_selected_in_editor();
}
const PropertyInfoList* BillboardComponent::get_props() {
	START_PROPS(BillboardComponent)
		REG_ASSET_PTR(texture, PROP_DEFAULT),
		REG_BOOL(visible, PROP_DEFAULT, "1"),
	END_PROPS(BillboardComponent)
}