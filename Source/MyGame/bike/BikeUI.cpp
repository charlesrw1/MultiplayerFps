#include "BikeHeaders.h"
#include "UI/Gui.h"
#include "Render/Texture.h"
#include "Framework/Util.h"

// ============================================================
// Power-zone meter HUD (horizontal, lower-center)
// ============================================================

void BikePlayer::draw_power_meter(float current_watts, int power_idx, bool coasting, bool speed_hold, float speed_hold_watts, float actual_watts, float power_ceiling)
{
	struct ZoneColor {
		int   w_min;
		float r_lit, g_lit, b_lit;
		float r_dim, g_dim, b_dim;
	};
	static const ZoneColor zone_colors[] = {
		{   0,  0.16f, 0.31f, 0.86f,  0.04f, 0.09f, 0.25f },  // blue
		{ 150,  0.16f, 0.73f, 0.24f,  0.04f, 0.20f, 0.07f },  // green
		{ 250,  0.88f, 0.80f, 0.14f,  0.25f, 0.22f, 0.04f },  // yellow
		{ 350,  0.88f, 0.43f, 0.08f,  0.25f, 0.12f, 0.02f },  // orange
		{ 500,  0.80f, 0.12f, 0.12f,  0.22f, 0.03f, 0.03f },  // red
	};
	constexpr int NUM_ZONES = 5;

	auto zone_for_watts = [&](int w) -> const ZoneColor& {
		int z = 0;
		for (int i = 1; i < NUM_ZONES; ++i)
			if (w >= zone_colors[i].w_min) z = i;
		return zone_colors[z];
	};

	const lRect screen = Gui::get_screen_size();
	const int sw = screen.w;
	const int sh = screen.h;

	constexpr int SEG_W = 16;   // width of each power segment
	constexpr int SEG_H = 14;   // height of each power segment
	constexpr int GAP   = 2;

	const int bar_total_w = BIKE_NUM_POWER_LEVELS * SEG_W + (BIKE_NUM_POWER_LEVELS - 1) * GAP;
	const int bar_x       = (sw - bar_total_w) / 2;
	const int bar_y       = sh / 4;  // quarter down — between top and centre

	// Draw left (low power) to right (high power)


	Gui::set_color(0, 0, 0, 0.5);
	Gui::rectangle(bar_x - 2, bar_y - 2, bar_total_w + 4, SEG_H + 4);
	for (int i = 0; i < BIKE_NUM_POWER_LEVELS; ++i) {
		const int x    = bar_x + i * (SEG_W + GAP);
		const bool lit = !coasting && i <= power_idx;
		const ZoneColor& zc = zone_for_watts(BIKE_POWER_LEVELS[i]);
		
		if(lit&&i==power_idx)
			Gui::set_color(zc.r_lit*1.2, zc.g_lit*1.2, zc.b_lit*1.2, 0.92f);
		else if (lit)
			Gui::set_color(zc.r_lit, zc.g_lit, zc.b_lit, 0.92f);
		else
			Gui::set_color(zc.r_dim, zc.g_dim, zc.b_dim, 0.75f);
		Gui::rectangle(x, bar_y, SEG_W, SEG_H);

	}

	// Speed hold marker: white tick above the matching segment
	if (speed_hold) {
		int sh_idx = 0;
		for (int i = 0; i < BIKE_NUM_POWER_LEVELS; ++i)
			if (BIKE_POWER_LEVELS[i] <= (int)speed_hold_watts) sh_idx = i;
		const int marker_x = bar_x + sh_idx * (SEG_W + GAP);
		Gui::set_color(1.f, 1.f, 1.f, 0.9f);
		Gui::rectangle(marker_x, bar_y - 4, SEG_W, 3);
	}

	// Power ceiling line: vertical line at the interpolated watt position
	const bool is_clamped = !coasting && (power_ceiling < current_watts - 1.f);
	if (is_clamped) {
		auto watts_to_x = [&](float w) -> int {
			w = glm::clamp(w, (float)BIKE_POWER_LEVELS[0], (float)BIKE_POWER_LEVELS[BIKE_NUM_POWER_LEVELS - 1]);
			int lo = 0;
			for (int i = 0; i < BIKE_NUM_POWER_LEVELS - 1; i++) {
				if (w >= BIKE_POWER_LEVELS[i] && w <= BIKE_POWER_LEVELS[i + 1]) { lo = i; break; }
			}
			const int hi = glm::min(lo + 1, BIKE_NUM_POWER_LEVELS - 1);
			const float lo_x = (float)(bar_x + lo * (SEG_W + GAP));
			const float hi_x = (float)(bar_x + hi * (SEG_W + GAP));
			const float t = (BIKE_POWER_LEVELS[hi] > BIKE_POWER_LEVELS[lo])
			              ? (w - BIKE_POWER_LEVELS[lo]) / (float)(BIKE_POWER_LEVELS[hi] - BIKE_POWER_LEVELS[lo])
			              : 0.f;
			return (int)glm::mix(lo_x, hi_x, t);
		};

		const int ceil_x = watts_to_x(power_ceiling);
		Gui::set_color(0.f, 0.f, 0.f, 0.6f);
		Gui::line(ceil_x + 1, bar_y - 1, ceil_x + 1, bar_y + SEG_H + 1, 2);
		Gui::set_color(1.f, 0.55f, 0.05f, 1.f);
		Gui::line(ceil_x, bar_y - 1, ceil_x, bar_y + SEG_H + 1, 2);
	}

	// Watt readout centred above the bar
	if (is_clamped) {
		const auto actual_str    = string_format("%.0fW", actual_watts);
		const auto requested_str = string_format("/%.0fW", current_watts);
		const lRect actual_sz    = Gui::measure_text(actual_str);
		const lRect req_sz       = Gui::measure_text(requested_str);
		const int   combined_w   = actual_sz.w + req_sz.w;
		const int   label_x      = (sw - combined_w) / 2;
		const int   label_y      = bar_y - 12 - 1;
		Gui::set_color(0.f, 0.f, 0.f, 1.f);
		Gui::print(actual_str, label_x + 1, label_y + 1);
		Gui::set_color(1.f, 0.55f, 0.05f, 1.f);
		Gui::print(actual_str, label_x, label_y);
		Gui::set_color(0.5f, 0.5f, 0.5f, 0.7f);
		Gui::print(requested_str, label_x + actual_sz.w, label_y);
	} else {
		const auto  watt_str = string_format("%.0fW", current_watts);
		const lRect watt_sz  = Gui::measure_text(watt_str);
		const int   label_x  = (sw - watt_sz.w) / 2;
		const int   label_y  = bar_y - 12 - 1;
		Gui::set_color(0.f, 0.f, 0.f, 1.f);
		Gui::print(watt_str, label_x + 1, label_y + 1);
		Gui::set_color(1.f, 1.f, 1.f, 1.f);
		Gui::print(watt_str, label_x, label_y);
	}
}
