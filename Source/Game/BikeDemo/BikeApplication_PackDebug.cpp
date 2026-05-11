// BikeApplication_PackDebug.cpp
// Debug menu for pack/boid visualisation: tuning sliders for avoidance,
// yield, AI cornering, and boundary avoidance parameters.

#include "BikeHeaders.h"

#include "Render/Texture.h"
#include "Render/Model.h"
#include "Game/GameplayStatic.h"
#include "Game/Components/MeshComponent.h"
#include "Game/Components/DecalComponent.h"
#include "Game/Components/PhysicsComponents.h"
#include "Game/Entities/CharacterController.h"
#include "Input/InputSystem.h"
#include "GameEnginePublic.h"
#include "Framework/MathLib.h"
#include "Framework/Config.h"
#include "imgui.h"
#include <SDL2/SDL_gamecontroller.h>
#include <glm/gtc/matrix_transform.hpp>
#include "Render/DrawPublic.h"
#include "Render/RenderObj.h"
#include "UI/Gui.h"
#include "Debug.h"
#include <algorithm>

// ============================================================
// Externs from BikeApplication.cpp — shared debug / follow state
// ============================================================
extern BikeGameApplication* g_bike_app;
extern bool  g_follow_rider;
extern int   g_follow_idx;
extern float g_follow_dist;
extern float g_follow_height;
extern float g_follow_pitch;

// Snapshot helpers — defined in BikeApplication.cpp
extern void snapshot_record();
extern void snapshot_restore();
extern bool snapshot_has_data();  // true when s_rider_snapshots is non-empty
extern int  snapshot_count();     // number of saved riders

// Pack-static vars from BikeApplication_Pack.cpp (avoidance layer)
extern float SIDE_BY_SIDE_LONG_M;
extern float SIDE_BY_SIDE_LAT_M;
extern float SIDE_BY_SIDE_POWER_W;
extern float AVOID_LONG_MAX;
extern float AVOID_BIKE_HALF_W;
extern float AVOID_CLEARANCE;
extern float AVOID_PREDICT_T1;
extern float AVOID_PREDICT_T2;
extern float AVOID_STEER_KP;
extern float YIELD_LONG_RADIUS;
extern float YIELD_OUTER_LAT;
extern float YIELD_INNER_LAT;
extern float YIELD_SQUEEZE_M;
extern float YIELD_BRAKE_K;

// ============================================================
// Debug menu: Boid visualisation (placed here so pack-static vars are in scope)
// ============================================================

