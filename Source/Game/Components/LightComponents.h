#pragma once

#include "Game/EntityComponent.h"
#include "Game/SerializePtrHelpers.h"
#include "Framework/Reflection2.h"
#include "Framework/StructReflection.h"
#include "Framework/BoolButton.h"
#include "Game/EntityPtr.h"
#include "Framework/EnumDefReflection.h"

GENERATED_CLASS_INCLUDE("Render/Texture.h");

glm::vec4 get_color_light_value(Color32 c, float intensity);

NEWENUM(ShadowMode, int8_t)
{
	Disabled,
	Realtime,	// shadows update when nessecary
	Static,	// shadows update only once
};

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
	void stop() final;
	void on_sync_render_data() final;
	void on_changed_transform() final {
		sync_render_data();
	}
#ifdef EDITOR_BUILD
	void editor_on_change_property() final {
		sync_render_data();
		if (shadow_size < 0)shadow_size = 0;
		else if (shadow_size > 2)shadow_size = 2;
	}
	const char* get_editor_outliner_icon() const final {
		return "eng/editor/light.png";
	}
#endif

	REF Color32 color = COLOR_WHITE;
	REF float intensity = 1.f;
	REF float radius = 20.f;
	REF float cone_angle = 45.0;
	REF float inner_cone = 40.0;
	// only works with shadows on. projects a square, mask texture to use to attenuate the light. this disables "inner_cone" attenuation
	REF const Texture* cookie_asset = nullptr;
	REF bool visible = true;
	REF ShadowMode shadow = ShadowMode::Disabled;
	REF int8_t shadow_size = 0;	// 0=small,1=medium,2=big

	handle<Render_Light> light_handle;
	obj<BillboardComponent> editor_billboard;
	obj<ArrowComponent> editor_arrow;
};

class PointLightComponent : public Component
{
public:
	CLASS_BODY(PointLightComponent);
	PointLightComponent();
	void start() final;
	void stop() final;
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
	~SunLightComponent();
	void start() final;
	void stop() final;
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
	void stop() final;
	void on_sync_render_data() final;
	void editor_on_change_property() final;
	REFLECT(transient)
	BoolButton recapture_skylight;	// Recapture
	
	Texture* mytexture = nullptr;
	
	REFLECT();
	bool enableBakedAmbient = false;
	glm::vec3 skyTop = glm::vec3(0.f);
	glm::vec3 skyBottom = glm::vec3(0.f);

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
	void stop() final;
	void on_changed_transform() final;
	void editor_on_change_property() final;
	void on_sync_render_data() final;

	REFLECT(transient)
	BoolButton recapture;	// Recapture
	REF CubemapAnchor anchor;

	void set_baked_probe_ofs(int ofs) {
		this->probe_ofs = ofs;
		sync_render_data();
	}
	glm::vec3 get_probe_pos()  {
		return get_ws_transform() * glm::vec4(anchor.p, 1.0);
	}
private:
	REFLECT(hide);
	int probe_ofs = -1;

	void update_editormeshbuilder();
	handle<Render_Reflection_Volume> handle;
	Texture* mytexture = nullptr;
	MeshBuilderComponent* editor_meshbuilder = nullptr;
	obj<Entity> editor_mesh;
};
struct Lightmap_Object;
class LightmapComponent : public Component {
public:
	CLASS_BODY(LightmapComponent);
	LightmapComponent() {
		set_call_init_in_editor(true);
	}
	~LightmapComponent();
	void start() final;
	void stop() final;
	void editor_on_change_property() final;
	void on_sync_render_data() final;
	void do_export();
	void do_import();

	void serialize(Serializer& s) final;
private:
	REFLECT(hide);
	Texture* lightmapTexture=nullptr;
	// Export For Baking
	REFLECT(transient)
	BoolButton bakeLightmaps;
	// Import From Baking
	REFLECT(transient)
	BoolButton importBaked;
	handle<Lightmap_Object> handle;

	// this is just used for import/exporting, not saved or used at runtime
	std::unordered_map<int, int> lmProbeToObj;

	// serialized with map in serialize()
	std::vector<glm::vec3> bakedProbes;
};