
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

DecalComponent::DecalComponent() {
	set_call_init_in_editor(true);
}
void DecalComponent::start() {
	
	if (eng->is_editor_level())
	{
		auto b = get_owner()->create_component<BillboardComponent>();
		b->set_texture(g_assets.find_sync<Texture>("icon/_nearest/decal.png"));
		b->dont_serialize_or_edit = true;	// editor only item, dont serialize
		auto a = get_owner()->create_component<ArrowComponent>();
		a->dont_serialize_or_edit = true;
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
	idraw->get_scene()->update_decal(handle, rd);
}
void DecalComponent::end() {
	idraw->get_scene()->remove_decal(handle);
}
void DecalComponent::on_changed_transform() {
	sync_render_data();
}
void DecalComponent::editor_on_change_property() {
	sync_render_data();
}

DecalComponent::~DecalComponent() {}

void DecalComponent::set_material(const MaterialInstance* mat)
{
	material.ptr = (MaterialInstance*)mat;
	sync_render_data();
}
