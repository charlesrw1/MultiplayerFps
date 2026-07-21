#include "BikeDebugger.h"
#include "BikeHeaders.h"
#include "GameEnginePublic.h"
#include "Game/GameplayStatic.h"
#include "Game/Components/CameraComponent.h"
#include "Game/Components/PhysicsComponents.h"
#include "Physics/Physics2.h"
#include "Input/InputSystem.h"
#include "UI/GUISystemPublic.h"
#include "Debug.h"
#include "Framework/Util.h"
#include "imgui.h"
#include <glm/gtc/matrix_transform.hpp>

extern BikeGameApplication* g_bike_app;

void BikeDebugger::init()
{
	debug_cam_ent = GameplayStatic::spawn_entity();
	debug_cam_ent->create_component<CameraComponent>();
}

void BikeDebugger::update(const std::vector<BikeObject*>& riders)
{
	auto* cam = debug_cam_ent.get() ? debug_cam_ent->get_component<CameraComponent>() : nullptr;
	if (!cam) return;

	if (!initialized_fly_cam) {
		fly_cam.position   = glm::vec3(0.f, 25.f, -25.f);
		fly_cam.yaw        = 0.f;
		fly_cam.pitch      = -0.7f;
		fly_cam.move_speed = 0.2f;
		initialized_fly_cam = true;
	}

	// No player camera in the AI-only race — this debug camera is the scene camera.
	cam->set_is_enabled(true);

	const bool rmb = Input::is_mouse_down(2);
	const bool mmb = Input::is_mouse_down(1);

	if (orbiting && selected) {
		const glm::vec3 new_target = selected->get_ws_position();
		UiSystem::inst->set_game_capture_mouse(mmb);
		// Sync position with the target's frame-to-frame delta FIRST, in every
		// case (MMB held or not). User_Camera::update_from_input's orbit branch
		// recomputes `distance = length(orbit_target - position)` on every call —
		// if we handed it a live (moved) orbit_target against last frame's
		// position without this pre-sync, that recompute alone would shrink
		// `distance` as an approaching rider closed the gap (and grow it as one
		// pulled away), i.e. zoom with no user input at all. Presyncing keeps
		// that recompute a no-op unless the user actually drags/scrolls this frame.
		fly_cam.position += (new_target - fly_cam.orbit_target);
		fly_cam.orbit_target = new_target;

		if (mmb) {
			// MMB held: let the user drag to change orbit angle/zoom.
			auto window_size = get_app_window_size();
			const float aspect = (float)window_size.x / (float)window_size.y;
			fly_cam.update_from_input(window_size.x, window_size.y, aspect, glm::radians(cam->fov));
		}
	} else {
		UiSystem::inst->set_game_capture_mouse(rmb);
		if (fly_cam.can_take_input()) {
			auto window_size = get_app_window_size();
			const float aspect = (float)window_size.x / (float)window_size.y;
			fly_cam.update_from_input(window_size.x, window_size.y, aspect, glm::radians(cam->fov));
		}
	}

	debug_cam_ent->set_ws_transform(glm::inverse(fly_cam.get_view_matrix()));

	// Click-to-select (LMB). Ignore clicks the ImGui panel already consumed.
	if (!ImGui::GetIO().WantCaptureMouse && Input::was_mouse_pressed(0)) {
		BikeObject* picked = pick_rider_under_cursor(riders);
		if (picked) {
			selected = picked;
			orbiting = true;
			fly_cam.set_orbit_target(selected->get_ws_position(), 2.f);
			// set_orbit_target() only moves position/orbit_target, not distance —
			// without this the no-MMB re-centering below (which uses distance)
			// would see distance == 0 until the user's first MMB drag.
			fly_cam.distance = glm::length(fly_cam.orbit_target - fly_cam.position);
			fly_cam.orbit_mode = true;
		} else if (orbiting) {
			orbiting = false;
			fly_cam.orbit_mode = false;
		}
	}

	if (orbiting && selected) {
		draw_rider_debug_info(selected);
		if (draw_avoidance_box)
			draw_rider_avoidance_box(selected, draw_avoidance_soft_box);
	}

	if (draw_rider_state_text) {
		for (BikeObject* r : riders) {
			BikeAI* ai = dynamic_cast<BikeAI*>(r->input.get());
			if (!ai) continue;

			const char* text = string_format("neighbors=%d%s", ai->dbg_num_neighbors,
				ai->dbg_clamped ? " CLAMPED" : "");

			Debug::add_text_ex(r->get_ws_position() + glm::vec3(0.f, 1.5f, 0.f), text, COLOR_WHITE, 0.f,true,true, true);
		}
	}
}

