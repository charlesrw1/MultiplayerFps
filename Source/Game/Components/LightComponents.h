#pragma once

#include "Game/EntityComponent.h"

class Texture;
struct Render_Light;
class BillboardComponent;
class ArrowComponent;
CLASS_H(SpotLightComponent, EntityComponent)
public:
	void start() override;
	void end() override;
	void on_changed_transform() override;
	~SpotLightComponent() override;
	SpotLightComponent();


	static const PropertyInfoList* get_props();



	Color32 color = COLOR_WHITE;
	float intensity = 1.f;
	float radius = 20.f;
	float cone_angle = 45.0;
	float inner_cone = 40.0;

	void build_render_light(Render_Light& light);

	void editor_on_change_property() override;

	AssetPtr<Texture> cookie_asset;
	bool visible = true;
	handle<Render_Light> light_handle;

	uint64_t editor_billboard = 0;
	uint64_t editor_arrow = 0;
};

CLASS_H(PointLightComponent, EntityComponent)
public:
	PointLightComponent();
	void start() override;
	void end() override;
	void on_changed_transform() override;
	~PointLightComponent() override;

	void editor_on_change_property() override;

	void build_render_light(Render_Light& light);

	static const PropertyInfoList* get_props();

	Color32 color = COLOR_WHITE;
	float intensity = 1.f;
	float radius = 5.f;

	bool visible = true;
	handle<Render_Light> light_handle;
	uint64_t editor_billboard = 0;
};

struct Render_Sun;
CLASS_H(SunLightComponent, EntityComponent)
public:
	void start() override;
	void end() override;
	void on_changed_transform() override;
	void build_render_light(Render_Sun& light);
	~SunLightComponent() override;
	SunLightComponent();

	void editor_on_change_property() override;

	static const PropertyInfoList* get_props();

	Color32 color = COLOR_WHITE;
	float intensity = 2.f;

	bool fit_to_scene = true;
	float log_lin_lerp_factor = 0.5;
	float max_shadow_dist = 80.f;
	float epsilon = 0.008f;
	float z_dist_scaling = 1.f;

	bool visible = true;
	handle<Render_Sun> light_handle;
	uint64_t editor_billboard = 0;
	uint64_t editor_arrow = 0;
};
