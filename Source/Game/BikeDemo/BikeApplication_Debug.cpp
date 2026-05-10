// BikeApplication_Debug.cpp
// Course debug menu, crack triggers + debug, AI follow camera,
// rider snapshots, and bike_course_debug ImGui panel.

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
#include <algorithm>
#include <fstream>
#include <random>

// Shared debug pointers — defined in BikeApplication.cpp
extern BikeGameApplication* g_bike_app;

// ============================================================
// Rider snapshot — record all riders and teleport back
// (used by the boid debug menu in BikeApplication_Pack.cpp)
// ============================================================

struct BikeRiderSnapshot {
	glm::vec3 position;
	glm::vec3 bike_direction;
	float speed;
	float course_dist_m;
	float lateral_pos;
	int   course_segment;
	float actual_power_command;  // AI only; ignored for player
};

static std::vector<BikeRiderSnapshot> s_rider_snapshots;

void snapshot_record()
{
	if (!g_bike_app) return;
	s_rider_snapshots.clear();
	for (BikeObject* bo : g_bike_app->all_riders) {
		BikeRiderSnapshot snap;
		snap.position             = bo->get_ws_position();
		snap.bike_direction       = bo->bike_direction;
		snap.speed                = bo->speed;
		snap.course_dist_m        = bo->course_dist_m;
		snap.lateral_pos          = bo->lateral_pos;
		snap.course_segment       = bo->course_segment;
		snap.actual_power_command = 0.f;
		if (auto* ai = dynamic_cast<BikeAI*>(bo->input.get()))
			snap.actual_power_command = ai->actual_power_command;
		s_rider_snapshots.push_back(snap);
	}
}

void snapshot_restore()
{
	if (!g_bike_app || s_rider_snapshots.empty()) return;
	const int n = (int)glm::min(g_bike_app->all_riders.size(), s_rider_snapshots.size());
	for (int i = 0; i < n; ++i) {
		BikeObject* bo           = g_bike_app->all_riders[i];
		const BikeRiderSnapshot& snap = s_rider_snapshots[i];
		bo->get_owner()->set_ws_position(snap.position);
		bo->bike_direction  = snap.bike_direction;
		bo->speed           = snap.speed;
		bo->course_dist_m   = snap.course_dist_m;
		bo->lateral_pos     = snap.lateral_pos;
		bo->course_segment  = snap.course_segment;
		// Reset transient physics state so the bike doesn't carry over a crash or spin
		bo->current_steer   = 0.f;
		bo->steer_committed = 0.f;
		bo->is_crashed      = false;
		bo->crash_timer     = 0.f;
		if (auto* ai = dynamic_cast<BikeAI*>(bo->input.get()))
			ai->actual_power_command = snap.actual_power_command;
	}
}

bool snapshot_has_data()  { return !s_rider_snapshots.empty(); }
int  snapshot_count()     { return (int)s_rider_snapshots.size(); }

// ============================================================
// Debug camera follow state
// ============================================================
bool  g_follow_rider  = false;
int   g_follow_idx    = 0;
float g_follow_dist   = 3.4f;
float g_follow_height = 1.55f;
float g_follow_pitch  = -20.f;