// Casts a ray from the camera through the mouse cursor against each rider's
// kinematic pick-sphere (BikeObject::start(), PL::Character layer, trigger-only)
// rather than doing screen-space math against rider positions — a real physics
// raycast handles occlusion/silhouette correctly and matches how the editor's
// own viewport picking works.
BikeObject* BikeDebugger::pick_rider_under_cursor(const std::vector<BikeObject*>& riders) const
{
	if (riders.empty()) return nullptr;

	auto* cam = debug_cam_ent->get_component<CameraComponent>();
	if (!cam) return nullptr;

	auto window_size = get_app_window_size();
	const float aspect = (float)window_size.x / (float)window_size.y;
	const glm::mat4 view      = fly_cam.get_view_matrix();
	const glm::mat4 proj      = glm::perspective(glm::radians(cam->fov), aspect, 0.1f, 2000.f);
	const glm::mat4 inv_view_proj = glm::inverse(proj * view);

	const glm::ivec2 mouse_pos = Input::get_mouse_pos();
	const float ndc_x =        (float)mouse_pos.x / (float)window_size.x  * 2.f - 1.f;
	const float ndc_y = 1.f - ((float)mouse_pos.y / (float)window_size.y) * 2.f;

	glm::vec4 near_pt = inv_view_proj * glm::vec4(ndc_x, ndc_y, -1.f, 1.f);
	glm::vec4 far_pt  = inv_view_proj * glm::vec4(ndc_x, ndc_y,  1.f, 1.f);
	near_pt /= near_pt.w;
	far_pt  /= far_pt.w;

	const uint32_t character_mask = (uint32_t)(1 << (int)PL::Character);
	world_query_result hit;
	if (!g_physics.trace_ray(hit, glm::vec3(near_pt), glm::vec3(far_pt), nullptr, character_mask))
		return nullptr;
	if (!hit.component) return nullptr;

	Entity* hit_owner = hit.component->get_owner();
	if (!hit_owner) return nullptr;
	BikeObject* picked = hit_owner->get_component<BikeObject>();
	if (!picked) return nullptr;

	// Sanity check: only accept if it's actually one of our riders.
	for (BikeObject* r : riders)
		if (r == picked) return picked;
	return nullptr;
}

