#pragma once
// Single per-level bake-parameter holder. Baker asserts exactly one exists in the level.
// All fields are REFLECT'd so they show in the standard property panel — no custom UI.
//
// @docs [[navigation#settings-component]]

#include "Game/EntityComponent.h"

class NavMeshSettingsComponent : public Component
{
public:
	CLASS_BODY(NavMeshSettingsComponent);
	NavMeshSettingsComponent() {
		set_call_init_in_editor(true);
#ifdef EDITOR_BUILD
		editor_is_editor_only = true;
#endif
	}

	// Recast voxelization
	REF float cell_size            = 0.3f;
	REF float cell_height          = 0.2f;

	// Agent profile
	REF float agent_radius         = 0.5f;
	REF float agent_height         = 2.0f;
	REF float agent_max_climb      = 0.4f;
	REF float agent_max_slope_deg  = 45.f;

	// Region + contour
	REF float region_min_size      = 8.f;
	REF float region_merge_size    = 20.f;
	REF float edge_max_len         = 12.f;
	REF float edge_max_error       = 1.3f;
	REF int   verts_per_poly       = 6;

	// Detail mesh
	REF float detail_sample_dist        = 6.f;
	REF float detail_sample_max_error   = 1.f;

	// Workflow
	REF bool  auto_save_on_bake    = true;

#ifdef EDITOR_BUILD
	const char* get_editor_outliner_icon() const final { return "eng/editor/settings_icon.png"; }
#endif
};
