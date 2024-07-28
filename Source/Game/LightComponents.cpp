#include "EntityComponentTypes.h"
#include "Render/Render_Light.h"
#include "Render/Render_Sun.h"
#include "DrawPublic.h"
#include "Texture.h"

const PropertyInfoList* SpotLightComponent::get_props() {
	START_PROPS(SpotLightComponent)
		REG_FLOAT(cone_angle, PROP_DEFAULT, "45.0"),
		REG_FLOAT(inner_cone, PROP_DEFAULT, "40.0"),
		REG_FLOAT(radius, PROP_DEFAULT, "20.0"),
		REG_BOOL(visible, PROP_DEFAULT, "1"),
		REG_ASSET_PTR(cookie_asset, PROP_DEFAULT)
	END_PROPS(SpotLightComponent)
}

void SpotLightComponent::build_render_light(Render_Light& light)
{
	light.color = color;
	light.projected_texture = cookie_asset.get();
	light.conemax = cone_angle;
	light.conemin = inner_cone;
	light.radius = radius;
	light.is_spotlight = true;

	auto& transform = get_ws_transform();

	light.position = transform[3];
	light.normal = glm::normalize(transform[0]);
}

void SpotLightComponent::on_init()
{
	Render_Light light;
	build_render_light(light);
	light_handle = idraw->get_scene()->register_light(light);
}

void SpotLightComponent::on_deinit()
{
	idraw->get_scene()->remove_light(light_handle);
}

void SpotLightComponent::on_changed_transform()
{
	assert(light_handle.is_valid());
	Render_Light light;
	build_render_light(light);
	idraw->get_scene()->update_light(light_handle, light);
}

const PropertyInfoList* PointLightComponent::get_props() {
	START_PROPS(PointLightComponent)
		REG_FLOAT(radius, PROP_DEFAULT, "5.0"),
		REG_BOOL(visible, PROP_DEFAULT, "1"),
	END_PROPS(PointLightComponent)
}


void PointLightComponent::build_render_light(Render_Light& light)
{
	light.color = color;
	light.radius = radius;
	light.is_spotlight = false;

	auto& transform = get_ws_transform();

	light.position = transform[3];
}

void PointLightComponent::on_init()
{
	Render_Light light;
	build_render_light(light);
	light_handle = idraw->get_scene()->register_light(light);
}

void PointLightComponent::on_deinit()
{
	idraw->get_scene()->remove_light(light_handle);
}

void PointLightComponent::on_changed_transform()
{
	assert(light_handle.is_valid());
	Render_Light light;
	build_render_light(light);
	idraw->get_scene()->update_light(light_handle, light);
}

const PropertyInfoList* SunLightComponent::get_props() {
	START_PROPS(SunLightComponent)
		REG_BOOL(visible, PROP_DEFAULT, "1"),
		REG_BOOL(fit_to_scene, PROP_DEFAULT, "1"),
		REG_FLOAT(log_lin_lerp_factor, PROP_DEFAULT, "0.5"),
		REG_FLOAT(max_shadow_dist, PROP_DEFAULT, "80.0"),
		REG_FLOAT(epsilon, PROP_DEFAULT, "0.008"),
		REG_FLOAT(z_dist_scaling, PROP_DEFAULT, "1.0"),
	END_PROPS(SunLightComponent)
}

void SunLightComponent::build_render_light(Render_Sun& light)
{
	light.color = color;
	light.fit_to_scene = fit_to_scene;
	light.log_lin_lerp_factor = log_lin_lerp_factor;
	light.z_dist_scaling = z_dist_scaling;
	light.max_shadow_dist = max_shadow_dist;
	light.epsilon = epsilon;
	light.cast_shadows = true;

	auto& transform = get_ws_transform();
	light.direction = glm::normalize(transform[0]);
}

void SunLightComponent::on_init()
{
	Render_Sun light;
	build_render_light(light);
	light_handle = idraw->get_scene()->register_sun(light);
}

void SunLightComponent::on_deinit()
{
	idraw->get_scene()->remove_sun(light_handle);
}

void SunLightComponent::on_changed_transform()
{
	assert(light_handle.is_valid());
	Render_Sun light;
	build_render_light(light);
	idraw->get_scene()->update_sun(light_handle, light);
}

SunLightComponent::~SunLightComponent() {}
PointLightComponent::~PointLightComponent() {}
SpotLightComponent::~SpotLightComponent() {}
SpotLightComponent::SpotLightComponent() {}