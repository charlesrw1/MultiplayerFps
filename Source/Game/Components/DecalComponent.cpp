
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
#include "Game/AssetPtrMacro.h"
#include "Framework/ReflectionMacros.h"

CLASS_IMPL(DecalComponent);
DecalComponent::DecalComponent() {
	set_call_init_in_editor(true);
}
void DecalComponent::start() {
	handle = idraw->get_scene()->register_decal(Render_Decal());
	update_handle();


	if (eng->is_editor_level())
	{
		auto b = get_owner()->create_component<BillboardComponent>();
		b->set_texture(g_assets.find_sync<Texture>("icon/_nearest/decal.png"));
		b->dont_serialize_or_edit = true;	// editor only item, dont serialize
		auto a = get_owner()->create_component<ArrowComponent>();
		a->dont_serialize_or_edit = true;
	}
}
void DecalComponent::end() {
	idraw->get_scene()->remove_decal(handle);
}
void DecalComponent::on_changed_transform() {
	update_handle();
}
void DecalComponent::editor_on_change_property() {
	update_handle();
}
void DecalComponent::update_handle()
{
	Render_Decal rd;
	rd.transform = get_ws_transform();
	rd.visible = true;
	rd.material = material.get();
	idraw->get_scene()->update_decal(handle, rd);
}
const PropertyInfoList* DecalComponent::get_props()
{
	START_PROPS(DecalComponent)
		REG_ASSET_PTR(material, PROP_DEFAULT)
		END_PROPS(DecalComponent)
}
DecalComponent::~DecalComponent() {}


void DecalComponent::set_material(const MaterialInstance* mat)
{
	material.ptr = (MaterialInstance*)mat;
	update_handle();
}
