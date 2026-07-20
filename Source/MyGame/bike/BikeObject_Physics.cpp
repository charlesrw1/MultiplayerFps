#include "BikeHeaders.h"
#include "BikeObject_Local.h"
#include "Framework/MathLib.h"
#include "Framework/Config.h"
#include "Physics/Physics2.h"
#include "imgui.h"

// Wind state accessed via g_wind (defined in BikeWind.cpp)

// ============================================================
// Physics-only constants (not shared with other BikeObject files)
// ============================================================
static constexpr float BIKE_CDA         = 0.3f;
static constexpr float BIKE_AIR_DENSITY = 1.225f;
static constexpr float BIKE_ROLL_RESIST = 0.004f;
static           float BIKE_MAX_BRAKE   = 1.8f;

static BikeObject* s_physics_debug = nullptr;  // set each tick for debug menu

// ------------------------------------------------------------
// BikeObject::tick_physics
//
// Applies propulsive, aerodynamic, rolling, braking and slope forces to
// advance `speed`. No pedal-stroke lurch, no traction/slip model, no
// crash detection — one straight power/drag/rolling/slope/brake balance.
// ------------------------------------------------------------
void BikeObject::tick_physics(ControlInput& ci, float dt)
{
	ASSERT(dt > 0.f);
	s_physics_debug = this;

	const float safe_speed = glm::max(speed, 0.3f);

	const float wind_along_bike  = get_wind_along_bike();
	const float apparent_speed   = speed - wind_along_bike;
	const float drag             = 0.5f * BIKE_AIR_DENSITY * (BIKE_CDA * draft_factor) * apparent_speed * glm::abs(apparent_speed);
	const float rolling          = (speed > 0.05f) ? (BIKE_ROLL_RESIST * BIKE_MASS * BIKE_GRAVITY) : 0.f;
	const float braking          = ci.brake_amount * BIKE_MASS * BIKE_MAX_BRAKE * surface_traction;
	const float slope_force      = BIKE_MASS * BIKE_GRAVITY * glm::sin(terrain_gradient);
	const float drive_force      = ci.power / safe_speed;

	const float net_force = drive_force - drag - rolling - braking - slope_force;
	speed = glm::max(0.f, speed + (net_force / BIKE_MASS) * dt);
}

// ============================================================
// Debug menu: Physics
// ============================================================

static void bike_physics_debug()
{
	ImGui::DragFloat("bike_brake", &BIKE_MAX_BRAKE, 0.05f, 0.f, 8.f);
	if (s_physics_debug)
		ImGui::DragFloat("surface_traction", &s_physics_debug->surface_traction, 0.01f, 0.f, 1.f);
}
ADD_TO_DEBUG_MENU(bike_physics_debug);
