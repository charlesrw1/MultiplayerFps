#pragma once

#include "Game/EntityComponent.h"
#include "Game/SerializePtrHelpers.h"
#include "Framework/Reflection2.h"
#include "Framework/StructReflection.h"
#include "Framework/BoolButton.h"

GENERATED_CLASS_INCLUDE("Render/Texture.h");

class Texture;
struct Render_Light;
class BillboardComponent;
class ArrowComponent;
class SpotLightComponent : public Component
{
public:
	CLASS_BODY(SpotLightComponent);
	SpotLightComponent();
	~SpotLightComponent() override;
	void start() final;
	void end() final;
	void on_sync_render_data() final;
	void on_changed_transform() final {
		sync_render_data();
	}
	void editor_on_change_property() final {
		sync_render_data();
	}
#ifdef EDITOR_BUILD
	const char* get_editor_outliner_icon() const final {
		return "eng/editor/light.png";
	}
#endif
	REF Color32 color = COLOR_WHITE;
	REF float intensity = 1.f;
	REF float radius = 20.f;
	REF float cone_angle = 45.0;
	REF float inner_cone = 40.0;
	REF AssetPtr<Texture> cookie_asset;
	REF bool visible = true;
	handle<Render_Light> light_handle;
	uint64_t editor_billboard = 0;
	uint64_t editor_arrow = 0;
};

class PointLightComponent : public Component
{
public:
	CLASS_BODY(PointLightComponent);
	PointLightComponent();
	void start() final;
	void end() final;
	void on_sync_render_data() final;
	void on_changed_transform() final {
		sync_render_data();
	}
	void editor_on_change_property() final {
		sync_render_data();
	}
	~PointLightComponent() final;
#ifdef EDITOR_BUILD
	const char* get_editor_outliner_icon() const final {
		return "eng/editor/light.png";
	}
#endif

	REF Color32 color = COLOR_WHITE;
	REF float intensity = 1.f;
	REF float radius = 5.f;
	REF bool visible = true;
	handle<Render_Light> light_handle;
	uint64_t editor_billboard = 0;
};

struct Render_Sun;
class SunLightComponent : public Component
{
public:
	CLASS_BODY(SunLightComponent);

	SunLightComponent();
	~SunLightComponent() final;
	void start() final;
	void end() final;
	void on_changed_transform() final {
		sync_render_data();
	}
	void editor_on_change_property() final {
		sync_render_data();
	}
	void on_sync_render_data() final;
#ifdef EDITOR_BUILD
	const char* get_editor_outliner_icon() const final {
		return "eng/editor/light.png";
	}
#endif
	REF Color32 color = COLOR_WHITE;
	REF float intensity = 2.f;
	REF bool fit_to_scene = true;
	REF float log_lin_lerp_factor = 0.5;
	REF float max_shadow_dist = 80.f;
	REF float epsilon = 0.008f;
	REF float z_dist_scaling = 1.f;
	REF bool visible = true;
	handle<Render_Sun> light_handle;
	uint64_t editor_billboard = 0;
	uint64_t editor_arrow = 0;
};


struct Render_Skylight;
class SkylightComponent : public Component
{
public:
	CLASS_BODY(SkylightComponent);
	SkylightComponent() {
		set_call_init_in_editor(true);
	}
	void start() final;
	void end() final;
	void on_sync_render_data() final;
	void editor_on_change_property() final;
	REFLECT(transient)
	BoolButton recapture_skylight;	// Recapture
	Texture* mytexture = nullptr;
	handle<Render_Skylight> handle;
};

struct CubemapAnchor {
	STRUCT_BODY();
	REF bool worldspace = false;
	REF glm::vec3 p = glm::vec3(0.f);
};
struct Render_Reflection_Volume;
class MeshComponent;
class MeshBuilderComponent;
class CubemapComponent : public Component
{
public:
	CLASS_BODY(CubemapComponent);
	CubemapComponent() {
		set_call_init_in_editor(true);
	}
	void start() final;
	void end() final;
	void on_changed_transform() final {
		update_editormeshbuilder();
	}
	void editor_on_change_property() final;
	void on_sync_render_data() final;

	REFLECT(transient)
	BoolButton recapture;	// Recapture
	REF CubemapAnchor anchor;
private:
	void update_editormeshbuilder();
	handle<Render_Reflection_Volume> handle;
	Texture* mytexture = nullptr;
	MeshBuilderComponent* editor_meshbuilder = nullptr;
};