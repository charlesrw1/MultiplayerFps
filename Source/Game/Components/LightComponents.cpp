
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

#include <filesystem>
////

glm::vec4 get_color_light_value(Color32 c, float intensity) {
	ASSERT(intensity >= 0.f);
	auto vec = color32_to_vec4(c);
	auto linear = colorvec_srgb_to_linear(vec);
	return linear * PI * intensity;
}

void SpotLightComponent::start() {
	ASSERT(get_owner() != nullptr);
	if (eng->is_editor_level()) {
		auto billboard = get_owner()->create_component<BillboardComponent>();
		billboard->set_texture(default_asset_load<Texture>("eng/icon/_nearest/flashlight.png"));
		billboard->dont_serialize_or_edit = true; // editor only item, dont serialize
		auto arrow_obj = get_owner()->create_child_entity();
		arrow_obj->dont_serialize_or_edit = true;
		auto arrow_comp = arrow_obj->create_component<ArrowComponent>();
		arrow_obj->set_ls_transform(glm::vec3(0, 0, 0.4), {}, glm::vec3(0.25f));
		editor_arrow = arrow_comp;
		editor_billboard = billboard;
	}

	sync_render_data();
}
void SpotLightComponent::on_sync_render_data() {
	ASSERT(idraw != nullptr);
	if (!light_handle.is_valid())
		light_handle = idraw->get_scene()->register_light();

	Render_Light light;
	light.color =
		get_color_light_value(color, intensity); // glm::vec3(color.r, color.g, color.b)* (intensity / 255.f)* PI;
	light.projected_texture = const_cast<Texture*>(cookie_asset);
	light.conemax = cone_angle;
	light.conemin = inner_cone;
	light.radius = radius;
	light.is_spotlight = true;
	light.casts_shadow_mode = (int8_t)shadow;
	light.casts_shadow_size = (int8_t)shadow_size;

	auto& transform = get_owner()->get_ws_transform();

	light.position = transform[3];
	light.normal = glm::normalize(-transform[2]);

	idraw->get_scene()->update_light(light_handle, light);
}

void SpotLightComponent::stop() {
	ASSERT(idraw != nullptr);
	idraw->get_scene()->remove_light(light_handle);
	if (auto b = editor_billboard.get()) {
		b->destroy();
	}
	if (auto arrow = editor_arrow.get()) {
		arrow->get_owner()->destroy_deferred();
	}
}

void PointLightComponent::on_sync_render_data() {
	ASSERT(idraw != nullptr);
	if (!light_handle.is_valid())
		light_handle = idraw->get_scene()->register_light();
	Render_Light light;
	light.color =
		get_color_light_value(color, intensity); // glm::vec3(color.r, color.g, color.b)* (intensity / 255.f)* PI;
	light.radius = radius;
	light.projected_texture = (Texture*)cookie_asset;
	light.is_spotlight = false;
	auto& transform = get_owner()->get_ws_transform();
	light.position = transform[3];
	idraw->get_scene()->update_light(light_handle, light);
};

void PointLightComponent::start() {
	ASSERT(get_owner() != nullptr);
	if (eng->is_editor_level()) {
		auto b = get_owner()->create_component<BillboardComponent>();
		b->set_texture(default_asset_load<Texture>("eng/icon/pointBig.png"));
		b->dont_serialize_or_edit = true; // editor only item, dont serialize
		editor_billboard = b->get_instance_id();
	}
	sync_render_data();
}

void PointLightComponent::stop() {
	ASSERT(idraw != nullptr);
	idraw->get_scene()->remove_light(light_handle);
	auto e = eng->get_object(editor_billboard);
	if (e)
		((Component*)e)->destroy();
}

void SunLightComponent::on_sync_render_data() {
	ASSERT(idraw != nullptr);
	if (!light_handle.is_valid())
		light_handle = idraw->get_scene()->register_sun();
	Render_Sun light;
	light.color =
		get_color_light_value(color, intensity); // glm::vec3(color.r, color.g, color.b)* (intensity / 255.f)* PI;
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

void SunLightComponent::start() {
	ASSERT(get_owner() != nullptr);
	if (eng->is_editor_level()) {
		auto b = get_owner()->create_component<BillboardComponent>();
		b->set_texture(default_asset_load<Texture>("eng/icon/_nearest/sun.png"));
		b->dont_serialize_or_edit = true; // editor only item, dont serialize

		auto s = get_owner()->create_component<ArrowComponent>();
		s->dont_serialize_or_edit = true;
		editor_arrow = s->get_instance_id();
		editor_billboard = b->get_instance_id();
	}

	sync_render_data();
}

void SunLightComponent::stop() {
	ASSERT(idraw != nullptr);
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

SpotLightComponent::SpotLightComponent() {
	set_call_init_in_editor(true);
}
PointLightComponent::PointLightComponent() {
	set_call_init_in_editor(true);
}

SunLightComponent::SunLightComponent() {
	set_call_init_in_editor(true);
}
