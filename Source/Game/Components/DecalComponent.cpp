
#include "DecalComponent.h"
#include "Render/Render_Decal.h"
#include "Render/DrawPublic.h"
#include "Render/MaterialPublic.h"

CLASS_IMPL(DecalComponent);

void DecalComponent::on_init() {
	handle = idraw->get_scene()->register_decal(Render_Decal());
	update_handle();
}
void DecalComponent::on_deinit() {
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
DecalComponent::DecalComponent() {}

void DecalComponent::set_material(const MaterialInstance* mat)
{
	material.ptr = (MaterialInstance*)mat;
	update_handle();
}
