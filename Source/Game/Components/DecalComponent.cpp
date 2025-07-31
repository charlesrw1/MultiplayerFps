
#include "DecalComponent.h"
#include "Render/Render_Decal.h"
#include "Render/DrawPublic.h"
#include "Render/MaterialPublic.h"
#include "GameEnginePublic.h"
#include "Render/Texture.h"
#include "Game/Entity.h"
#include "BillboardComponent.h"
#include "Assets/AssetDatabase.h"
#include "ArrowComponent.h"

#include "Framework/ReflectionMacros.h"

// comment 3

DecalComponent::DecalComponent() {
	set_call_init_in_editor(true);
}
void DecalComponent::start() {
	
	if (eng->is_editor_level())
	{
		auto b = get_owner()->create_component<BillboardComponent>();
		b->set_texture(Texture::load("eng/icon/_nearest/decal.png"));
		b->dont_serialize_or_edit=true;
		auto a = get_owner()->create_component<ArrowComponent>();
		a->dont_serialize_or_edit=true;

	}

	sync_render_data();
}
void DecalComponent::on_sync_render_data()
{
	if (!handle.is_valid()) {
		handle = idraw->get_scene()->register_decal();
	}
	Render_Decal rd;
	rd.transform = get_ws_transform();
	rd.visible = true;
#ifdef  EDITOR_BUILD
	rd.visible &= !get_owner()->get_hidden_in_editor();
#endif //  EDITOR_BUILD

	rd.material = material.get();
	rd.ordering = sort_order;
	idraw->get_scene()->update_decal(handle, rd);
}
void DecalComponent::stop() {
	idraw->get_scene()->remove_decal(handle);
}
void DecalComponent::on_changed_transform() {
	sync_render_data();
}
#ifdef  EDITOR_BUILD
void DecalComponent::editor_on_change_property() {
	sync_render_data();
}
#endif //  EDITOR_BUILD

DecalComponent::~DecalComponent() {}

void DecalComponent::set_material(MaterialInstance* mat)
{
	material.ptr = mat;
	sync_render_data();
}
#include "UI/GUISystemPublic.h"
class DecalComponentEditorUi : public IComponentEditorUi {
public:
	DecalComponentEditorUi(DecalComponent* decal) : decal(decal) {

	}
	DecalComponent* decal = nullptr;
	bool draw() final {
		auto& window = UiSystem::inst->window;
		//window.draw()

		return false;
	}
};
