#pragma once
#include "imgui.h"
#include <vector>

// Reusable PID tuning widgets — a gains editor and an interactive step-response
// visualizer. Not tied to any particular controller; any system with a
// BikeAI-style kp/ki/kd controller can share these instead of hand-rolling
// DragFloats + ad-hoc debug text. See MyGame/bike/BikeObject.cpp for a
// real usage (the heading steering PID).

// Plain PID gains.
struct PidGains {
	float kp = 1.f;
	float ki = 0.f;
	float kd = 0.f;
};

// Three DragFloats (kp/ki/kd) under `label`. Returns true if any value changed this frame.
bool imgui_pid_gains_editor(const char* label, PidGains& gains,
                            float kp_speed = 0.05f, float kp_max = 50.f,
                            float ki_speed = 0.02f, float ki_max = 20.f,
                            float kd_speed = 0.02f, float kd_max = 20.f);

// Persistent simulation state for imgui_pid_visualizer. Owned by the caller
// (e.g. a static local or a member on whatever debug window hosts the
// widget) and zero-initialized on first use — it must survive across frames,
// unlike ordinary immediate-mode ImGui calls.
struct PidVisualizerState {
	float value    = 0.f;   // simulated plant output (e.g. a heading angle, radians)
	float rate     = 0.f;   // d(value)/dt — real momentum, not snapped to target
	float integral = 0.f;   // accumulated error, feeds the I term
	float target   = 0.4f;  // setpoint — drag the track to change it
	std::vector<float> value_history;
	std::vector<float> target_history;
};

// Interactive PID tuning visualizer: drag anywhere on the track to move the
// target (yellow marker), and watch state.value (cyan marker) — a simulated
// second-order plant where the PID's output drives an acceleration, exactly
// like a real momentum-based controller (see BikeObject::tick_transform's
// heading_turn_rate) — converge to it in real time. Rise time, overshoot, and
// oscillation from the current kp/ki/kd are immediately visible instead of
// abstract numbers. A scrolling history plot underneath shows the same thing
// over time (yellow = target, cyan = value).
//
// Advances the simulation by ImGui::GetIO().DeltaTime every call, so call it
// once per frame while the panel housing it is open (it free-runs even if
// you don't touch the target, so you can watch it settle).
//
// accel_limit/rate_limit mirror a real actuator's physical caps (0 = unlimited).
// value_range sets the track/plot's -range..+range display bounds.
void imgui_pid_visualizer(const char* label, PidVisualizerState& state, const PidGains& gains,
                          float accel_limit = 0.f, float rate_limit = 0.f,
                          float value_range = 1.f, ImVec2 size = ImVec2(0.f, 120.f));
