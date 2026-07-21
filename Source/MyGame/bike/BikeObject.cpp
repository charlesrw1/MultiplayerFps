#include "BikeHeaders.h"
#include "BikeObject_Local.h"
#include "Debug.h"

#include "Game/GameplayStatic.h"

// Wind state accessed via g_wind (defined in BikeWind.cpp)
#include "Game/Components/MeshComponent.h"
#include "Game/Components/PhysicsComponents.h"
#include "Game/Prefab.h"
#include "Render/Model.h"
#include "Render/MaterialPublic.h"
#include "Framework/MathLib.h"
#include "Framework/Config.h"
#include "Framework/PidTuner.h"
#include "Physics/Physics2.h"
#include "imgui.h"
#include <cfloat>

// ============================================================
// Transform / gear tuning vars
// ============================================================

// Gear shift cooldown
static float bike_gear_shift_cooldown = 3.f;

// Worldspace steering — see tick_transform. Declared extern in BikeObject_Local.h.
// bike_heading_gains is THE steering PID (kp/ki/kd on heading angle error,
// output = angular acceleration). Defaults reproduce the old fixed damped-
// spring behavior (kp=8 rad/s^2/rad, kd=2*sqrt(kp) for critical damping,
// ki=0) as a starting point now that it's actually tunable.
PidGains bike_heading_gains        = { 16.f, 0.f, 5.4f };
float bike_heading_integral_clamp  = 1.f;    // anti-windup clamp on the heading PID's accumulated error (rad*s)
float bike_heading_max_offset_deg  = 35.f;   // max heading deviation from track tangent at |ci.lateral_shift|=1
float bike_heading_turn_rate_dps   = 180.f;  // max angular velocity the heading can turn at, independent of speed
float bike_heading_turn_accel_dps2 = 400.f;  // max angular acceleration — also the heading PID's output clamp
float bike_min_turn_radius_m       = 4.5f;   // physical curvature limit — max turn rate is also capped at speed/this

static BikeObject* s_bike_debug = nullptr;  // set each tick for debug menu

// ============================================================
// BikeObject
// ============================================================

// Small palette of saturated jersey colors — avoids random RGB occasionally
// landing on a muddy brown/grey, keeps riders visually distinct.
static const Color32 s_jersey_palette[] = {
	{ 220, 30,  30,  255 },  // red
	{ 30,  80,  220, 255 },  // blue
	{ 40,  180, 60,  255 },  // green
	{ 230, 200, 20,  255 },  // yellow
	{ 230, 120, 20,  255 },  // orange
	{ 150, 40,  200, 255 },  // purple
	{ 30,  190, 200, 255 },  // cyan
	{ 230, 40,  150, 255 },  // pink
	{ 20,  20,  20,  255 },  // black
	{ 240, 240, 240, 255 },  // white
};

// Strips any auto-generated collision off a spawned cosmetic mesh (recursing
// into children, e.g. "fork" under "frame") — MeshComponent::add_collision_if_available
// defaults to true, and the prefab's primitive shapes (capsule/sphere/cube) all
// carry baked-in collision, which was self-hitting the terrain-follow raycast.
static void strip_cosmetic_collision(Entity* e)
{
	if (!e) return;
	if (auto* mesh = e->get_component<MeshComponent>())
		mesh->set_add_collision(false);
	if (auto* collider = e->get_component<MeshColliderComponent>())
		collider->destroy();
	for (Entity* child : e->get_children())
		strip_cosmetic_collision(child);
}

