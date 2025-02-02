#pragma once

#include "Game/EntityComponent.h"
#include "Game/SerializePtrHelpers.h"
#include "Framework/Reflection2.h"

GENERATED_CLASS_INCLUDE("Render/Texture.h");

class Texture;
struct Render_Light;
class BillboardComponent;
class ArrowComponent;
NEWCLASS(SpotLightComponent, EntityComponent)
public:
	void start() override;
	void end() override;
	void on_changed_transform() override;
	~SpotLightComponent() override;
	SpotLightComponent();


	REFLECT();
	Color32 color = COLOR_WHITE;
	REFLECT();
	float intensity = 1.f;
	REFLECT();
	float radius = 20.f;
	REFLECT();
	float cone_angle = 45.0;
	REFLECT();
	float inner_cone = 40.0;
	REFLECT();
	AssetPtr<Texture> cookie_asset;
	REFLECT();
	bool visible = true;

	void build_render_light(Render_Light& light);

	void editor_on_change_property() override;


	handle<Render_Light> light_handle;
	uint64_t editor_billboard = 0;
	uint64_t editor_arrow = 0;
};

NEWCLASS(PointLightComponent, EntityComponent)
public:
	PointLightComponent();
	void start() override;
	void end() override;
	void on_changed_transform() override;
	~PointLightComponent() override;

	void editor_on_change_property() override;

	void build_render_light(Render_Light& light);

	REFLECT();
	Color32 color = COLOR_WHITE;
	REFLECT();
	float intensity = 1.f;
	REFLECT();
	float radius = 5.f;
	REFLECT();
	bool visible = true;

	handle<Render_Light> light_handle;
	uint64_t editor_billboard = 0;
};

struct Render_Sun;
NEWCLASS(SunLightComponent, EntityComponent)
public:
	void start() override;
	void end() override;
	void on_changed_transform() override;
	void build_render_light(Render_Sun& light);
	~SunLightComponent() override;
	SunLightComponent();

	void editor_on_change_property() override;

	REFLECT();
	Color32 color = COLOR_WHITE;
	REFLECT();
	float intensity = 2.f;
	REFLECT();
	bool fit_to_scene = true;
	REFLECT();
	float log_lin_lerp_factor = 0.5;
	REFLECT();
	float max_shadow_dist = 80.f;
	REFLECT();
	float epsilon = 0.008f;
	REFLECT();
	float z_dist_scaling = 1.f;
	REFLECT();
	bool visible = true;

	handle<Render_Sun> light_handle;
	uint64_t editor_billboard = 0;
	uint64_t editor_arrow = 0;
};

struct Render_Skylight;
NEWCLASS(SkylightComponent, EntityComponent)
public:
	SkylightComponent() {
		set_call_init_in_editor(true);
	}

	void start() override;
	void end() override;
	void on_changed_transform() override {
	}

	void editor_on_change_property() override;

	REFLECT(type="BoolButton",hint="Recapture",transient)
	bool recapture_skylight = false;

	Texture* mytexture = nullptr;
	handle<Render_Skylight> handle;
};