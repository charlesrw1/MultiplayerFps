#pragma once
// Shared constants and declarations for BikeObject split translation units.
// inline constexpr avoids ODR violations in unity builds.
inline constexpr float BIKE_MASS        = 83.f;
inline constexpr float BIKE_WHEEL_CIRC  = 2.1f;
inline constexpr float BIKE_GRAVITY     = 9.81f;
inline constexpr float BIKE_REAR_Z      = -0.449f;
inline constexpr float BIKE_FRONT_Z     =  0.5394f;
inline constexpr float BIKE_WHEELBASE   = BIKE_FRONT_Z - BIKE_REAR_Z;
// ci.lateral_shift no longer translates a rail lateral_pos — it bends
// bike_direction away from the track tangent (see BikeObject::tick_transform).
// Tunable in the Transform debug menu (BikeObject.cpp).
extern float bike_heading_max_offset_deg;  // max heading deviation from track tangent, at |ci.lateral_shift|=1
extern float bike_heading_turn_rate_dps;   // max rate bike_direction can turn to close that deviation

// Steering/visual tuning vars — defined in BikeObject_Steer.cpp.
extern float steer_max_deg;
extern float steer_max_deg_hi;
extern float steer_ref_speed;
extern float steer_speed_power;
extern float lean_max_deg;
extern float bar_scale_lo_steer;
extern float bar_scale_hi_steer;
extern float bar_visual_lean_min;
extern float bar_lean_fade_lo;
extern float bar_lean_fade_hi;

// Defined in BikeObject_Steer.cpp — used by tick_transform in BikeObject.cpp.
float compute_max_steer_rad(float speed);
