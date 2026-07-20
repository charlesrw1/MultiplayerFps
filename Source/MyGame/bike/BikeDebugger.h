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

private:
	BikeObject* pick_rider_under_cursor(const std::vector<BikeObject*>& riders) const;

	User_Camera fly_cam;
	EntityPtr   debug_cam_ent;
	BikeObject* selected = nullptr;
	bool        orbiting = false;
	bool        initialized_fly_cam = false;
	bool        draw_rider_state_text = false;  // per-rider paceline state + timer, drawn above their head
};