void BikeObject::start()
{
	ASSERT(get_owner() != nullptr);
	set_ticking(true);
	bike_direction = glm::vec3(0.f, 0.f, 1.f);
	visual_heading = bike_direction;

	// Assign palette colors by spawn order, not randomly, so concurrently-alive
	// riders never duplicate a jersey color (as long as rider count <= palette size).
	static constexpr int NUM_JERSEY_COLORS = (int)(sizeof(s_jersey_palette) / sizeof(s_jersey_palette[0]));
	static int s_next_jersey_index = 0;
	const Color32 jersey_color = s_jersey_palette[s_next_jersey_index % NUM_JERSEY_COLORS];
	++s_next_jersey_index;

	// Cosmetics-only: bike_rider.tprefab has no logic components, just the
	// frame/fork/rider meshes, parented onto this (logic) entity.
	// `transform` must be the bike's current world transform, not identity:
	// PrefabAsset::spawn places roots at transform * authored_local_offset, then
	// bakes THAT world position into the parent-relative offset. Passing identity
	// placed the mesh near world origin and baked in a bogus local offset that
	// then swung wildly as the bike rotated (steering lean / terrain roll).
	auto roots = PrefabAsset::spawn("bike/bike_rider.tprefab", get_owner()->get_ws_transform(), get_owner());
	for (const EntityPtr& root : roots) {
		if (!root) continue;
		strip_cosmetic_collision(root.get());

		if (root->get_editor_name() == "rider_body") {
			if (auto* mesh = root->get_component<MeshComponent>()) {
				jersey_mat = imaterials->create_dynmaic_material(MaterialInstance::load("defaultPBR.mm"));
				// colorMult is declared "VAR vec4" in defaultPBR.mm, which the material
				// parser treats as MatParamType::Vector (a packed Color32), not FloatVec —
				// set_floatvec_parameter silently no-ops against it (param-type mismatch).
				jersey_mat->set_u8vec_parameter("colorMult", jersey_color);
				mesh->set_material_override(jersey_mat.get());
			}
		} else if (root->get_editor_name() == "frame") {
			for (Entity* child : root->get_children()) {
				if (child->get_editor_name() == "fork")
					fork_entity = child;
			}
		}
	}

	// Click-to-select target for BikeDebugger: a kinematic sphere on the
	// Character layer (never touched by the terrain-follow raycast above, which
	// explicitly excludes that layer). Must NOT be a trigger — PhysicsBody::
	// set_shape_flags() only sets PxShapeFlag::eSCENE_QUERY_SHAPE on non-trigger
	// shapes, so a trigger shape is invisible to raycasts entirely (this is what
	// broke click-to-select the first time). Kinematic means it never moves
	// under simulation, so it's safe to leave as a normal (blocking) shape.
	auto* pick = get_owner()->create_component<SphereComponent>();
	pick->set_radius(1.1f);
	pick->set_body_type(BodyType::Kinematic);
	pick->set_physics_layer(PL::Character);
}

void BikeObject::update()
{
	ASSERT(eng != nullptr);
	s_bike_debug = this;
	if (input)
		input->evaluate(this);
}

BikeObject::ControlInput BikeObject::update_tick(ControlInput ci)
{
	ASSERT(eng != nullptr);
	const float dt = eng->get_dt();
	tick_physics(ci, dt);
	tick_steer(ci, dt);
	tick_gears(dt);
	tick_transform(ci, dt);
	return ci;
}

// ------------------------------------------------------------
void BikeObject::tick_gears(float dt)
{
	ASSERT(dt > 0.f);
	speed_smoothed      = damp_dt_independent(speed, speed_smoothed, 0.04f, dt);
	gear_shift_cooldown = glm::max(0.f, gear_shift_cooldown - dt);
	just_shifted        = false;

	if (gear_shift_cooldown == 0.f) {
		int   best_gear = gear.current_high_gear;
		float best_diff = FLT_MAX;
		for (int i = 0; i < (int)gear.back_cogs.size(); i++) {
			const float ratio = (float)gear.front_cogs[gear.current_low_gear] / (float)gear.back_cogs[i];
			const float diff  = glm::abs(speed_smoothed / (ratio * BIKE_WHEEL_CIRC) - 1.5f);
			if (diff < best_diff) { best_diff = diff; best_gear = i; }
		}
		const float cur_ratio  = (float)gear.front_cogs[gear.current_low_gear]
		                       / (float)gear.back_cogs[gear.current_high_gear];
		const float cur_rpm    = (speed_smoothed > 0.1f) ? (speed_smoothed / (cur_ratio * BIKE_WHEEL_CIRC)) * 60.f : 0.f;
		if (best_gear != gear.current_high_gear && (cur_rpm < 80.f || cur_rpm > 95.f)) {
			gear.current_high_gear = best_gear;
			gear_shift_cooldown    = bike_gear_shift_cooldown;
			just_shifted           = true;
		}
	}

	const float gear_ratio = (float)gear.front_cogs[gear.current_low_gear]
	                       / (float)gear.back_cogs[gear.current_high_gear];
	cadence = (speed_smoothed > 0.1f) ? (speed_smoothed / (gear_ratio * BIKE_WHEEL_CIRC)) : 0.f;
}

