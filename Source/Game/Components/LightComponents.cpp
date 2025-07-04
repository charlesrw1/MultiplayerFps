
#include "LightComponents.h"
#include "Render/Render_Light.h"
#include "Render/Render_Sun.h"
#include "Render/DrawPublic.h"
#include "Render/Texture.h"

#include "BillboardComponent.h"
#include "ArrowComponent.h"

#include "Game/Entity.h"
#include "Assets/AssetDatabase.h"

#include "Framework/ReflectionMacros.h"
#include "Framework/ReflectionProp.h"

#include "GameEnginePublic.h"

////



void SpotLightComponent::start()
{
	if (eng->is_editor_level())
	{
		auto b = get_owner()->create_component<BillboardComponent>();
		b->set_texture(default_asset_load<Texture>("icon/_nearest/flashlight.png"));
		b->dont_serialize_or_edit = true;	// editor only item, dont serialize

		auto arrow_obj = get_owner()->create_child_entity();
		arrow_obj->dont_serialize_or_edit = true;
		arrow_obj->create_component<ArrowComponent>();
		arrow_obj->set_ls_transform(glm::vec3(0,0,0.4), {}, glm::vec3(0.25f));
		editor_arrow = arrow_obj->get_instance_id();
		editor_billboard = b->get_instance_id();
	}

	sync_render_data();
}
void SpotLightComponent::on_sync_render_data()
{
	if (!light_handle.is_valid())
		light_handle = idraw->get_scene()->register_light();

	Render_Light light;
	light.color = glm::vec3(color.r, color.g, color.b) * (intensity / 255.f);
	light.projected_texture = cookie_asset.get();
	light.conemax = cone_angle;
	light.conemin = inner_cone;
	light.radius = radius;
	light.is_spotlight = true;

	auto& transform = get_owner()->get_ws_transform();

	light.position = transform[3];
	light.normal = glm::normalize(-transform[2]);

	idraw->get_scene()->update_light(light_handle, light);
}



void SpotLightComponent::stop()
{
	idraw->get_scene()->remove_light(light_handle);
	auto e = eng->get_object(editor_billboard);
	if (e)
		((Component*)e)->destroy();
	e = eng->get_object(editor_arrow);
	if (e)
		((Component*)e)->destroy();
}



void PointLightComponent::on_sync_render_data()
{
	if (!light_handle.is_valid())
		light_handle = idraw->get_scene()->register_light();
	Render_Light light;
	light.color = glm::vec3(color.r, color.g, color.b) * (intensity / 255.f);
	light.radius = radius;
	light.is_spotlight = false;
	auto& transform = get_owner()->get_ws_transform();
	light.position = transform[3];
	idraw->get_scene()->update_light(light_handle, light);
};

void PointLightComponent::start()
{
	if (eng->is_editor_level())
	{
		auto b = get_owner()->create_component<BillboardComponent>();
		b->set_texture(default_asset_load<Texture>("icon/pointBig.png"));
		b->dont_serialize_or_edit = true;	// editor only item, dont serialize
		editor_billboard = b->get_instance_id();
	}
	sync_render_data();
}

void PointLightComponent::stop()
{
	idraw->get_scene()->remove_light(light_handle);
	auto e = eng->get_object(editor_billboard);
	if (e)
		((Component*)e)->destroy();
}

void SunLightComponent::on_sync_render_data()
{
	if (!light_handle.is_valid())
		light_handle = idraw->get_scene()->register_sun();
	Render_Sun light;
	light.color = glm::vec3(color.r, color.g, color.b) * (intensity / 255.f);
	light.fit_to_scene = fit_to_scene;
	light.log_lin_lerp_factor = log_lin_lerp_factor;
	light.z_dist_scaling = z_dist_scaling;
	light.max_shadow_dist = max_shadow_dist;
	light.epsilon = epsilon;
	light.cast_shadows = true;
	auto& transform = get_owner()->get_ws_transform();
	light.direction = glm::normalize(-transform[2]);
	idraw->get_scene()->update_sun(light_handle, light);
}



void SunLightComponent::start()
{
	if (eng->is_editor_level())
	{
		auto b = get_owner()->create_component<BillboardComponent>();
		b->set_texture(default_asset_load<Texture>("icon/_nearest/sun.png"));
		b->dont_serialize_or_edit = true;	// editor only item, dont serialize

		auto s = get_owner()->create_component<ArrowComponent>();
		s->dont_serialize_or_edit = true;
		editor_arrow = s->get_instance_id();
		editor_billboard = b->get_instance_id();
	}

	sync_render_data();
}

void SunLightComponent::stop()
{
	idraw->get_scene()->remove_sun(light_handle);

	auto e = eng->get_object(editor_billboard);
	if (e)
		((Component*)e)->destroy();
	e = eng->get_object(editor_arrow);
	if (e)
		((Component*)e)->destroy();
}


