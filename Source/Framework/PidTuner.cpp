#include "PidTuner.h"

bool imgui_pid_gains_editor(const char* label, PidGains& gains,
                            float kp_speed, float kp_max,
                            float ki_speed, float ki_max,
                            float kd_speed, float kd_max) {
	ImGui::PushID(label);
	bool changed = false;
	if (label && label[0])
		ImGui::TextUnformatted(label);
	changed |= ImGui::DragFloat("kp", &gains.kp, kp_speed, 0.f, kp_max, "%.3f");
	changed |= ImGui::DragFloat("ki", &gains.ki, ki_speed, 0.f, ki_max, "%.3f");
	changed |= ImGui::DragFloat("kd", &gains.kd, kd_speed, 0.f, kd_max, "%.3f");
	ImGui::PopID();
	return changed;
}

void imgui_pid_visualizer(const char* label, PidVisualizerState& state, const PidGains& gains,
                          float accel_limit, float rate_limit,
                          float value_range, ImVec2 size) {
	ImGui::PushID(label);
	if (label && label[0])
		ImGui::TextUnformatted(label);

	if (size.x <= 0.f)
		size.x = ImGui::CalcItemWidth();
	if (size.y <= 0.f)
		size.y = 120.f;
	value_range = ImMax(value_range, 0.001f);

	// ---- Advance the simulated plant: PID output -> acceleration -> rate -> value,
	// same structure as a real momentum-based controller (see PidTuner.h). Guard
	// against a huge dt (window was paused/dragged) blowing up the integrator. ----
	const float dt = ImGui::GetIO().DeltaTime;
	if (dt > 0.f && dt < 0.25f) {
		const float error = state.target - state.value;
		state.integral = ImClamp(state.integral + error * dt, -5.f, 5.f);
		const float derivative = -state.rate;  // d(error)/dt when target is momentarily constant, no finite-diff kick
		float accel = gains.kp * error + gains.ki * state.integral + gains.kd * derivative;
		if (accel_limit > 0.f)
			accel = ImClamp(accel, -accel_limit, accel_limit);
		state.rate += accel * dt;
		if (rate_limit > 0.f)
			state.rate = ImClamp(state.rate, -rate_limit, rate_limit);
		state.value += state.rate * dt;

		state.value_history.push_back(state.value);
		state.target_history.push_back(state.target);
		constexpr size_t MAX_HISTORY = 300;
		if (state.value_history.size() > MAX_HISTORY) {
			state.value_history.erase(state.value_history.begin());
			state.target_history.erase(state.target_history.begin());
		}
	}

	ImDrawList* dl = ImGui::GetWindowDrawList();

	// ---- Track: click/drag anywhere to move the target ----
	ImGui::InvisibleButton("##pid_track", ImVec2(size.x, 36.f));
	const ImVec2 track_min = ImGui::GetItemRectMin();
	const ImVec2 track_max = ImGui::GetItemRectMax();
	dl->AddRectFilled(track_min, track_max, IM_COL32(40, 40, 40, 255), 4.f);
	const float mid_y = (track_min.y + track_max.y) * 0.5f;
	dl->AddLine(ImVec2(track_min.x, mid_y), ImVec2(track_max.x, mid_y), IM_COL32(90, 90, 90, 255));

	auto value_to_x = [&](float v) {
		const float t = ImClamp((v + value_range) / (2.f * value_range), 0.f, 1.f);
		return track_min.x + t * (track_max.x - track_min.x);
	};

	if (ImGui::IsItemActive()) {
		const float mx = ImGui::GetIO().MousePos.x;
		const float t  = ImClamp((mx - track_min.x) / (track_max.x - track_min.x), 0.f, 1.f);
		state.target = -value_range + t * 2.f * value_range;
	}

	// Target marker: yellow triangle.
	{
		const float x = value_to_x(state.target);
		dl->AddTriangleFilled(ImVec2(x, mid_y - 10.f), ImVec2(x - 8.f, mid_y + 8.f), ImVec2(x + 8.f, mid_y + 8.f),
		                       IM_COL32(255, 210, 60, 255));
	}
	// Value marker: cyan circle — where the simulated plant actually is right now.
	{
		const float x = value_to_x(state.value);
		dl->AddCircleFilled(ImVec2(x, mid_y), 7.f, IM_COL32(60, 220, 255, 255));
	}

	ImGui::TextDisabled("Drag track to move target (yellow). Cyan = simulated response.");

	// ---- Scrolling history plot: same colors, value over time ----
	if (state.value_history.size() > 1) {
		const ImVec2 plot_min = ImGui::GetCursorScreenPos();
		const ImVec2 plot_max = plot_min + ImVec2(size.x, size.y);
		dl->AddRectFilled(plot_min, plot_max, IM_COL32(25, 25, 25, 255), 4.f);

		float lo = -value_range, hi = value_range;
		for (float v : state.value_history)  { lo = ImMin(lo, v); hi = ImMax(hi, v); }
		for (float v : state.target_history) { lo = ImMin(lo, v); hi = ImMax(hi, v); }
		if (hi - lo < 0.01f) hi = lo + 0.01f;

		const size_t n = state.value_history.size();
		auto to_screen = [&](size_t i, float v) {
			const float tx = (float)i / (float)(n - 1);
			const float ty = 1.f - (v - lo) / (hi - lo);
			return ImVec2(plot_min.x + tx * size.x, plot_min.y + ty * size.y);
		};
		for (size_t i = 1; i < n; ++i) {
			dl->AddLine(to_screen(i - 1, state.target_history[i - 1]), to_screen(i, state.target_history[i]),
			            IM_COL32(255, 210, 60, 180), 1.5f);
			dl->AddLine(to_screen(i - 1, state.value_history[i - 1]), to_screen(i, state.value_history[i]),
			            IM_COL32(60, 220, 255, 255), 2.f);
		}
		ImGui::Dummy(ImVec2(size.x, size.y));
		ImGui::TextDisabled("kp=%.2f ki=%.2f kd=%.2f", gains.kp, gains.ki, gains.kd);
	}

	ImGui::PopID();
}