// Called by BikeGameApplication::update() after all rider updates
void apply_debug_follow_camera()
{
	if (!g_follow_rider || !g_bike_app) return;
	const auto& all = g_bike_app->all_riders;
	if (all.empty()) return;
	const int idx = glm::clamp(g_follow_idx, 0, (int)all.size() - 1);
	BikeObject* bo = all[idx];

	CameraComponent* cc = CameraComponent::get_scene_camera();
	if (!cc) return;

	const glm::vec3 fwd      = glm::normalize(bo->bike_direction);
	const glm::vec3 world_up = glm::vec3(0, 1, 0);
	const glm::vec3 pivot    = bo->get_ws_position() + world_up * g_follow_height;
	const glm::vec3 right    = glm::normalize(glm::cross(fwd, world_up));

	const glm::quat pitch_rot = glm::angleAxis(glm::radians(g_follow_pitch), right);
	const glm::vec3 orbit_dir = glm::normalize(pitch_rot * (-fwd));
	const glm::vec3 cam_pos   = pivot + orbit_dir * g_follow_dist;

	const glm::vec3 look_at   = pivot + fwd * 3.f;
	const glm::vec3 cam_fwd   = glm::normalize(look_at - cam_pos);
	const glm::vec3 cam_right = glm::normalize(glm::cross(cam_fwd, world_up));
	const glm::vec3 cam_up    = glm::normalize(glm::cross(cam_right, cam_fwd));

	cc->get_owner()->set_ws_transform(glm::mat4(
		glm::vec4(cam_right, 0.f),
		glm::vec4(cam_up,    0.f),
		glm::vec4(-cam_fwd,  0.f),
		glm::vec4(cam_pos,   1.f)));

	BikePlayer* bp = dynamic_cast<BikePlayer*>(bo->input.get());
	BikeAI*     ai = dynamic_cast<BikeAI*>(bo->input.get());
	if (bp) {
		GameplayStatic::debug_text(string_format("[Player] steer_final:   %.2f", bp->dbg_steer_final));
	} else if (ai) {
		// --- Mode and path-following breakdown ---
		const char* ai_mode = ai->wheel ? paceline_state_name(ai->paceline_state) : "LEAD";
		GameplayStatic::debug_text(string_format("[AI:%s] spd=%.1fm/s  look=%.1fm  near_r=%.1fm  cf=%.2f",
			ai_mode, bo->speed, ai->dbg_lookahead_dist, ai->dbg_min_r, ai->dbg_corner_factor));
		GameplayStatic::debug_text(string_format("[AI] lat_off=%+.2f→%+.2f  bias=%+.2f  pace=%s t=%.1fs",
			ai->lat_offset, ai->dbg_lat_offset_target, ai->lat_offset_bias,
			paceline_state_name(ai->paceline_state), ai->paceline_timer_s));
		// Upcoming corner: distance, radius, safe speed, brake demand
		if (ai->dbg_brake_dist_m > 0.f)
			GameplayStatic::debug_text(string_format("[AI] corner in %.0fm  r=%.1fm  v_max=%.1fm/s  brake=%.2f",
				ai->dbg_brake_dist_m, ai->dbg_brake_corner_r, ai->dbg_v_max, ai->dbg_brake_amount));
		else
			GameplayStatic::debug_text(string_format("[AI] corner: clear for %.0fm",
				(float)(ai->BRAKE_SCAN_STEPS * ai->BRAKE_SCAN_STEP_M)));
		GameplayStatic::debug_text(string_format("[AI] steer: near=%+.2f  far=%+.2f  edge=%+.2f  avoid=%+.2f  final=%+.2f",
			ai->dbg_steer_near, ai->dbg_steer_far, ai->dbg_edge_steer, ai->dbg_avoid_steer, ai->dbg_steer_final));
		if (ai->dbg_avoid_brake > 0.f || ai->hard_steer_min > -1.f || ai->hard_steer_max < 1.f)
			GameplayStatic::debug_text(string_format("[AI] YIELD: brake=%.2f  steer_min=%+.1f  steer_max=%+.1f",
				ai->dbg_avoid_brake, ai->hard_steer_min, ai->hard_steer_max));
		GameplayStatic::debug_text(string_format("[AI] power base:%.0fW  sep:%+.0fW  cmd=%.0fW  actual=%.0fW",
			ai->dbg_power_base, bo->boid_long_sep_power, ai->dbg_power_final, bo->stamina.actual_power));

		// --- Visual overlays for selected AI ---
		// Lookahead point + line
		Debug::add_sphere(ai->dbg_lookahead_pt, 0.35f, Color32(0x00, 0xff, 0xff, 0xff), -1.f);
		Debug::add_line(bo->get_ws_position(), ai->dbg_lookahead_pt, Color32(0x00, 0xff, 0xff, 0xaa), -1.f);

		// Racing line reference point at the bike's current course position (lateral error line)
		if (g_bike_app && g_bike_app->course.is_built) {
			const BikeWaypoint cur_wp = g_bike_app->course.sample(bo->course_dist_m);
			const glm::vec3 rl_ref = cur_wp.racing_line_pos;
			// White dot on the racing line closest to the bike, red line showing lateral error
			Debug::add_sphere(rl_ref, 0.25f, Color32(0xff, 0xff, 0xff, 0xff), -1.f);
			const glm::vec3 bike_pos = bo->get_ws_position();
			const glm::vec3 bike_on_road = cur_wp.position + cur_wp.right * bo->lateral_pos;
			Debug::add_line(bike_on_road, rl_ref, Color32(0xff, 0x33, 0x33, 0xff), -1.f);
		}
	}
}

