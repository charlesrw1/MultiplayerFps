#include "BillboardComponent.h"
#include "Render/Texture.h"
#include "Render/MaterialPublic.h"
#include "Render/DrawPublic.h"
#include "Render/RenderObj.h"
#include "Render/Model.h"
#include "Game/Entity.h"

#include "Framework/ReflectionMacros.h"
#include "Render/ModelManager.h"

// comment 1
static int VARIABLE = 0;

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

	sync_render_data();
}
void BillboardComponent::stop() {
	idraw->get_scene()->remove_obj(handle);
}
void BillboardComponent::on_changed_transform() {
	sync_render_data();
}
#ifdef EDITOR_BUILD
void BillboardComponent::editor_on_change_property() {

	dynamicMaterial->set_tex_parameter(NAME("Sprite"), texture.get());
	sync_render_data();
}
#endif
void BillboardComponent::set_texture(const Texture* tex) {
	if (tex == texture.get())
		return;
	texture.ptr = (Texture*)tex; // FIXME
	if (!handle.is_valid())	// could be initialization setting
		return;
	dynamicMaterial->set_tex_parameter(NAME("Sprite"), texture.get());
	sync_render_data();
}

void BillboardComponent::on_sync_render_data()
{
	if (!handle.is_valid())
		handle = idraw->get_scene()->register_obj();

	Render_Object obj;
	obj.visible = visible;
#ifdef  EDITOR_BUILD
	obj.visible &= !get_owner()->get_hidden_in_editor();
	obj.outline = get_owner()->get_is_any_selected_in_editor();
#endif //  EDITOR_BUILD

	obj.model = g_modelMgr.get_default_plane_model();
	ASSERT(dynamicMaterial.get());
	obj.mat_override = dynamicMaterial.get();
	obj.transform = glm::translate(glm::mat4(1), get_ws_position());
	obj.shadow_caster = false;
	obj.owner = this;

	idraw->get_scene()->update_obj(handle, obj);
}
