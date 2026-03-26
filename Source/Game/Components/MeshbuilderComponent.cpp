#include "MeshbuilderComponent.h"
#include "Game/Entity.h"
// comment 4

void MeshBuilderComponent::on_sync_render_data() {
	if (!editor_mb_handle.is_valid())
		editor_mb_handle = idraw->get_scene()->register_meshbuilder();
	MeshBuilder_Object obj;
	obj.depth_tested = depth_tested;
	obj.owner = this;
	if (use_transform)
		obj.transform = get_ws_transform();
	else
		obj.transform = glm::mat4(1);
	obj.visible = true;
#ifdef EDITOR_BUILD
	obj.visible &= !get_owner()->get_hidden_in_editor();
#endif //  EDITOR_BUILD
	obj.use_background_color = use_background_color;
	obj.background_color = background_color;
	obj.meshbuilder = &mb;
	idraw->get_scene()->update_meshbuilder(editor_mb_handle, obj);
}

#include "Render/MaterialPublic.h"
WorldTextComponent::WorldTextComponent() {
	set_call_init_in_editor(true);
}

WorldTextComponent::~WorldTextComponent() {
	assert(!editor_mb_handle.is_valid());
}

void WorldTextComponent::start() {
	font_mat = imaterials->create_dynmaic_material(MaterialInstance::load("font_particle.mm"));
	sync_render_data();
}

void WorldTextComponent::stop() {
	idraw->get_scene()->remove_particle_obj(editor_mb_handle);
}

void WorldTextComponent::on_changed_transform() {
	sync_render_data();
}
#include "Render/RenderWindow.h"
#include "UI/GUISystemPublic.h"
#include "UI/UILoader.h"
void WorldTextComponent::on_sync_render_data() {
	if (!font_mat)
		return;

	mb.Begin();
	TextShape shape;
	shape.text = text;
	shape.color = COLOR_WHITE;
	shape.font = UiSystem::inst->defaultFont;
	if (font)
		shape.font = font;
	TextShape::draw_text_to_meshbuilder(shape, mb);
	mb.End();

	if (!editor_mb_handle.is_valid())
		editor_mb_handle = idraw->get_scene()->register_particle_obj();

	font_mat->set_tex_parameter("Image", shape.font->font_texture.get());

	Particle_Object po;
	po.meshbuilder = &mb;
	po.owner = this;
	po.transform = get_ws_transform() * glm::scale(glm::mat4(1), glm::vec3(0.05));
	po.material = font_mat.get();

	idraw->get_scene()->update_particle_obj(editor_mb_handle, po);
}