// ============================================================
// Debug menu: Course / Race State
// ============================================================

static void bike_course_debug()
{
	if (!g_bike_app) return;
	const BikeCourse& c = g_bike_app->course;

	if (!c.is_built) {
		ImGui::TextColored({1,0.4f,0.4f,1}, "Course not built — no bike_waypoint spawners in level?");
		return;
	}

	ImGui::Text("Waypoints: %d   Length: %.0f m", (int)c.waypoints.size(), c.total_length_m);

	ImGui::SeparatorText("AI Count");
	ImGui::SliderInt("num_ai", &g_bike_app->num_ai, 0, 20);
	if (ImGui::Button("Respawn AI"))
		g_bike_app->respawn_ai();

	ImGui::SeparatorText("Wheel picker");
	{
		BikeAIParams& p = g_ai_params;
		ImGui::DragFloat("wheel_long_min",     &p.wheel_long_min,     0.05f, 0.f,   5.f, "%.2f");
		ImGui::DragFloat("wheel_long_max",     &p.wheel_long_max,     0.1f,  1.f,  20.f, "%.1f");
		ImGui::DragFloat("wheel_lat_max",      &p.wheel_lat_max,      0.05f, 0.5f,  6.f, "%.2f");
		ImGui::DragFloat("wheel_long_gap",     &p.wheel_long_gap,     0.05f, 0.5f, 10.f, "%.2f");
		ImGui::DragFloat("wheel_w_long",       &p.wheel_w_long,       0.05f, 0.f,   5.f, "%.2f");
		ImGui::DragFloat("wheel_w_lat",        &p.wheel_w_lat,        0.05f, 0.f,   5.f, "%.2f");
		ImGui::DragFloat("wheel_w_draft",      &p.wheel_w_draft,      0.05f, 0.f,   5.f, "%.2f");
		ImGui::DragFloat("wheel_stickiness",   &p.wheel_stickiness,   0.05f, 0.f,   3.f, "%.2f");
		ImGui::DragFloat("wheel_score_thresh", &p.wheel_score_thresh, 0.05f,-2.f,   2.f, "%.2f");
	}

	ImGui::SeparatorText("Clear-air resolver");
	{
		BikeAIParams& p = g_ai_params;
		ImGui::DragFloat("clear_air_lat_window",  &p.clear_air_lat_window,  0.05f, 0.1f, 3.f, "%.2f");
		ImGui::DragFloat("clear_air_long_window", &p.clear_air_long_window, 0.05f, 0.1f, 3.f, "%.2f");
		ImGui::DragFloat("clear_air_center_bias", &p.clear_air_center_bias, 0.01f, 0.f,  1.f, "%.2f");
		ImGui::DragFloat("clear_air_damp_tau",    &p.clear_air_damp_tau,    0.05f, 0.1f, 5.f, "%.2f");
		ImGui::DragFloat("clear_air_max_offset",  &p.clear_air_max_offset,  0.05f, 0.1f, 3.f, "%.2f");
		ImGui::DragFloat("clear_air_step",        &p.clear_air_step,        0.05f, 0.1f, 1.f, "%.2f");
		ImGui::DragFloat("corner_factor_r_full",  &p.corner_factor_r_full,  1.f,   5.f, 200.f, "%.0f");
		ImGui::DragFloat("corner_factor_min",     &p.corner_factor_min,     0.01f, 0.f,  1.f, "%.2f");
		ImGui::DragFloat("follower_lat_k",        &p.follower_lat_k,        0.05f, 0.f,  3.f, "%.2f");
		ImGui::SameLine(); ImGui::TextDisabled("near-field lat error → steer (followers)");
		ImGui::DragFloat("follower_lat_d_k",      &p.follower_lat_d_k,      0.02f, 0.f,  2.f, "%.2f");
		ImGui::SameLine(); ImGui::TextDisabled("damp lateral velocity");
	}

	ImGui::SeparatorText("Gap Regulation");
	{
		// These live in BikeApplication_Pack.cpp; expose via externs (non-static there)
		extern float GAP_POWER_K;
		extern float GAP_POWER_MAX_DELTA;
		extern float GAP_FREE_POWER_W;
		ImGui::DragFloat("GAP_POWER_K",         &GAP_POWER_K,         1.f,   0.f, 200.f, "%.0f");
		ImGui::SameLine(); ImGui::TextDisabled("W correction per metre of gap error");
		ImGui::DragFloat("GAP_POWER_MAX_DELTA", &GAP_POWER_MAX_DELTA, 5.f,  10.f, 600.f, "%.0f");
		ImGui::DragFloat("GAP_FREE_POWER_W",    &GAP_FREE_POWER_W,    5.f,  50.f, 800.f, "%.0f");
	}

	ImGui::SeparatorText("Paceline FSM");
	{
		BikeAIParams& p = g_ai_params;
		ImGui::DragFloat("pull_cooldown_s",    &p.pull_cooldown_s,    0.5f,  0.f, 60.f, "%.1f");
		ImGui::DragFloat("pull_duration_s",    &p.pull_duration_s,    1.f,   1.f,120.f, "%.0f");
		ImGui::DragFloat("peel_duration_s",    &p.peel_duration_s,    0.1f,  0.5f, 8.f, "%.1f");
		ImGui::DragFloat("drift_duration_s",   &p.drift_duration_s,   0.5f,  1.f, 30.f, "%.1f");
		ImGui::DragFloat("peel_offset_m",      &p.peel_offset_m,      0.05f, 0.1f, 3.f, "%.2f");
		ImGui::DragFloat("peel_power_delta_w", &p.peel_power_delta_w, 5.f, -200.f, 0.f, "%.0f");
		ImGui::DragFloat("drift_power_frac",   &p.drift_power_frac,   0.02f, 0.3f, 1.f, "%.2f");
		ImGui::DragFloat("pull_power_frac",    &p.pull_power_frac,    0.02f, 0.5f, 1.5f,"%.2f");
	}

	ImGui::SeparatorText("Riders");
	const auto& sorted = g_bike_app->riders_sorted;
	for (int i = 0; i < (int)sorted.size(); ++i) {
		const BikeObject* r = sorted[i];
		const BikeAI*     ai = dynamic_cast<const BikeAI*>(r->input.get());
		const char* mode = ai
		    ? (ai->wheel ? paceline_state_name(ai->paceline_state) : "LEAD")
		    : "player";
		ImGui::Text("P%d  dist=%.0f m  lat=%+.2f m  draft=%.2f  [%s]",
			r->race_position, r->course_dist_m, r->lateral_pos, r->draft_factor, mode);
	}

	ImGui::SeparatorText("Road Network");
	BikeCourse& course_rw = g_bike_app->course;
	ImGui::SliderFloat("Sample step (m)", &course_rw.sample_step_m, 0.2f, 5.f, "%.2f");

	ImGui::SeparatorText("Corner Fillets");
	ImGui::Checkbox("Fillet enabled", &course_rw.fillet_enabled);
	ImGui::DragFloat("Min angle (deg)", &course_rw.fillet_min_angle_deg, 0.5f, 0.f, 89.f, "%.1f");
	ImGui::Text("%d fillets active", (int)course_rw.debug_fillets.size());
	if (ImGui::Button("Rebuild Course"))
		g_bike_app->rebuild_course();

	ImGui::SeparatorText("Racing Line");
	bool rl_dirty = false;
	rl_dirty |= ImGui::DragFloat("RL k (stiffness)",   &course_rw.rl_k,            0.1f,  0.1f, 200.f,  "%.2f");
	rl_dirty |= ImGui::DragFloat("RL mass",            &course_rw.rl_mass,         1.f,   1.f,  500.f,  "%.1f");
	rl_dirty |= ImGui::DragInt  ("RL iterations",      &course_rw.rl_num_iters,    50,    100,  20000);
	rl_dirty |= ImGui::DragInt  ("RL smooth passes",   &course_rw.rl_smooth_passes, 1,     0,    100);
	ImGui::SameLine(); ImGui::TextDisabled("removes kinks from uneven waypoint spacing");
	rl_dirty |= ImGui::DragFloat("RL smooth weight",   &course_rw.rl_smooth_w,     0.01f, 0.f,  1.f,   "%.2f");
	if (rl_dirty)
		course_rw.rebuild_racing_line();
	if (ImGui::Button("Rebuild Racing Line"))
		course_rw.rebuild_racing_line();
	static bool draw_fillets = false;
	ImGui::Checkbox("Draw fillet geometry", &draw_fillets);
	if (draw_fillets)
		c.debug_draw_fillets();

	ImGui::SeparatorText("Visualisation");
	static bool draw_course         = true;
	static bool draw_projections    = true;   // sphere on spline at each rider's course_dist_m
	static bool draw_lookahead_all  = true;   // lookahead sphere for every rider
	ImGui::Checkbox("Draw course spline",          &draw_course);
	ImGui::Checkbox("Draw course projections",     &draw_projections);
	ImGui::Indent();
	ImGui::TextDisabled("Yellow = player  Orange = AI");
	ImGui::TextDisabled("Sphere on spline, line to actual position.");
	ImGui::TextDisabled("Gap shows projection error.");
	ImGui::Unindent();
	ImGui::Checkbox("Draw lookahead (all riders)", &draw_lookahead_all);
	ImGui::Indent();
	ImGui::TextDisabled("Cyan = player  Light cyan = AI");
	ImGui::Unindent();

	if (draw_course)
		c.debug_draw();

	// Lookahead parameters mirror BikeAI defaults so the player's dot is comparable.
	static constexpr float LOOK_BASE_M  = 10.f;
	static constexpr float LOOK_PER_MS  = 2.0f;
	static constexpr float CORNER_SCAN  = 50.f;
	static constexpr float CORNER_COEFF = 2.5f;

	for (auto* r : g_bike_app->all_riders) {
		const bool is_player = (dynamic_cast<BikePlayer*>(r->input.get()) != nullptr);

		if (draw_projections) {
			// Sphere ON the spline at course_dist_m. Distance from rider to this sphere
			// is the projection error — should be small except at wide racing-line offsets.
			const BikeWaypoint proj = c.sample(r->course_dist_m);
			const glm::vec3    proj_pos = proj.position + glm::vec3(0, 0.4f, 0);
			const Color32 col = is_player
			    ? Color32(0xff, 0xff, 0x00, 0xff)   // yellow  = player
			    : Color32(0xff, 0x88, 0x00, 0xff);  // orange  = AI
			Debug::add_sphere(proj_pos, 0.4f, col, -1.f);
			Debug::add_line(r->get_ws_position(), proj_pos,
			                Color32(col.r, col.g, col.b, 0x55), -1.f);
		}

		if (draw_lookahead_all) {
			glm::vec3 lookahead_pt;
			if (auto* ai = dynamic_cast<BikeAI*>(r->input.get())) {
				lookahead_pt = ai->dbg_lookahead_pt;  // already computed this frame
			} else {
				// Compute the same lookahead the AI would use, applied to the player.
				const float scan    = glm::max(CORNER_SCAN, r->speed * 0.8f);
				const float raw_r   = c.min_turn_radius_ahead(r->course_dist_m, scan);
				const float min_r   = glm::max(raw_r, 3.f);
				const float look_d  = glm::min(LOOK_BASE_M + r->speed * LOOK_PER_MS,
				                               CORNER_COEFF * min_r);
				lookahead_pt = c.racing_line_lookahead(r->course_dist_m, look_d);
			}
			const Color32 col = is_player
			    ? Color32(0x00, 0xff, 0xff, 0xff)   // cyan       = player
			    : Color32(0x88, 0xff, 0xff, 0xff);  // light cyan = AI
			Debug::add_sphere(lookahead_pt, 0.55f, col, -1.f);
			Debug::add_line(r->get_ws_position(), lookahead_pt,
			                Color32(col.r, col.g, col.b, 0x77), -1.f);
		}
	}
}
ADD_TO_DEBUG_MENU(bike_course_debug);
