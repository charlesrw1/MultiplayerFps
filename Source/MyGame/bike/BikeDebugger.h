#pragma once
#include "User_Camera.h"
#include "Framework/Config.h"
#include "Game/Entity.h"
#include <vector>

class BikeObject;

// Free-fly debug camera for the AI-only bike race: RMB+WASD to fly, LMB-click
// a rider to snap into an orbit cam around them (with a live stats/gizmo
// panel), plus the same time-scale controls as fpsDebugCamera.
class BikeDebugger {
public:
	void init();                                        // spawn the debug camera entity
	void update(const std::vector<BikeObject*>& riders); // drive camera + picking, called every frame
	void on_imgui();

	// Clears the current selection/orbit (if any). Call before destroying any
	// rider that might be selected -- selected is a raw BikeObject* with no
	// lifetime tracking of its own, so leaving it pointed at a destroyed rider
	// crashes the next on_imgui()/update() call.
	void deselect() { selected = nullptr; orbiting = false; behind_cam_initialized = false; }
	bool has_selection(const BikeObject* rider) const { return selected == rider; }

private:
	BikeObject* pick_rider_under_cursor(const std::vector<BikeObject*>& riders) const;

	User_Camera fly_cam;
	EntityPtr   debug_cam_ent;
	BikeObject* selected = nullptr;
	bool        orbiting = false;
	bool        initialized_fly_cam = false;
	bool        draw_rider_state_text = false;  // per-rider paceline state + timer, drawn above their head
	bool        draw_avoidance_box      = false;  // selected rider only — drop-dead box + avoid vectors, see draw_rider_avoidance_box
	bool        draw_avoidance_soft_box = false;  // selected rider only — also draws the outer soft-reaction boundary, different color

	// Behind camera (3rd person chase cam) — alternative to MMB-orbit while a
	// rider is selected, same offset math as apply_debug_follow_camera() (see
	// BikeApplication_Debug.cpp) but smoothed like BikePlayer::update_camera.
	bool      behind_camera_enabled  = false;
	float     behind_cam_dist_m      = 3.4f;   // distance behind rider (m)
	float     behind_cam_height_m    = 1.55f;  // pivot height above rider origin (m)
	float     behind_cam_pitch_deg   = -20.f;  // camera pitch, negative = looking down at the rider
	// damp_dt_independent's "smoothing" factor, not seconds — closer to 1 = laggier/smoother,
	// closer to 0 = snappier. Position and rotation lag independently (see update()).
	float     behind_cam_pos_smooth_time_s = 0.02f;
	float     behind_cam_rot_smooth_time_s = 0.05f;
	glm::vec3 behind_cam_pos{};
	glm::quat behind_cam_rot{ 1.f, 0.f, 0.f, 0.f };
	bool      behind_cam_initialized = false;
};
