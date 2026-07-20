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
#include "Input/Sdl2CompatGamepad.h"
#include <glm/gtc/matrix_transform.hpp>
#include "Render/DrawPublic.h"
#include "Render/RenderObj.h"
#include "UI/Gui.h"
#include "Debug.h"
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
			snap.actual_power_command = ai->dbg_power_final;
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
		// Reset transient physics state so the bike doesn't carry over a spin
		bo->current_steer     = 0.f;
		bo->heading_turn_rate = 0.f;
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

// Shared per-rider stats text + gizmo overlay — used by both the index-based
// follow camera below and BikeDebugger's click-to-select orbit camera.
void draw_rider_debug_info(BikeObject* bo)
{
	if (!bo) return;
	BikePlayer* bp = dynamic_cast<BikePlayer*>(bo->input.get());
	BikeAI*     ai = dynamic_cast<BikeAI*>(bo->input.get());

	// Worldspace velocity vector, drawn from the rider's (approximate) head
	// position — bike_direction*speed is the true integrated velocity, see
	// BikeObject::tick_transform.
	{
		const glm::vec3 head_pos   = bo->get_ws_position() + glm::vec3(0.f, 1.5f, 0.f);
		const glm::vec3 world_vel  = bo->bike_direction * bo->speed;
		Debug::add_line(head_pos, head_pos + world_vel, Color32(0xff, 0x00, 0xff, 0xff), -1.f);
		Debug::add_text_ex(head_pos + glm::vec3(0.f, 0.3f, 0.f),
			string_format("vel: (%.2f, %.2f, %.2f)  |v|=%.2fm/s", world_vel.x, world_vel.y, world_vel.z, bo->speed),
			Color32(0xff, 0x00, 0xff, 0xff), 0.f, true, true, true);
	}

	if (bp) {
		GameplayStatic::debug_text(string_format("[Player] steer_final:   %.2f", bp->dbg_steer_final));
		GameplayStatic::debug_text(string_format("[Steer] cur_lat=%+.2fm  cmd=%+.2f", bo->lateral_pos, bo->dbg_steer_cmd));
		GameplayStatic::debug_text(string_format("[Steer] heading target=%+.1fdeg  actual=%+.1fdeg", bo->dbg_desired_heading_offset_deg, bo->dbg_heading_offset_deg));
		GameplayStatic::debug_text(string_format("[Steer] turn_rate=%+.0fdeg/s", bo->dbg_turn_rate_dps));
	} else if (ai) {
		// --- Mode and path-following breakdown ---
		GameplayStatic::debug_text(string_format("[AI] spd=%.1fm/s  neighbors=%d  min_r=%.1fm%s",
			bo->speed, ai->dbg_num_neighbors, ai->dbg_min_r, ai->dbg_clamped ? "  CLAMPED" : ""));
		GameplayStatic::debug_text(string_format("[AI] cohesion=%+.2f  separation=%+.2f", ai->dbg_cohesion_offset, ai->dbg_separation_offset));
		GameplayStatic::debug_text(string_format("[AI] draft=%+.2f  lineform=%+.2f", ai->dbg_draft_offset, ai->dbg_lineform_offset));
		// Upcoming corner: distance, radius, safe speed, brake demand
		if (ai->dbg_brake_dist_m > 0.f)
			GameplayStatic::debug_text(string_format("[AI] corner in %.0fm  r=%.1fm  v_max=%.1fm/s  brake=%.2f",
				ai->dbg_brake_dist_m, ai->dbg_brake_corner_r, ai->dbg_v_max, ai->dbg_brake_amount));
		else
			GameplayStatic::debug_text(string_format("[AI] corner: clear for %.0fm",
				(float)(ai->BRAKE_SCAN_STEPS * ai->BRAKE_SCAN_STEP_M)));
		GameplayStatic::debug_text(string_format("[AI] target_speed=%.1fm/s  power=%.0fW", ai->dbg_target_speed, ai->dbg_power_final));

		// --- Steering breakdown ---
		GameplayStatic::debug_text(string_format("[Steer] cur_lat=%+.2fm  lat_target=%+.2fm", bo->lateral_pos, ai->dbg_target_lat_offset));
		GameplayStatic::debug_text(string_format("[Steer] lat_err=%+.2fm  cmd=%+.2f", ai->dbg_target_lat_offset - bo->lateral_pos, bo->dbg_steer_cmd));
		GameplayStatic::debug_text(string_format("[Steer] heading target=%+.1fdeg  actual=%+.1fdeg", bo->dbg_desired_heading_offset_deg, bo->dbg_heading_offset_deg));
		GameplayStatic::debug_text(string_format("[Steer] turn_rate=%+.0fdeg/s", bo->dbg_turn_rate_dps));

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

	draw_rider_debug_info(bo);
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

	ImGui::SeparatorText("Speed/power PID");
	{
		BikeAIParams& p = g_ai_params;
		ImGui::DragFloat("speed_kp",     &p.speed_kp,     1.f,  0.f, 300.f, "%.0f");
		ImGui::DragFloat("speed_ki",     &p.speed_ki,     0.5f, 0.f, 50.f,  "%.1f");
		ImGui::DragFloat("speed_kd",     &p.speed_kd,     0.5f, 0.f, 50.f,  "%.1f");
		ImGui::DragFloat("base_power_w", &p.base_power_w, 5.f, 50.f, 800.f, "%.0f");
		ImGui::DragFloat("min_power_w",  &p.min_power_w,  5.f,  0.f, 200.f, "%.0f");
		ImGui::DragFloat("max_power_w",  &p.max_power_w,  10.f, 200.f, 1500.f, "%.0f");
	}

	ImGui::SeparatorText("Hemisphere sense");
	{
		BikeAIParams& p = g_ai_params;
		ImGui::DragFloat("sense_radius_m",       &p.sense_radius_m,       0.5f, 1.f, 50.f, "%.1f");
		ImGui::DragFloat("sense_half_angle_deg", &p.sense_half_angle_deg, 1.f,  10.f, 180.f, "%.0f");
	}

	ImGui::SeparatorText("Steering");
	{
		BikeAIParams& p = g_ai_params;
		ImGui::DragFloat("steer_lookahead_m",      &p.steer_lookahead_m,      0.2f,  0.f, 20.f, "%.1f");
		ImGui::DragFloat("steer_lookahead_time_s", &p.steer_lookahead_time_s, 0.02f, 0.f, 2.f,  "%.2f");
	}

	ImGui::SeparatorText("Magnetism");
	{
		BikeAIParams& p = g_ai_params;
		ImGui::Checkbox("enable_magnetism (off = follow path only)", &p.enable_magnetism);
		ImGui::DragFloat("cohesion_k",              &p.cohesion_k,              0.02f, 0.f, 3.f,  "%.2f");
		ImGui::DragFloat("cohesion_trigger_dist_m", &p.cohesion_trigger_dist_m, 0.2f,  0.f, 30.f, "%.1f");
		ImGui::DragFloat("separation_k",            &p.separation_k,            0.05f, 0.f, 5.f,  "%.2f");
		ImGui::DragFloat("separation_dist_m",       &p.separation_dist_m,       0.05f, 0.1f, 5.f, "%.2f");
		ImGui::DragFloat("draft_k",                 &p.draft_k,                 0.05f, 0.f, 3.f,  "%.2f");
		ImGui::DragFloat("draft_dist_m",             &p.draft_dist_m,           0.5f,  0.f, 30.f, "%.1f");
		ImGui::DragFloat("lineformation_k",         &p.lineformation_k,         0.02f, 0.f, 2.f,  "%.2f");
		ImGui::DragFloat("lateral_shift_kp",        &p.lateral_shift_kp,        0.05f, 0.f, 5.f,  "%.2f");
		ImGui::DragFloat("edge_safety_m",           &p.edge_safety_m,           0.05f, 0.f, 3.f,  "%.2f");
	}

	ImGui::SeparatorText("Riders");
	const auto& sorted = g_bike_app->riders_sorted;
	for (int i = 0; i < (int)sorted.size(); ++i) {
		const BikeObject* r = sorted[i];
		const BikeAI*     ai = dynamic_cast<const BikeAI*>(r->input.get());
		const char* mode = ai ? "AI" : "player";
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
	ImGui::Checkbox("Draw lateral target (all riders)", &draw_lookahead_all);
	ImGui::Indent();
	ImGui::TextDisabled("Cyan = player  Light cyan = AI");
	ImGui::Unindent();

	if (draw_course)
		c.debug_draw();

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
			// AI: exact target lateral point computed this frame (racing line + magnetism).
			// Player: no magnetism, so just the racing line's own offset at their position.
			glm::vec3 lookahead_pt;
			if (auto* ai = dynamic_cast<BikeAI*>(r->input.get())) {
				lookahead_pt = ai->dbg_lookahead_pt;  // already computed this frame
			} else {
				lookahead_pt = c.racing_line_lookahead(r->course_dist_m, 0.f);
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
