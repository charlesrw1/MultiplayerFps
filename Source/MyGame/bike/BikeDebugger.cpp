#include "BikeDebugger.h"
#include "BikeHeaders.h"
#include "GameEnginePublic.h"
#include "Game/GameplayStatic.h"
#include "Game/Components/CameraComponent.h"
#include "Game/Components/PhysicsComponents.h"
#include "Physics/Physics2.h"
#include "Input/InputSystem.h"
#include "UI/ViewportSystem.h"
#include "Debug.h"
#include "Framework/Util.h"
#include "Framework/MathLib.h"
#include "imgui.h"
#include <glm/gtc/matrix_transform.hpp>

extern BikeGameApplication* g_bike_app;

// Gamepad steer stick shaping for manual rider control (BikeAI::manual_control,
// see BikeDebugger::update). >1 compresses small deflections for finer control
// near center, same idea as BikePlayer's steer_expo. Keyboard steer is already
// binary (+-1) so this only ever applies to the analog stick.
static float manual_gp_steer_expo = 1.5f;

void BikeDebugger::revert_manual_control(BikeObject* rider)
{
	if (!rider) return;
	if (BikeAI* ai = dynamic_cast<BikeAI*>(rider->input.get()))
		ai->manual_control = false;
}

void BikeDebugger::deselect()
{
	revert_manual_control(selected);
	selected = nullptr;
	orbiting = false;
	behind_cam_initialized = false;
}

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

	if (orbiting && selected && behind_camera_enabled) {
		// Drive-game style chase cam: same pivot/offset construction as
		// apply_debug_follow_camera() (BikeApplication_Debug.cpp), but damped
		// like BikePlayer::update_camera so the view doesn't snap on every
		// steering/lean twitch. fly_cam/MMB-orbit input is bypassed entirely
		// while this is active.
		ViewportSystem::set_game_capture_mouse(false);

		const glm::vec3 fwd      = glm::normalize(selected->bike_direction);
		const glm::vec3 world_up = glm::vec3(0, 1, 0);
		const glm::vec3 pivot    = selected->get_ws_position() + world_up * behind_cam_height_m;
		const glm::vec3 right    = glm::normalize(glm::cross(fwd, world_up));

		const glm::quat pitch_rot  = glm::angleAxis(glm::radians(behind_cam_pitch_deg), right);
		const glm::vec3 orbit_dir  = glm::normalize(pitch_rot * (-fwd));
		const glm::vec3 target_pos = pivot + orbit_dir * behind_cam_dist_m;

		// Target rotation: camera faces the same (pitched-down) direction the
		// rider is actually heading RIGHT NOW — not a direction reconstructed
		// from two independently-lagged points, which swims/distorts through
		// fast turns. The quat itself is what gets smoothed below (slerp), so
		// the rotation lag is real and decoupled from the position lag.
		const glm::vec3 cam_fwd_target   = glm::normalize(pitch_rot * fwd);
		const glm::vec3 cam_right_target = glm::normalize(glm::cross(cam_fwd_target, world_up));
		const glm::vec3 cam_up_target    = glm::normalize(glm::cross(cam_right_target, cam_fwd_target));
		const glm::quat target_rot = glm::quat(glm::mat3(cam_right_target, cam_up_target, -cam_fwd_target));

		if (!behind_cam_initialized) {
			behind_cam_pos = target_pos;
			behind_cam_rot = target_rot;
			behind_cam_initialized = true;
		} else {
			const float dt = eng->get_dt();
			behind_cam_pos = damp_dt_independent(target_pos, behind_cam_pos, behind_cam_pos_smooth_time_s, dt);
			behind_cam_rot = damp_dt_independent(target_rot, behind_cam_rot, behind_cam_rot_smooth_time_s, dt);
		}

		glm::mat4 xform = glm::mat4_cast(behind_cam_rot);
		xform[3] = glm::vec4(behind_cam_pos, 1.f);
		debug_cam_ent->set_ws_transform(xform);
	} else if (orbiting && selected) {
		behind_cam_initialized = false;
		const glm::vec3 new_target = selected->get_ws_position();
		ViewportSystem::set_game_capture_mouse(mmb);
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

		debug_cam_ent->set_ws_transform(glm::inverse(fly_cam.get_view_matrix()));
	} else {
		behind_cam_initialized = false;
		ViewportSystem::set_game_capture_mouse(rmb);
		if (fly_cam.can_take_input()) {
			auto window_size = get_app_window_size();
			const float aspect = (float)window_size.x / (float)window_size.y;
			fly_cam.update_from_input(window_size.x, window_size.y, aspect, glm::radians(cam->fov));
		}

		debug_cam_ent->set_ws_transform(glm::inverse(fly_cam.get_view_matrix()));
	}

	// Click-to-select (LMB). Ignore clicks the ImGui panel already consumed.
	if (!ImGui::GetIO().WantCaptureMouse && Input::was_mouse_pressed(0)) {
		BikeObject* picked = pick_rider_under_cursor(riders);
		if (picked) {
			// Switching to a different rider hands manual control (if active)
			// back to whichever one we're leaving -- only ever one player-driven
			// rider at a time.
			if (selected != picked)
				revert_manual_control(selected);
			selected = picked;
			orbiting = true;
			fly_cam.set_orbit_target(selected->get_ws_position(), 2.f);
			// set_orbit_target() only moves position/orbit_target, not distance —
			// without this the no-MMB re-centering below (which uses distance)
			// would see distance == 0 until the user's first MMB drag.
			fly_cam.distance = glm::length(fly_cam.orbit_target - fly_cam.position);
			fly_cam.orbit_mode = true;
		} else if (orbiting) {
			// Clicked empty space away from the selected rider -- stop orbiting
			// and hand manual control back to AI.
			revert_manual_control(selected);
			orbiting = false;
			fly_cam.orbit_mode = false;
		}
	}

	// Manual control of the selected AI rider (Selected Rider panel, BikeAI::manual_control).
	// Read every frame regardless of whether the ImGui window is open, same as
	// the click-to-select handling above.
	if (selected) {
		if (BikeAI* ai = dynamic_cast<BikeAI*>(selected->input.get())) {
			if (ai->manual_control) {
				const bool left  = Input::is_key_down(SDL_SCANCODE_A) || Input::is_key_down(SDL_SCANCODE_LEFT);
				const bool right = Input::is_key_down(SDL_SCANCODE_D) || Input::is_key_down(SDL_SCANCODE_RIGHT);
				// Sign matches BikeAI's own lateral_shift/wp.right convention (see
				// BikeObject::tick_transform) with A/left = negative = bike-left --
				// note this is inverted vs. BikePlayer::evaluate's kb_left/kb_right,
				// which feeds a differently-signed steer_combined.
				float steer = 0.f;
				if (left)  steer += 1.f;
				if (right) steer -= 1.f;

				// Gamepad: left stick X. SDL's axis is negative when pushed left,
				// positive when pushed right -- opposite of the keyboard convention
				// above, so negate. Deadzone cuts drift; expo curve (rescaled onto
				// the post-deadzone range) compresses small deflections for finer
				// control near center without losing full deflection at the edge.
				const float gp_axis = (float)Input::get_con_axis(SDL_CONTROLLER_AXIS_LEFTX);
				constexpr float GP_DEADZONE = 0.15f;
				if (glm::abs(gp_axis) > GP_DEADZONE) {
					const float mag_raw = (glm::abs(gp_axis) - GP_DEADZONE) / (1.f - GP_DEADZONE);
					const float mag_shaped = glm::pow(mag_raw, manual_gp_steer_expo);
					steer += -glm::sign(gp_axis) * mag_shaped;
				}

				ai->manual_steer_input  = glm::clamp(steer, -1.f, 1.f);
				// Feeds BikeAI's stick-active magnetism easing (see BikeAI::evaluate) --
				// a small threshold so stick drift/keyboard release doesn't flicker it.
				ai->manual_stick_active = glm::abs(steer) > 0.05f;

				// Gamepad: DPAD up/down nudges wattage in fixed steps -- coexists
				// with the ImGui power slider, which still works independently.
				static constexpr float POWER_STEP_W = 25.f;
				if (Input::was_con_button_pressed(SDL_CONTROLLER_BUTTON_DPAD_UP))
					ai->manual_power_w = glm::clamp(ai->manual_power_w + POWER_STEP_W, 0.f, 1000.f);
				if (Input::was_con_button_pressed(SDL_CONTROLLER_BUTTON_DPAD_DOWN))
					ai->manual_power_w = glm::clamp(ai->manual_power_w - POWER_STEP_W, 0.f, 1000.f);
			}
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

			const char* state_suffix =
				r->ai_behavior_state == BikeAIBehaviorState::MovingToFront  ? "\nMOVING TO FRONT" :
				r->ai_behavior_state == BikeAIBehaviorState::StayingAtFront ? "\nSTAYING AT FRONT" : "";
			const char* text = string_format("neighbors=%d%s%s", ai->dbg_num_neighbors,
				ai->dbg_clamped ? " CLAMPED" : "",
				state_suffix);

			Debug::add_text_ex(r->get_ws_position() + glm::vec3(0.f, 1.5f, 0.f), text, COLOR_WHITE, 0.f, true, true, false);
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
	ImGui::SetNextWindowBgAlpha(0.5f);
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

		ImGui::Checkbox("Behind camera (3rd person)", &behind_camera_enabled);
		if (behind_camera_enabled) {
			ImGui::TextDisabled("Replaces MMB-orbit while active; click another rider or Clear Selection to exit.");
			ImGui::DragFloat("Distance behind",  &behind_cam_dist_m,        0.1f, 0.5f, 15.f, "%.2f m");
			ImGui::DragFloat("Height above",      &behind_cam_height_m,     0.05f, 0.f, 5.f,  "%.2f m");
			ImGui::DragFloat("Pitch",             &behind_cam_pitch_deg,    0.5f, -80.f, 10.f, "%.1f deg");
			ImGui::DragFloat("Position smoothing", &behind_cam_pos_smooth_time_s, 0.005f, 0.001f, 0.95f, "%.3f");
			ImGui::DragFloat("Rotation smoothing", &behind_cam_rot_smooth_time_s, 0.005f, 0.001f, 0.95f, "%.3f");
			ImGui::TextDisabled("Lower = snappier, higher = laggier/smoother (both, independently).");
		}

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

			ImGui::SeparatorText("Behavior mode (this rider only)");
			ImGui::Checkbox("Ride 2nd wheel (target group leader)", &selected->ride_2nd_wheel_enabled);
			ImGui::TextDisabled("Locks cohesion's draft target onto this rider's own group's");
			ImGui::TextDisabled("leader, at any distance, instead of the nearest sensed rider ahead.");
			if (selected->ride_2nd_wheel_enabled)
				ImGui::Text("Group %d, pos_in_group=%.2f", selected->group_id, selected->pos_in_group_norm);

			// One-shot goal: button starts it, clicking again while active cancels
			// back to Default early. BikeAI::evaluate auto-cancels it on its own
			// once this rider actually reaches the front of its group.
			const bool moving_to_front = selected->ai_behavior_state == BikeAIBehaviorState::MovingToFront;
			if (ImGui::Button(moving_to_front ? "Cancel move to front" : "Move to front of group"))
				selected->ai_behavior_state = moving_to_front ? BikeAIBehaviorState::Default : BikeAIBehaviorState::MovingToFront;

			// Persistent goal: unlike the one-shot above, this never auto-cancels
			// once reached — the rider sprints to the front then holds there, with
			// cohesion's draft-lock suppressed so it doesn't tuck in behind whoever
			// else is at the front. Multiple riders with this enabled naturally
			// settle side-by-side (avoidance's side-by-side rule pushes them apart
			// laterally instead of single-file) rather than queuing.
			const bool staying_at_front = selected->ai_behavior_state == BikeAIBehaviorState::StayingAtFront;
			if (ImGui::Button(staying_at_front ? "Stop staying at front" : "Stay at front of group (abreast)"))
				selected->ai_behavior_state = staying_at_front ? BikeAIBehaviorState::Default : BikeAIBehaviorState::StayingAtFront;

			ImGui::SeparatorText("Player Control (this rider only)");
			if (ImGui::Checkbox("Manual control (A/D or Left/Right to steer)", &ai->manual_control)) {
				if (!ai->manual_control) ai->manual_steer_input = 0.f;
			}
			if (ai->manual_control) {
				ImGui::TextDisabled("Click another rider or Clear Selection to hand back to AI.");
				ImGui::SliderFloat("Steer sensitivity", &ai->manual_steer_sensitivity, 0.05f, 1.f, "%.2f");
				ImGui::TextDisabled("Scales full-stick down to roughly the AI's own correction range -- lower");
				ImGui::TextDisabled("if small pushes are leaning the bike much harder than the AI ever does.");
				ImGui::SliderFloat("Player power", &ai->manual_power_w, 0.f, 1000.f, "%.0f W");
				ImGui::SliderFloat("Magnetism: steering assist (ceiling)", &ai->magnetism_lateral_k, 0.f, 1.f, "%.2f");
				ImGui::SliderFloat("Magnetism: speed assist (ceiling)",    &ai->magnetism_speed_k,   0.f, 1.f, "%.2f");
				ImGui::TextDisabled("0 = fully manual, 1 = fully AI (draft/cohesion/racing line) right at the target.");
				ImGui::SliderFloat("Magnetism: active-stick multiplier", &ai->magnetism_active_mult, 0.f, 1.f, "%.2f");
				ImGui::TextDisabled("Ceiling above is multiplied by this while the stick is held -- eases assist");
				ImGui::TextDisabled("off so it doesn't fight manual input; back to full ceiling once released.");
				ImGui::SliderFloat("Magnetism: ease smoothing", &ai->magnetism_smooth_time_s, 0.01f, 1.f, "%.2f s");
				ImGui::TextDisabled("How long it takes to relax back to the full ceiling after releasing the stick.");
				ImGui::SliderFloat("Magnetism: lateral range", &ai->magnetism_falloff_range_m, 0.2f, 10.f, "%.1f m");
				ImGui::SliderFloat("Magnetism: speed range",   &ai->magnetism_speed_falloff_ms, 0.2f, 10.f, "%.1f m/s");
				ImGui::TextDisabled("A magnet, not a spring: pull is strongest right at the AI's target and fades");
				ImGui::TextDisabled("to zero over this distance/speed-error, instead of growing the further off you are.");
				ImGui::SliderFloat("Gamepad stick expo", &manual_gp_steer_expo, 1.f, 4.f, "%.2f");
				ImGui::TextDisabled("Higher = finer control near center of the left stick (keyboard unaffected).");
			}

			if (moving_to_front)
				ImGui::TextColored(ImVec4(1.f, 0.6f, 0.1f, 1.f), "State: MOVING TO FRONT (group %d, pos_in_group=%.2f)",
					selected->group_id, selected->pos_in_group_norm);
			else if (staying_at_front)
				ImGui::TextColored(ImVec4(0.2f, 0.9f, 1.f, 1.f), "State: STAYING AT FRONT (group %d, pos_in_group=%.2f)",
					selected->group_id, selected->pos_in_group_norm);
			else
				ImGui::TextDisabled("State: Default");
		} else {
			ImGui::TextDisabled("Offset/overrides only affect AI riders.");
		}

		if (ImGui::Button("Clear Selection")) {
			deselect();
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