// ------------------------------------------------------------
// Worldspace movement: worldspace position is authoritative — it integrates
// from bike_direction*speed every tick, never resampled from the course rail.
// course_dist_m/lateral_pos are DERIVED each tick (course->project of the new
// position) for AI targeting / braking lookahead / lean-vs-curvature; they
// feed back into nothing but next tick's projection window hint. This means
// track-parametrization quirks (fillet seams, uneven waypoint spacing) can no
// longer teleport the bike sideways in corners — only real steering moves it.
// See [[bike/bikeai#Worldspace steering]].
//
// ci.lateral_shift is the only lateral control: it bends bike_direction away
// from the track tangent (toward wp.right for +ve) via bike_heading_gains, a
// real PID (kp/ki/kd on heading angle error, output = angular acceleration —
// see PidTuner.h). heading_turn_rate is persistent angular momentum, not a
// rate-capped snap — the PID's output accelerates it, capped both by a flat
// rate limit and by the bike's physical curvature limit
// (speed / bike_min_turn_radius_m), so slow riders can't out-turn their own
// geometry. There is no hard clamp keeping the bike on the road surface
// anymore (that was a rail-space guarantee) — staying on track is now
// entirely the AI's job, via how aggressively it closes the lateral error.
// ------------------------------------------------------------
void BikeObject::tick_transform(const ControlInput& ci, float dt)
{
	ASSERT(dt > 0.f);
	ASSERT(course != nullptr);

	// Captured before any of this tick's movement, so the real displacement
	// below reflects actual worldspace motion (rail advance + lateral shift +
	// terrain height change), not a reconstruction from rail-space components.
	const glm::vec3 prev_ws_pos = get_owner()->get_ws_position();

	// ---- Heading: track tangent bent by the steer command ----
	// NOTE: wp.right (BikeCourse's cross(WORLD_UP, forward) convention) is the
	// exact opposite handedness of this file's own cross(bike_direction, up)
	// "right" used below for the orientation basis. Positive ci.steer/lean is
	// tuned against THIS file's convention (see tick_steer), so callers
	// feeding ci.steer from ci.lateral_shift must negate it — see BikeAI.cpp /
	// BikePlayer::evaluate.
	const BikeWaypoint wp_here     = course->sample(course_dist_m);
	const float steer_cmd          = glm::clamp(ci.lateral_shift, -1.f, 1.f);
	const float heading_offset_rad = steer_cmd * glm::radians(bike_heading_max_offset_deg);
	const glm::vec3 desired_heading = glm::normalize(wp_here.forward + wp_here.right * glm::tan(heading_offset_rad));

	// Signed angle (about world up) from bike_direction to desired_heading.
	const float angle_to_target = std::atan2(glm::dot(glm::vec3(0, 1, 0), glm::cross(bike_direction, desired_heading)),
	                                          glm::dot(bike_direction, desired_heading));

	// Max achievable turn rate: the flat cap, further limited by the bike's
	// physical curvature limit (a real bike can't out-turn its own geometry —
	// turn rate at a given curvature is speed/radius, so slower riders turn
	// slower even if the flat cap would allow more).
	const float max_omega_flat      = glm::radians(bike_heading_turn_rate_dps);
	const float max_omega_curvature = (bike_min_turn_radius_m > 0.01f) ? (speed / bike_min_turn_radius_m) : max_omega_flat;
	const float max_omega           = glm::min(max_omega_flat, max_omega_curvature);

	// THE steering PID: bike_heading_gains (kp/ki/kd) on heading angle error,
	// output = angular acceleration — this is the one real feedback controller
	// in the whole steering chain (BikeAI's lateral guidance upstream is
	// deliberately proportional-only; it just sets angle_to_target's setpoint).
	// D term uses heading_turn_rate directly (the measured d(angle)/dt) rather
	// than a finite-difference of the error, so there's no derivative kick.
	// heading_turn_rate is real momentum: the PID's output is an acceleration,
	// clamped to bike_heading_turn_accel_dps2 (the "actuator torque" limit),
	// not applied directly to the angle — the bike's actual "weight" comes
	// from that accel cap, not from the gains themselves. Tune it live with
	// the widget in the Worldspace Steering debug menu below.
	heading_error_integral = glm::clamp(heading_error_integral + angle_to_target * dt,
	                                     -bike_heading_integral_clamp, bike_heading_integral_clamp);
	const float heading_derivative = -heading_turn_rate;
	const float pid_accel = bike_heading_gains.kp * angle_to_target
	                       + bike_heading_gains.ki * heading_error_integral
	                       + bike_heading_gains.kd * heading_derivative;
	const float max_turn_accel = glm::radians(bike_heading_turn_accel_dps2);
	const float turn_accel     = glm::clamp(pid_accel, -max_turn_accel, max_turn_accel);
	heading_turn_rate = glm::clamp(heading_turn_rate + turn_accel * dt, -max_omega, max_omega);

	bike_direction = glm::normalize(glm::vec3(
	    glm::rotate(glm::mat4(1.f), heading_turn_rate * dt, glm::vec3(0, 1, 0)) * glm::vec4(bike_direction, 0.f)));

	// ---- Steering debug (on-screen overlay, draw_rider_debug_info) ----
	dbg_steer_cmd                  = steer_cmd;
	dbg_desired_heading_offset_deg = glm::degrees(heading_offset_rad);
	dbg_turn_rate_dps              = glm::degrees(heading_turn_rate);
	{
		const float actual_angle = std::atan2(glm::dot(glm::vec3(0, 1, 0), glm::cross(wp_here.forward, bike_direction)),
		                                       glm::dot(wp_here.forward, bike_direction));
		dbg_heading_offset_deg = glm::degrees(actual_angle);
	}

	// ---- Position: integrate worldspace velocity directly ----
	glm::vec3 pos = prev_ws_pos + bike_direction * speed * dt;

	// Avoidance slide: a direct worldspace displacement, already a full 3D
	// vector (computed by BikeAI in its OWN local frame, not the course's),
	// NOT routed through bike_direction/heading at all. Deliberately bypasses
	// the heading PID's momentum/accel-cap chain — going through heading meant
	// the turn kept accelerating past what was needed, then had to unwind the
	// same way once clear, producing a spring/overshoot "yoyo" between
	// avoidance and cohesion pulling back in. A direct velocity has no memory:
	// it's proportional to this tick's conflict severity only, so it decays to
	// zero exactly as fast as the conflict does, no unwind needed. See
	// BikeAI.cpp section 3 / ControlInput::avoidance_lateral_vel.
	pos += ci.avoidance_lateral_vel * dt;

	// ---- Re-derive rail state from the new worldspace position ----
	const float lateral_pos_before = lateral_pos;
	float new_lateral = lateral_pos;
	int   new_segment = course_segment;
	course_dist_m  = course->project(pos, &new_lateral, &new_segment, course_dist_m);
	lateral_pos    = new_lateral;
	course_segment = new_segment;
	lateral_vel    = (dt > 1e-6f) ? (lateral_pos - lateral_pos_before) / dt : 0.f;

	// Terrain raycasts
	// The fork rotates around the head-tube hinge, not the bike origin.
	// Fork entity local position is (0, 0.747, -0.349). The local Z column of
	// the bike's orientation matrix is -bike_direction, so local z=-0.349 maps
	// to +0.349 along bike_direction in world space.
	// The fork leg length (hinge to front axle) is BIKE_FRONT_Z - hinge_forward.
	// When steered, the front contact patch rotates around the hinge, not origin.
	static constexpr float FORK_HINGE_FWD = 0.349f;
	static constexpr float FORK_LEG       = BIKE_FRONT_Z - FORK_HINGE_FWD; // ~0.63884
	const float ray_up    = 1.5f;
	const float ray_reach = 4.0f;
	const float steer_max_rad_t = compute_max_steer_rad(speed);
	const float steer_angle_t   = current_steer * steer_max_rad_t;
	const glm::vec3 steered_front_dir = glm::normalize(
	    glm::mat3(glm::rotate(glm::mat4(1.f), -steer_angle_t, glm::vec3(0, 1, 0))) * bike_direction);
	const glm::vec3 hinge_pos = pos + bike_direction * FORK_HINGE_FWD;
	const glm::vec3 front_org = hinge_pos + steered_front_dir * FORK_LEG + glm::vec3(0, ray_up, 0);
	const glm::vec3 rear_org  = pos + bike_direction * BIKE_REAR_Z  + glm::vec3(0, ray_up, 0);
	const glm::vec3 down      = glm::vec3(0, -(ray_up + ray_reach), 0);

	// Excludes PL::Character so this never hits a rider's own pick-sphere (or any
	// other rider) instead of the ground — was causing riders to bounce vertically.
	const uint32_t terrain_mask = ~(uint32_t)(1 << (int)PL::Character);
	world_query_result front_res, rear_res;
	const bool front_hit = g_physics.trace_ray(front_res, front_org, front_org + down, nullptr, terrain_mask);
	const bool rear_hit  = g_physics.trace_ray(rear_res,  rear_org,  rear_org  + down, nullptr, terrain_mask);

	// When steered, the front contact is laterally displaced from bike_direction.
	// slope_vec (front - rear) therefore has a lateral component that would corrupt:
	//   - terrain_gradient (diagonal XZ distance > forward distance → underestimates slope)
	//   - terrain_forward_dir (points sideways, misaligning pitch orientation)
	//   - orig_t for pos.y (was hardcoded to unsteered BIKE_FRONT_Z)
	// Fix: measure gradient and height interpolation along the bike's forward axis only.
	const float front_fwd = FORK_HINGE_FWD + glm::cos(steer_angle_t) * FORK_LEG;
	terrain_forward_dir = bike_direction;
	float pitch_rise = 0.f, pitch_horiz = 1.f;  // captured for the visual_terrain_forward blend below
	if (front_hit && rear_hit) {
		const float rise    = front_res.hit_pos.y - rear_res.hit_pos.y;
		const float horiz   = front_fwd - BIKE_REAR_Z; // forward span along bike axis
		terrain_gradient    = atan2f(rise, horiz);
		terrain_forward_dir = glm::normalize(bike_direction * horiz + glm::vec3(0, rise, 0));
		pitch_rise = rise; pitch_horiz = horiz;
		const float orig_t  = -BIKE_REAR_Z / horiz;
		pos.y = rear_res.hit_pos.y + rise * orig_t;
	} else if (rear_hit)  { pos.y = rear_res.hit_pos.y;  terrain_gradient = 0.f; }
	else if (front_hit)   { pos.y = front_res.hit_pos.y; terrain_gradient = 0.f; }
	else                  { terrain_gradient = 0.f; }

	// Wheel trail debug lines
	{
		const glm::vec3 fc = front_hit ? front_res.hit_pos
		                               : (hinge_pos + steered_front_dir * FORK_LEG);
		const glm::vec3 rc = rear_hit  ? rear_res.hit_pos  : (pos + bike_direction * BIKE_REAR_Z);
		if (wheel_history_initialized) {
			Debug::add_line(prev_front_wheel_pos, fc, COLOR_CYAN,  3.f, false);
			Debug::add_line(prev_rear_wheel_pos,  rc, COLOR_GREEN, 3.f, false);
		}
		prev_front_wheel_pos = fc;
		prev_rear_wheel_pos  = rc;
		wheel_history_initialized = true;
	}

	// Bump detection
	prev_gradient = damp_dt_independent(terrain_gradient, prev_gradient, 0.08f, dt);
	bump_impulse  = glm::abs(terrain_gradient - prev_gradient) * speed;

	// Smoothed VISUAL heading: the model's yaw tracks the bike's actual
	// worldspace displacement this tick (not the rail tangent directly), so it
	// turns based on real movement — including any lateral shift and, under a
	// hard force-onto-racing-line snap, the snap itself. Low-passed so it eases
	// in rather than snapping every tick. bike_direction stays the exact,
	// un-smoothed rail tangent (used for sensing/wind/raycast-probe placement
	// above); only rendering below reads visual_heading.
	{
		const glm::vec3 real_vel = (pos - prev_ws_pos) / dt;
		const float real_speed = glm::length(real_vel);
		const glm::vec3 target_heading = (real_speed > 0.05f) ? (real_vel / real_speed) : bike_direction;
		visual_heading = glm::normalize(damp_dt_independent(target_heading, visual_heading, 0.03f, dt));
	}
	// visual_terrain_forward mirrors terrain_forward_dir's pitch blend (see
	// above), but built on visual_heading — used only for the orientation
	// quat below, so the smoothed yaw survives slope pitching too.
	const glm::vec3 visual_terrain_forward = glm::normalize(visual_heading * pitch_horiz + glm::vec3(0, pitch_rise, 0));

	// Orientation: smoothed yaw (visual_heading) + terrain pitch + roll
	const glm::vec3 right       = glm::normalize(glm::cross(visual_heading, glm::vec3(0, 1, 0)));
	const glm::vec3 terrain_up  = glm::normalize(glm::cross(right, visual_terrain_forward));
	const glm::quat base_orient = glm::quat(glm::mat3(right, terrain_up, -visual_terrain_forward));
	const glm::quat orient      = glm::angleAxis(current_roll, visual_terrain_forward) * base_orient;

	get_owner()->set_ws_position_rotation(pos, orient);

	// Fork steer rotation
	if (fork_entity) {
		static constexpr float HEAD_TUBE_RAD = glm::radians(17.77f);
		const float max_steer_rad = compute_max_steer_rad(speed);
		const float fork_angle    = current_steer * max_steer_rad;
		// Non-linear steer response: large amplification at small steer, smaller at full steer.
		// Then lean fade reduces bars further toward neutral at full lean.
		const float steer_abs  = glm::abs(current_steer);
		const float steer_scale = glm::mix(bar_scale_lo_steer, bar_scale_hi_steer, steer_abs);
		const float lean_frac  = glm::clamp(glm::abs(current_roll) / glm::radians(lean_max_deg), 0.f, 1.f);
		const float lean_fade  = glm::smoothstep(bar_lean_fade_lo, bar_lean_fade_hi, lean_frac);
		const float bar_scale  = glm::mix(steer_scale, bar_visual_lean_min, lean_fade);
		const float fork_visual = fork_angle * bar_scale - current_roll * glm::sin(HEAD_TUBE_RAD);
		fork_entity->set_ls_euler_rotation(glm::vec3(HEAD_TUBE_RAD, -fork_visual, 0.f));
	}
}

