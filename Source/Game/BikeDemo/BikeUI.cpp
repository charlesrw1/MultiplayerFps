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

// ============================================================
// Stamina HUD (bottom-left)
// ============================================================

void BikePlayer::draw_stamina_ui(const StaminaState& s, const RiderStats& r)
{
	const lRect screen = Gui::get_screen_size();
	const int sw = screen.w;
	const int sh = screen.h;

	// ---- Vignette: pulsating red edge overlay ----
	const float HR_min_to_use = 130.f;
	const float hr_frac = glm::clamp((s.hr_current - HR_min_to_use) / (r.hr_max - HR_min_to_use), 0.f, 1.f);
	const float vignette_base = glm::clamp((hr_frac - 0.55f) / 0.45f, 0.f, 1.f);
	const float pulse = glm::max(0.f, glm::sin(s.hr_pulse_phase));
	const float vignette_alpha = vignette_base * (0.55f + 0.45f * pulse);

	if (vignette_alpha > 0.01f) {
		static const int VIGNETTE_LAYERS = 6;
		for (int i = 0; i < VIGNETTE_LAYERS; i++) {
			const float t       = (float)(VIGNETTE_LAYERS - 1 - i) / (float)(VIGNETTE_LAYERS - 1);
			const float layer_a = vignette_alpha * t * t * 0.55f;
			const int   inset   = i * 18;
			const int   x0      = inset;
			const int   y0      = inset;
			const int   w_      = sw - inset * 2;
			const int   h_      = sh - inset * 2;
			const int   thick   = 18;
			Gui::set_color(0.85f, 0.05f, 0.05f, layer_a);
			Gui::rectangle(x0,              y0,              w_,    thick);
			Gui::rectangle(x0,              y0 + h_ - thick, w_,    thick);
			Gui::rectangle(x0,              y0 + thick,      thick, h_ - thick * 2);
			Gui::rectangle(x0 + w_ - thick, y0 + thick,      thick, h_ - thick * 2);
		}
	}

	// ---- Heat vignette: warm orange/yellow tint at screen edges ----
	if (s.heat_stress > 0.05f) {
		static const int HEAT_LAYERS = 5;
		for (int i = 0; i < HEAT_LAYERS; i++) {
			const float t       = (float)(HEAT_LAYERS - 1 - i) / (float)(HEAT_LAYERS - 1);
			const float layer_a = s.heat_stress * t * t * 0.30f;
			const int   inset   = i * 22;
			const int   x0      = inset;
			const int   y0      = inset;
			const int   w_      = sw - inset * 2;
			const int   h_      = sh - inset * 2;
			const int   thick   = 22;
			Gui::set_color(0.95f, 0.55f, 0.05f, layer_a);
			Gui::rectangle(x0,              y0,              w_,    thick);
			Gui::rectangle(x0,              y0 + h_ - thick, w_,    thick);
			Gui::rectangle(x0,              y0 + thick,      thick, h_ - thick * 2);
			Gui::rectangle(x0 + w_ - thick, y0 + thick,      thick, h_ - thick * 2);
		}
	}

	// ---- HR panel (bottom-left) ----
	constexpr int HUD_X   = 12;
	constexpr int HUD_Y_B = 20;
	const int hud_y_base  = sh - HUD_Y_B;

	static const float hr_zone_r[] = { 0.16f, 0.16f, 0.88f, 0.88f, 0.80f };
	static const float hr_zone_g[] = { 0.31f, 0.73f, 0.80f, 0.43f, 0.12f };
	static const float hr_zone_b[] = { 0.86f, 0.24f, 0.14f, 0.08f, 0.12f };

	const int zone  = s.hr_zone(r.hr_rest, r.hr_max);
	const float hr_r = hr_zone_r[zone];
	const float hr_g = hr_zone_g[zone];
	const float hr_b = hr_zone_b[zone];

	// Heart icon: pulsates with heartbeat
	constexpr int ICON_BASE = 22;
	const float icon_scale = 1.f + 0.25f * glm::max(0.f, glm::sin(s.hr_pulse_phase));
	const int   icon_size  = (int)(ICON_BASE * icon_scale);
	const int   icon_x     = HUD_X + (ICON_BASE - icon_size) / 2;
	const int   icon_y     = hud_y_base - ICON_BASE - (icon_size - ICON_BASE) / 2;

	if (heart_icon_tex) {
		Gui::set_color(hr_r, hr_g, hr_b, 1.f);
		Gui::image(heart_icon_tex, icon_x, icon_y, icon_size, icon_size);
	} else {
		Gui::set_color(hr_r, hr_g, hr_b, 1.f);
		Gui::circle(HUD_X + ICON_BASE / 2, hud_y_base - ICON_BASE / 2, (int)(icon_size / 2), 16);
	}

	// BPM readout
	const int  bpm_x   = HUD_X + ICON_BASE + 5;
	const int  bpm_y   = hud_y_base - ICON_BASE;
	const auto bpm_str = string_format("%d bpm", (int)s.hr_current);
	Gui::set_color(0.f, 0.f, 0.f, 0.85f);
	Gui::print(bpm_str, bpm_x + 1, bpm_y + 1);
	Gui::set_color(hr_r, hr_g, hr_b, 1.f);
	Gui::print(bpm_str, bpm_x, bpm_y);

	// ---- W' indicator (3 dots) ----
	constexpr int WPRIME_Y_OFFSET = 28;
	const int wprime_y = hud_y_base - ICON_BASE - WPRIME_Y_OFFSET;
	const int bars     = s.w_prime_bars(r.w_prime_max);
	constexpr int DOT_R   = 5;
	constexpr int DOT_GAP = 14;

	for (int i = 0; i < 3; i++) {
		const bool filled = i < bars;
		const int  dot_x  = HUD_X + DOT_R + i * DOT_GAP;
		if (filled) {
			const float dep = 1.f - (float)bars / 3.f;
			Gui::set_color(1.f, glm::mix(0.8f, 0.3f, dep), 0.1f, 0.95f);
		} else {
			Gui::set_color(0.25f, 0.12f, 0.04f, 0.6f);
		}
		Gui::circle(dot_x, wprime_y, DOT_R, 12);
	}

	// ---- Heat indicator (above W' dots, only when significant) ----
	constexpr int LEGS_Y_OFFSET = 18;
	int legs_y = wprime_y - LEGS_Y_OFFSET;

	if (s.heat_stress > 0.05f) {
		// 4 small heat bars (like signal bars) colored orange→red
		constexpr int BAR_W = 4;
		constexpr int BAR_GAP = 2;
		constexpr int BAR_MAX_H = 12;
		constexpr int NUM_HEAT_BARS = 4;
		const int heat_x = HUD_X;
		const int heat_y = legs_y - 2;
		const int filled_bars = (int)(s.heat_stress * NUM_HEAT_BARS + 0.5f);

		for (int i = 0; i < NUM_HEAT_BARS; i++) {
			const int bar_h = 3 + (i * (BAR_MAX_H - 3)) / (NUM_HEAT_BARS - 1);
			const int bx    = heat_x + i * (BAR_W + BAR_GAP);
			const int by    = heat_y - bar_h;
			const bool lit  = i < filled_bars;
			if (lit)
				Gui::set_color(glm::mix(0.95f, 0.85f, (float)i / NUM_HEAT_BARS),
				               glm::mix(0.65f, 0.10f, (float)i / NUM_HEAT_BARS),
				               0.05f, 0.95f);
			else
				Gui::set_color(0.25f, 0.15f, 0.05f, 0.5f);
			Gui::rectangle(bx, by, BAR_W, bar_h);
		}

		// "HOT" label to the right of bars
		const int label_x = heat_x + NUM_HEAT_BARS * (BAR_W + BAR_GAP) + 3;
		const int label_y_pos = heat_y - 9;
		Gui::set_color(0.f, 0.f, 0.f, 0.7f);
		Gui::print("HOT", label_x + 1, label_y_pos + 1);
		Gui::set_color(0.95f, glm::mix(0.65f, 0.10f, s.heat_stress), 0.05f, 0.9f);
		Gui::print("HOT", label_x, label_y_pos);

		legs_y -= (BAR_MAX_H + 6);  // push legs descriptor up to make room
	}

	// ---- Legs descriptor ----
	const char* desc   = s.legs_descriptor();
	Gui::set_color(0.f, 0.f, 0.f, 0.8f);
	Gui::print(desc, HUD_X + 1, legs_y + 1);
	const float g_frac = s.glycogen;
	Gui::set_color(glm::mix(0.9f, 0.3f, 1.f - g_frac),
	               glm::mix(0.5f, 0.9f, g_frac),
	               0.2f, 1.f);
	Gui::print(desc, HUD_X, legs_y);
}