SunLightComponent::~SunLightComponent() {}
PointLightComponent::~PointLightComponent() {}
SpotLightComponent::~SpotLightComponent() {}
#include "Render/Render_Volumes.h"

void SkylightComponent::start() {

	mytexture = new Texture; // g_imgs.install_system_texture("_skylight");
	sync_render_data();

	if (eng->is_editor_level()) {
		auto b = get_owner()->create_component<BillboardComponent>();
		b->set_texture(default_asset_load<Texture>("icon/_nearest/skylight.png"));
		b->dont_serialize_or_edit = true;	// editor only item, dont serialize
	}
}
void SkylightComponent::stop() {
	idraw->get_scene()->remove_skylight(handle);
	//delete mytexture;
	mytexture = nullptr;
}
void SkylightComponent::on_sync_render_data()
{
	if (!handle.is_valid())
		handle = idraw->get_scene()->register_skylight();
	Render_Skylight sl;
	sl.generated_cube = mytexture;
	sl.wants_update = true;
	idraw->get_scene()->update_skylight(handle, sl);
}

void SkylightComponent::editor_on_change_property()  {
	if (recapture_skylight.check_and_swap()) {
		sys_print(Debug, "recapturing skylight");
		sync_render_data();
	}
}

#include "Game/Entity.h"


SpotLightComponent::SpotLightComponent() {
	set_call_init_in_editor(true);
}
PointLightComponent::PointLightComponent() {
	set_call_init_in_editor(true);
}

SunLightComponent::SunLightComponent() {
	set_call_init_in_editor(true);
}
#include "Game/Components/MeshbuilderComponent.h"
void CubemapComponent::start() {
	mytexture = new Texture;

	if (eng->is_editor_level()) {
		editor_meshbuilder = get_owner()->create_component<MeshBuilderComponent>();
		editor_meshbuilder->dont_serialize_or_edit = true;
		editor_meshbuilder->use_background_color = true;
		editor_meshbuilder->use_transform = false;
		update_editormeshbuilder();
	}

	sync_render_data();
}
void CubemapComponent::on_sync_render_data()
{
	if (!handle.is_valid())
		handle = idraw->get_scene()->register_reflection_volume();
	Render_Reflection_Volume h;
	h.wants_update = true;
	h.generated_cube = mytexture;
	h.probe_position = get_ws_transform() * glm::vec4(anchor.p, 1.0);
	glm::vec3 scale = get_owner()->get_ls_scale();
	h.boxmin = get_ws_position() - scale * 0.5f;
	h.boxmax = get_ws_position() + scale * 0.5f;
	idraw->get_scene()->update_reflection_volume(handle, h);
}

void CubemapComponent::update_editormeshbuilder()
{
	if (!editor_meshbuilder)
		return;
	glm::vec3 scale = get_owner()->get_ls_scale();
	auto boxmin = get_ws_position() - scale * 0.5f;
	auto boxmax = get_ws_position() + scale * 0.5f;
	editor_meshbuilder->mb.Begin();
	editor_meshbuilder->mb.PushLineBox(boxmin, boxmax, COLOR_GREEN);
	editor_meshbuilder->mb.End();
}
void CubemapComponent::stop() {
	idraw->get_scene()->remove_reflection_volume(handle);
	delete mytexture;
	mytexture = nullptr;
	if (editor_meshbuilder) {
		editor_meshbuilder->destroy();
		editor_meshbuilder = nullptr;
	}
}
void CubemapComponent::editor_on_change_property() {
	if (recapture.check_and_swap()) {
		sync_render_data();
	}
	update_editormeshbuilder();
}


#include "Framework/AddClassToFactory.h"
#ifdef EDITOR_BUILD
// FIXME!

#endif
class CubemapAnchorSerializer : public IPropertySerializer
{
	// Inherited via IPropertySerializer
	virtual std::string serialize(DictWriter& out, const PropertyInfo& info, const void* inst, ClassBase* user) override
	{
		const CubemapAnchor* j = (const CubemapAnchor*)info.get_ptr(inst);
		return string_format("%f %f %f %d", j->p.x, j->p.y, j->p.z,(int)j->worldspace);
	}
	virtual void unserialize(DictParser& in, const PropertyInfo& info, void* inst, StringView token, ClassBase* user,IAssetLoadingInterface*) override
	{
		CubemapAnchor* j = (CubemapAnchor*)info.get_ptr(inst);
		std::string to_str(token.str_start, token.str_len);
		int d = 0;
		int args = sscanf(to_str.c_str(), "%f %f %f %d", &j->p.x, &j->p.y, &j->p.z,&d);
		j->worldspace = bool(d);
		if (args != 4) 
			sys_print(Warning, "Anchor joint unserializer fail\n");
	}
};
ADDTOFACTORYMACRO_NAME(CubemapAnchorSerializer, IPropertySerializer, "CubemapAnchor");