float BikeObject::get_wind_along_bike() const
{
	ASSERT(true);  // pure computation, no preconditions
	const float eff_wind_speed = g_wind.wind_speed * (1.f + g_wind.wind_gust_factor * 1.5f);
	const glm::vec3 wdir_norm = glm::length(g_wind.wind_direction) > 0.001f
		? glm::normalize(g_wind.wind_direction) : glm::vec3(0.f);
	const float wind_along_bike = glm::dot(wdir_norm, bike_direction) * eff_wind_speed;

	return wind_along_bike;
}

glm::vec3 BikeObject::get_wind_along_bike_vector() const
{
	ASSERT(true);  // pure computation, no preconditions
	const float eff_wind_speed = g_wind.wind_speed * (1.f + g_wind.wind_gust_factor * 1.5f);

	return g_wind.wind_direction * eff_wind_speed - bike_direction * speed;
}

// ============================================================
// Debug menu: Transform / Gears
// ============================================================

static void bike_transform_debug()
{
	ImGui::SeparatorText("Gear");
	{
		ImGui::InputFloat("shift_cooldown", &bike_gear_shift_cooldown);
		if (s_bike_debug)
			ImGui::Text("cadence=%.2f  gear=%d/%d",
				s_bike_debug->cadence,
				s_bike_debug->gear.current_high_gear,
				s_bike_debug->gear.current_low_gear);
	}
	ImGui::SeparatorText("Handlebar Visual");
	{
		ImGui::DragFloat("bar_scale_lo_steer",  &bar_scale_lo_steer,  0.1f, 0.5f, 12.f);
		ImGui::DragFloat("bar_scale_hi_steer",  &bar_scale_hi_steer,  0.1f, 0.1f,  6.f);
		ImGui::DragFloat("bar_visual_lean_min", &bar_visual_lean_min, 0.02f, 0.f,  1.f);
		ImGui::DragFloat("bar_lean_fade_lo",    &bar_lean_fade_lo,    0.01f, 0.f,  1.f);
		ImGui::DragFloat("bar_lean_fade_hi",    &bar_lean_fade_hi,    0.01f, 0.f,  1.f);
	}
	ImGui::SeparatorText("Worldspace Steering — the steering PID");
	{
		ImGui::TextDisabled("BikeAI's lateral guidance (Steering section above) just sets this PID's setpoint.");
		imgui_pid_gains_editor("bike_heading_gains (kp/ki/kd on heading angle, rad/s^2 output)", bike_heading_gains,
		                       0.2f, 60.f, 0.05f, 10.f, 0.2f, 30.f);
		ImGui::DragFloat("bike_heading_integral_clamp", &bike_heading_integral_clamp, 0.05f, 0.f, 5.f);

		// Live visualizer: simulates the exact same PID -> accel -> rate -> angle
		// chain as tick_transform above, driven by the SAME bike_heading_gains, so
		// dragging the target here and watching it settle previews in-game response
		// before touching a rider. accel/rate limits mirror the real caps below.
		static PidVisualizerState s_heading_pid_viz;
		imgui_pid_visualizer("Heading PID response preview", s_heading_pid_viz, bike_heading_gains,
		                     glm::radians(bike_heading_turn_accel_dps2), glm::radians(bike_heading_turn_rate_dps),
		                     glm::radians(bike_heading_max_offset_deg));

		ImGui::SeparatorText("Physical limits (not gains)");
		ImGui::DragFloat("bike_heading_max_offset_deg",  &bike_heading_max_offset_deg,  0.5f, 5.f,  80.f);
		ImGui::DragFloat("bike_heading_turn_rate_dps",   &bike_heading_turn_rate_dps,   1.f,  10.f, 400.f);
		ImGui::DragFloat("bike_heading_turn_accel_dps2", &bike_heading_turn_accel_dps2, 5.f,  20.f, 2000.f);
		ImGui::DragFloat("bike_min_turn_radius_m",       &bike_min_turn_radius_m,       0.1f, 0.5f, 15.f);
	}
}
ADD_TO_DEBUG_MENU(bike_transform_debug);
