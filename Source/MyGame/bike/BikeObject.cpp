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
#include "Physics/Physics2.h"
#include "imgui.h"
#include <cfloat>

// ============================================================
// Transform / crack tuning vars
// ============================================================

// Crack visual pitch spring (cosmetic only)
static float crack_vis_pitch_impulse = 5;    // rad/s applied per unit crack_impulse
static float crack_vis_spring        = 340;  // spring constant
static float crack_vis_damp          = 10.f; // damping

// Gear shift cooldown
static float bike_gear_shift_cooldown = 3.f;

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
	tick_stamina(ci, dt);
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
void BikeObject::tick_transform(const ControlInput& ci, float dt)
{
	ASSERT(dt > 0.f);
	// Move: advance rear contact point, reconstruct entity origin
	glm::vec3 pos = get_owner()->get_ws_position();
	{
		glm::vec3 rear_contact = pos + bike_direction * BIKE_REAR_Z;
		rear_contact += bike_direction * speed * dt;
		pos = rear_contact - bike_direction * BIKE_REAR_Z;
	}

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
	if (front_hit && rear_hit) {
		const float rise    = front_res.hit_pos.y - rear_res.hit_pos.y;
		const float horiz   = front_fwd - BIKE_REAR_Z; // forward span along bike axis
		terrain_gradient    = atan2f(rise, horiz);
		terrain_forward_dir = glm::normalize(bike_direction * horiz + glm::vec3(0, rise, 0));
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

	// Orientation: terrain pitch + roll
	const glm::vec3 right       = glm::normalize(glm::cross(bike_direction, glm::vec3(0, 1, 0)));
	const glm::vec3 terrain_up  = glm::normalize(glm::cross(right, terrain_forward_dir));
	const glm::quat base_orient = glm::quat(glm::mat3(right, terrain_up, -terrain_forward_dir));
	const glm::quat orient      = glm::angleAxis(current_roll, terrain_forward_dir) * base_orient;

	// Cosmetic crack pitch spring — fires on crack_impulse, springs back to neutral
	if (crack_impulse > 0.f)
		crack_pitch_vel -= crack_vis_pitch_impulse * crack_impulse;
	crack_pitch_vel  += (-crack_vis_spring * crack_pitch_disp - crack_vis_damp * crack_pitch_vel) * dt;
	crack_pitch_disp += crack_pitch_vel * dt;
	const glm::quat crack_pitch_rot = glm::angleAxis(crack_pitch_disp, right);

	get_owner()->set_ws_position_rotation(pos, crack_pitch_rot * orient);

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
// Debug menu: Transform / Gears / Crack
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
	ImGui::SeparatorText("Crack Visual Pitch");
	{
		ImGui::DragFloat("crack_vis_pitch_impulse (deg)", &crack_vis_pitch_impulse, 0.001f, 0.f, glm::radians(20.f));
		ImGui::DragFloat("crack_vis_spring",              &crack_vis_spring,        1.f,    10.f, 400.f);
		ImGui::DragFloat("crack_vis_damp",                &crack_vis_damp,          0.1f,   0.f,  30.f);
		if (s_bike_debug)
			ImGui::Text("pitch_disp=%.3f rad  vel=%.2f", s_bike_debug->crack_pitch_disp, s_bike_debug->crack_pitch_vel);
	}
	ImGui::SeparatorText("Handlebar Visual");
	{
		ImGui::DragFloat("bar_scale_lo_steer",  &bar_scale_lo_steer,  0.1f, 0.5f, 12.f);
		ImGui::DragFloat("bar_scale_hi_steer",  &bar_scale_hi_steer,  0.1f, 0.1f,  6.f);
		ImGui::DragFloat("bar_visual_lean_min", &bar_visual_lean_min, 0.02f, 0.f,  1.f);
		ImGui::DragFloat("bar_lean_fade_lo",    &bar_lean_fade_lo,    0.01f, 0.f,  1.f);
		ImGui::DragFloat("bar_lean_fade_hi",    &bar_lean_fade_hi,    0.01f, 0.f,  1.f);
	}
}
ADD_TO_DEBUG_MENU(bike_transform_debug);