void BikeDebugger::on_imgui()
{
	if (!ImGui::Begin("Bike Debugger")) {
		ImGui::End();
		return;
	}

	ImGui::TextDisabled("RMB+WASD: fly   LMB: select rider   MMB-drag: orbit selected rider");

	if (selected) {
		ImGui::SeparatorText("Selected Rider");
		ImGui::Text("Race position: P%d   Speed: %.1f m/s", selected->race_position, selected->speed);

		// Slider range is the road's actual half-width at the rider's current
		// position — "offset is a measure between centre line and sides of
		// road", so the slider itself shouldn't be able to ask for anything the
		// road can't represent. Also hard-clamped again downstream (BikeAI's
		// normal edge_safety_m clamp) once the road curves under the rider.
		float road_hw = 4.f;
		if (g_bike_app && g_bike_app->course.is_built)
			road_hw = g_bike_app->course.sample(selected->course_dist_m).road_half_width;
		ImGui::SliderFloat("Manual lateral offset", &selected->manual_lateral_offset, -road_hw, road_hw, "%.2f m");
		ImGui::Checkbox("Draw avoidance box + avoid vectors", &draw_avoidance_box);
		if (draw_avoidance_box) {
			ImGui::SameLine();
			ImGui::Checkbox("+ soft box", &draw_avoidance_soft_box);
		}
		if (BikeAI* ai = dynamic_cast<BikeAI*>(selected->input.get())) {
			ImGui::Text("Offset blend: %.0f%% (0%% mid-corner, 100%% on a straight)", ai->offset_blend * 100.f);

			// Hard overrides — unlike the offset above, these bypass the AI's own
			// decision-making entirely rather than biasing it. Meant for forcing
			// one rider into an odd state (too slow, parked off-line, etc.) to see
			// how the REST of the pack reacts — draft/separation/avoidance all key
			// off this rider's real sensed speed/lateral_pos regardless of what's
			// driving them, so an overridden rider is a legitimate stimulus.
			ImGui::SeparatorText("AI Overrides (this rider only)");
			ImGui::Checkbox("Override target speed", &selected->ai_override_speed_enabled);
			if (selected->ai_override_speed_enabled) {
				ImGui::SliderFloat("Target speed", &selected->ai_override_target_speed_ms, 0.f, 25.f, "%.1f m/s");
				ImGui::SameLine();
				ImGui::TextDisabled("(%.1f km/h)", selected->ai_override_target_speed_ms * 3.6f);
			}
			ImGui::Checkbox("Override lateral position", &selected->ai_override_lateral_enabled);
			if (selected->ai_override_lateral_enabled)
				ImGui::SliderFloat("Target lateral", &selected->ai_override_lateral_pos_m, -road_hw, road_hw, "%.2f m");
		} else {
			ImGui::TextDisabled("Offset/overrides only affect AI riders.");
		}

		if (ImGui::Button("Clear Selection")) {
			selected = nullptr;
			orbiting = false;
			fly_cam.orbit_mode = false;
		}
	} else {
		ImGui::TextDisabled("No rider selected — click one to orbit and inspect it.");
	}

	ImGui::SeparatorText("Riders");
	ImGui::Checkbox("Draw rider state text", &draw_rider_state_text);

	if (g_bike_app) {
		ImGui::SeparatorText("Course");
		bool draw_racing_line = g_bike_app->draw_racing_line_debug;
		if (ImGui::Checkbox("Draw racing spline", &draw_racing_line))
			g_bike_app->set_draw_racing_line(draw_racing_line);

		// Padding fraction of road_half_width the racing line stays within, so it doesn't ride
		// right against the track edge. Rebuilding the road re-runs the racing-line simulation
		// with the new margin and refreshes the road/racing-line meshes to match.
		float margin = g_bike_app->course.rl_margin;
		if (ImGui::SliderFloat("Racing line margin", &margin, 0.5f, 1.0f, "%.2f")) {
			g_bike_app->course.rl_margin = margin;
			g_bike_app->rebuild_course();
		}
	}

	ImGui::SeparatorText("Time Control");

	bool paused = g_slomo.get_float() < 0.001f;
	if (ImGui::Button(paused ? "Play" : "Pause"))
		g_slomo.set_float(paused ? 1.0f : 0.0001f);

	ImGui::SameLine();
	float slomo = g_slomo.get_float();
	if (ImGui::SliderFloat("Time Scale", &slomo, 0.0001f, 5.0f, "%.3f", ImGuiSliderFlags_Logarithmic))
		g_slomo.set_float(slomo);

	if (ImGui::Button("1x")) g_slomo.set_float(1.0f);
	ImGui::SameLine();
	if (ImGui::Button("0.5x")) g_slomo.set_float(0.5f);
	ImGui::SameLine();
	if (ImGui::Button("0.25x")) g_slomo.set_float(0.25f);
	ImGui::SameLine();
	if (ImGui::Button("0.1x")) g_slomo.set_float(0.1f);

	ImGui::End();
}
