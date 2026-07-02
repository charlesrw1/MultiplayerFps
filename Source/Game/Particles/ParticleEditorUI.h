#pragma once
#ifdef EDITOR_BUILD

#include "Game/EntityComponent.h"
#include "Framework/CurveEditorImgui.h"
#include "Framework/GradientEditorImgui.h"
#include "ParticleTypes.h"
class PropertyGrid;

class ParticleSystemComponent;

class ParticleSystemEditorUi : public IComponentEditorUi
{
public:
	ParticleSystemEditorUi(ParticleSystemComponent* comp);
	~ParticleSystemEditorUi();
	bool draw() override;

private:
	void draw_subsystem_list();
	void draw_main_module(struct MainModule& mod);
	void draw_emission_module(struct EmissionModule& mod);
	void draw_shape_module(struct ShapeModule& mod);
	void draw_velocity_module(struct VelocityOverLifetimeModule& mod);
	void draw_force_module(struct ForceOverLifetimeModule& mod);
	void draw_limit_velocity_module(struct LimitVelocityOverLifetimeModule& mod);
	void draw_color_module(struct ColorOverLifetimeModule& mod);
	void draw_size_module(struct SizeOverLifetimeModule& mod);
	void draw_rotation_module(struct RotationOverLifetimeModule& mod);
	void draw_texture_sheet_module(struct TextureSheetModule& mod);
	void draw_noise_module(struct NoiseModule& mod);
	void draw_trail_module(struct TrailModule& mod);
	void draw_renderer_module(struct RendererModule& mod);
	void draw_playback_controls();

	bool draw_module_header(const char* name, bool& enabled, bool& expanded);
	void draw_minmax_curve(const char* label, MinMaxCurve& curve);
	void draw_gradient_field(const char* label, Gradient& gradient);

	ParticleSystemComponent* comp;
	int selected_subsystem = 0;
	CurveEditorImgui curve_editor_popup;
	GradientEditorImgui gradient_editor;
	GradientEditorImgui gradient_window_editor;

	MinMaxCurve* editing_curve = nullptr;
	bool show_curve_popup = false;

	Gradient* editing_gradient = nullptr;
	bool show_gradient_popup = false;

	bool expanded_main = true;
	bool expanded_emission = true;
	bool expanded_shape = false;
	bool expanded_velocity = false;
	bool expanded_force = false;
	bool expanded_limit_velocity = false;
	bool expanded_color = false;
	bool expanded_size = false;
	bool expanded_rotation = false;
	bool expanded_texture = false;
	bool expanded_noise = false;
	bool expanded_trail = false;
	bool expanded_renderer = true;

	bool show_create_popup = false;
	char create_name[128] = {};

	// PropertyGrid for the material slot; rebuilt whenever it would point at a
	// different RendererModule than last frame. Keyed on the module's address
	// (not the subsystem index) because the index alone can't detect an asset
	// swap or a subsystems-vector reallocation (Add/Remove Subsystem) that
	// leaves the index unchanged but relocates/repoints the underlying module —
	// a stale pointer here writes into whatever memory used to be there.
	std::unique_ptr<PropertyGrid> mat_pg;
	void* mat_pg_owner = nullptr;
};

#endif