static void bike_boid_debug()
{
	ASSERT(true);  // always valid — early-out guards below
	if (!g_bike_app) return;
	const auto& all    = g_bike_app->all_riders;
	const auto& sorted = g_bike_app->riders_sorted;
	if (all.empty()) return;

	// ---- Snapshot ----
	ImGui::SeparatorText("Segment Replay");
	if (ImGui::Button("Record positions"))
		snapshot_record();
	ImGui::SameLine();
	const bool has_snap = snapshot_has_data();
	if (!has_snap) ImGui::BeginDisabled();
	if (ImGui::Button("Teleport to recorded"))
		snapshot_restore();
	if (!has_snap) ImGui::EndDisabled();
	if (has_snap)
		ImGui::SameLine(), ImGui::TextDisabled("(%d riders saved)", snapshot_count());

	// ---- Rider picker ----
	static int  selected_idx  = 0;          // index into all_riders
	static bool draw_boids    = true;

	ImGui::Checkbox("Draw boid debug", &draw_boids);

	// Build label list: "Player" for BikePlayer, "AI #N" for BikeAI
	ImGui::Text("Select rider:");
	ImGui::SameLine();
	ImGui::InputInt("##label", &selected_idx,1);
	selected_idx = glm::clamp(selected_idx, 0, (int)all.size() - 1);
	BikeObject* bo = all[selected_idx];

	// ---- Camera follow ----
	g_follow_idx = selected_idx;
	ImGui::Checkbox("Follow selected rider (camera)", &g_follow_rider);
	if (g_follow_rider) {
		ImGui::SetNextItemWidth(70.f); ImGui::DragFloat("dist",   &g_follow_dist,   0.05f, 0.5f, 10.f);
		ImGui::SetNextItemWidth(70.f); ImGui::DragFloat("height", &g_follow_height, 0.05f, 0.5f,  5.f);
		ImGui::SetNextItemWidth(70.f); ImGui::DragFloat("pitch°", &g_follow_pitch,  0.5f,  0.f, 80.f);
	}

	// ---- Text readout ----
	ImGui::Separator();
	ImGui::Text("long_sep_power:    %+.1f W", bo->boid_long_sep_power);

	// ---- Tuning sliders ----
	ImGui::SeparatorText("Side-by-side power yield");
	ImGui::DragFloat("long m",     &SIDE_BY_SIDE_LONG_M,  0.05f, 0.5f, 10.f);
	ImGui::DragFloat("lat m",      &SIDE_BY_SIDE_LAT_M,   0.05f, 0.1f,  3.f);
	ImGui::DragFloat("power max W",&SIDE_BY_SIDE_POWER_W, 5.f,   0.f, 200.f);

	ImGui::SeparatorText("AI Cornering");
	ImGui::DragFloat("lookahead_dist_base",   &g_ai_params.lookahead_dist_base,   0.1f,  0.1f, 20.f);
	ImGui::SameLine(); ImGui::TextDisabled("base lookahead m (+ speed*per_ms)");
	ImGui::DragFloat("lookahead_dist_per_ms", &g_ai_params.lookahead_dist_per_ms, 0.05f, 0.f,  3.f);
	ImGui::DragFloat("steer_k",               &g_ai_params.steer_k,               0.1f,  0.1f, 10.f);
	ImGui::SameLine(); ImGui::TextDisabled("lateral error gain");
	ImGui::DragFloat("corner_look_m",         &g_ai_params.corner_look_m,         1.f,   5.f, 120.f);
	ImGui::DragFloat("corner_speed_k",        &g_ai_params.corner_speed_k,        0.05f, 0.1f,  5.f);
	ImGui::SameLine(); ImGui::TextDisabled("v_max=sqrt(k*g*R)");
	ImGui::DragFloat("anticipation dist scale", &g_ai_params.anticipation_dist_scale, 0.1f, 1.f, 5.f);
	ImGui::SameLine(); ImGui::TextDisabled("far lookahead = near * this");
	ImGui::DragFloat("anticipation k",          &g_ai_params.anticipation_k,          0.05f, 0.f, 1.f);
	ImGui::SameLine(); ImGui::TextDisabled("0=disabled 1=all-far");

	ImGui::SeparatorText("AI Boundary Avoidance");
	ImGui::DragFloat("edge predict t", &g_ai_params.edge_predict_t, 0.05f, 0.1f, 3.f);
	ImGui::SameLine(); ImGui::TextDisabled("seconds ahead to project arc");
	ImGui::DragFloat("edge safety m",  &g_ai_params.edge_safety_m,  0.05f, 0.f,  2.f);
	ImGui::SameLine(); ImGui::TextDisabled("margin inside road edge");
	ImGui::DragFloat("edge steer k",   &g_ai_params.edge_steer_k,   0.05f, 0.f,  5.f);
	ImGui::SameLine(); ImGui::TextDisabled("P gain: steer per metre beyond safe zone");
	ImGui::DragFloat("edge vel damp",  &g_ai_params.edge_vel_damp,  0.05f, 0.f,  3.f);
	ImGui::SameLine(); ImGui::TextDisabled("D gain: damp correction when already returning");
	ImGui::DragFloat("edge off brake k",   &g_ai_params.edge_off_brake_k,   0.05f, 0.f, 3.f);
	ImGui::SameLine(); ImGui::TextDisabled("brake fraction per metre past road edge");
	ImGui::DragFloat("edge off brake max", &g_ai_params.edge_off_brake_max, 0.05f, 0.f, 1.f);
	ImGui::SameLine(); ImGui::TextDisabled("max brake fraction during off-track recovery");

	ImGui::SeparatorText("Soft Lateral Separation");
	ImGui::DragFloat("avoid long max m",  &AVOID_LONG_MAX,    0.1f,  0.5f, 10.f);
	ImGui::DragFloat("avoid half-w m",    &AVOID_BIKE_HALF_W, 0.01f, 0.1f,  1.f);
	ImGui::DragFloat("avoid clearance m", &AVOID_CLEARANCE,   0.01f, 0.f,   1.f);
	ImGui::DragFloat("avoid predict t1",  &AVOID_PREDICT_T1,  0.05f, 0.f,   3.f);
	ImGui::DragFloat("avoid predict t2",  &AVOID_PREDICT_T2,  0.05f, 0.f,   3.f);
	ImGui::DragFloat("avoid steer kp",    &AVOID_STEER_KP,    0.05f, 0.f,   5.f);

	ImGui::SeparatorText("Priority Yield (Hard Clamp)");
	ImGui::DragFloat("yield long radius m", &YIELD_LONG_RADIUS, 0.1f, 0.5f, 10.f);
	ImGui::DragFloat("yield outer lat m",   &YIELD_OUTER_LAT,   0.05f, 0.1f,  3.f);
	ImGui::DragFloat("yield inner lat m",   &YIELD_INNER_LAT,   0.01f, 0.f,   0.5f);
	ImGui::DragFloat("yield squeeze m",     &YIELD_SQUEEZE_M,   0.05f, 0.f,   2.f);
	ImGui::DragFloat("yield brake k",       &YIELD_BRAKE_K,     0.05f, 0.f,   1.f);

	if (!draw_boids) return;

	// ============================================================
	// World-space drawing
	// ============================================================
	const glm::vec3 my_pos   = bo->get_ws_position();
	const glm::vec3 up        = glm::vec3(0, 0.05f, 0);  // slight y offset so lines clear the ground
	const glm::vec3 right_ws  = glm::normalize(glm::cross(bo->bike_direction, glm::vec3(0, 1, 0)));

	// Highlight selected rider with a white sphere
	Debug::add_sphere(my_pos + glm::vec3(0, 1.2f, 0), 0.25f, COLOR_WHITE, -1.f);

	// ---- Draft factor bar — horizontal line above rider, length = (1-draft_factor)*4 ----
	{
		const float benefit_len = (1.f - bo->draft_factor) * 4.f;  // 0 = no draft, up to ~1.4m at full draft
		if (benefit_len > 0.01f) {
			const glm::vec3 bar_start = my_pos + glm::vec3(0, 2.f, 0) - right_ws * benefit_len * 0.5f;
			const glm::vec3 bar_end   = bar_start + right_ws * benefit_len;
			Debug::add_line(bar_start, bar_end, Color32(0xff, 0xff, 0x00, 0xff), -1.f);  // yellow = draft
		}
	}
}
ADD_TO_DEBUG_MENU(bike_boid_debug);
