#include "BikeCourse.h"
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <cmath>

// Bike dynamics constants (matched to BikeObject physics).
static float MU_LATERAL = 0.55f;  // road tyre lateral friction
static float G_ACC = 9.81f;
static float BIKE_DECEL = 7.0f;   // hard braking (m/s²)
static float BIKE_ACCEL = 3.0f;   // conservative exit acceleration (m/s²)
static float V_MAX_FLAT = 20.f;   // ~72 km/h, fast road cycling absolute cap
static float KAPPA_THRESH = 0.008f; // radius < 125 m → corner

static void rl_smooth_scalar(std::vector<float>& v, bool loop, int r)
{
	const int n = (int)v.size();
	std::vector<float> out(n);
	for (int i = 0; i < n; ++i) {
		float sum = 0.f;
		int   cnt = 0;
		for (int j = -r; j <= r; ++j) {
			int k = i + j;
			if      (loop)            k = (k % n + n) % n;
			else if (k < 0 || k >= n) continue;
			sum += v[k];
			++cnt;
		}
		out[i] = cnt > 0 ? sum / (float)cnt : v[i];
	}
	v = out;
}
#include "Framework/Config.h"

#include "imgui.h"
void raceline_debug()
{
	ImGui::DragFloat("mu_lat", &MU_LATERAL, 0.01f);
	ImGui::DragFloat("BIKE_ACCEL", &BIKE_ACCEL, 0.01f);
	ImGui::DragFloat("V_MAX_FLAT", &V_MAX_FLAT, 0.1f);
	ImGui::DragFloat("KAPPA_THRESH", &KAPPA_THRESH, 0.0001f);
}
ADD_TO_DEBUG_MENU(raceline_debug);

void BikeCourse::compute_racing_line(std::vector<BikeWaypoint>& wps, bool loop,
                                     float strength, int num_iters)
{
	const int n = (int)wps.size();
	if (n < 3) return;

	const float total_len = loop && n > 1
		? wps.back().dist_from_start + glm::distance(wps.back().position, wps[0].position)
		: wps.back().dist_from_start;

	// ----------------------------------------------------------------
	// Phase 1 — Signed curvature at every centerline waypoint.
	// ----------------------------------------------------------------
	std::vector<float> kappa(n, 0.f);
	for (int i = 0; i < n; ++i) {
		const int im = loop ? (i - 1 + n) % n : std::max(0, i - 1);
		const int ip = loop ? (i + 1) % n     : std::min(n - 1, i + 1);
		if (im == i || ip == i) continue;

		const glm::vec3 a  = wps[im].position;
		const glm::vec3 b  = wps[i].position;
		const glm::vec3 c  = wps[ip].position;
		const glm::vec3 ba = a - b, bc = c - b;
		const float la = glm::length(ba), lc = glm::length(bc), ac = glm::distance(a, c);
		if (la < 1e-4f || lc < 1e-4f || ac < 1e-4f) continue;

		const glm::vec3 cr      = glm::cross(ba / la, bc / lc);
		const float     kappa_u = glm::length(cr) / (0.5f * ac);
		kappa[i] = (cr.y > 0.f ? 1.f : -1.f) * kappa_u;
	}

	// ----------------------------------------------------------------
	// Phase 2 — Arc-length table + gradient-aware centerline speed profile.
	//
	// v_lat[i] = max cornering speed on a slope:
	//   v = sqrt(mu * g * cos(gradient) / |kappa|)
	// cos(gradient) scales available lateral grip with road normal force.
	//
	// Forward/backward kinematic passes propagate limits along the course:
	//   uphill   → harder to accelerate, easier to brake
	//   downhill → easier to accelerate, harder to brake (longer distances)
	//
	// The resulting speed[] profile is queried in Phase 3 so each corner's
	// braking/accel zone is sized from the real approach/exit speed rather
	// than the naïve v_apex²/(2a) formula.
	// ----------------------------------------------------------------
	std::vector<float> ds(n, 0.f);
	for (int i = 0; i < n; ++i) {
		const int nxt = loop ? (i + 1) % n : std::min(i + 1, n - 1);
		ds[i] = glm::distance(wps[i].position, wps[nxt].position);
	}

	// kappa_floor keeps lateral speed capped at V_MAX_FLAT on long straights.
	const float kappa_floor = MU_LATERAL * G_ACC / (V_MAX_FLAT * V_MAX_FLAT);

	std::vector<float> speed(n);
	for (int i = 0; i < n; ++i) {
		const float kabs   = std::max(kappa_floor, std::abs(kappa[i]));
		const float mu_eff = MU_LATERAL * std::cos(std::abs(wps[i].gradient));
		speed[i] = std::sqrt(mu_eff * G_ACC / kabs);
	}

	// Sweep 2n elements per pass so loop wrap-around constraints converge in one outer iteration.
	const int sw_s = loop ? 0     : 1;
	const int sw_e = loop ? 2 * n : n - 1;  // exclusive upper bound

	for (int pass = 0; pass < (loop ? 2 : 1); ++pass) {
		for (int i = sw_s; i < sw_e; ++i) {
			const int cur  = loop ? i % n : i;
			const int prev = loop ? (i - 1 + n) % n : cur - 1;
			const float a_eff = std::max(0.3f, BIKE_ACCEL - G_ACC * std::sin(wps[cur].gradient));
			speed[cur] = std::min(speed[cur],
				std::sqrt(speed[prev] * speed[prev] + 2.f * a_eff * ds[prev]));
		}
		for (int i = sw_e - 1; i >= sw_s; --i) {
			const int cur = loop ? i % n : i;
			const int nxt = loop ? (i + 1) % n : cur + 1;
			const float d_eff = std::max(1.f, BIKE_DECEL + G_ACC * std::sin(wps[cur].gradient));
			speed[cur] = std::min(speed[cur],
				std::sqrt(speed[nxt] * speed[nxt] + 2.f * d_eff * ds[cur]));
		}
	}

	// ----------------------------------------------------------------
	// Phase 3 — Corner detection with gradient-aware zone sizing.
	//
	// Apex speed: v = sqrt(mu * g * cos(grad) * R)
	//   On a hill the tyre's normal force is m*g*cos(grad), so available
	//   lateral grip — and maximum corner speed — scales with cos(grad).
	//
	// Braking zone: (v_approach² - v_apex²) / (2 * eff_decel)
	//   where v_approach is read from the speed profile ~bk_dist before
	//   the apex, and eff_decel = BIKE_DECEL + g*sin(grad) (uphill shortens,
	//   downhill extends the braking distance).
	//   This formula is typically 2–4× larger than the old v_apex²/(2a),
	//   so the OIO entry zone stretches back to where braking actually starts.
	// ----------------------------------------------------------------

	struct Corner {
		int   apex_idx;
		float apex_kappa;
		float braking_dist;
		float accel_dist;
	};
	std::vector<Corner> corners;

	{
		auto scan_back = [&](int from, float dist) -> int {
			int cur = from; float rem = dist; int guard = n;
			while (rem > 0.f && --guard >= 0) {
				const int prev = loop ? (cur - 1 + n) % n : std::max(0, cur - 1);
				if (prev == cur) break;
				rem -= ds[prev]; cur = prev;
			}
			return cur;
		};
		auto scan_fwd = [&](int from, float dist) -> int {
			int cur = from; float rem = dist; int guard = n;
			while (rem > 0.f && --guard >= 0) {
				const int nxt = loop ? (cur + 1) % n : std::min(n - 1, cur + 1);
				if (nxt == cur) break;
				rem -= ds[cur]; cur = nxt;
			}
			return cur;
		};

		int i = 0;
		while (i < n) {
			if (std::abs(kappa[i]) < KAPPA_THRESH) { ++i; continue; }

			int   apex_idx = i;
			float peak_abs = std::abs(kappa[i]);
			while (i < n && std::abs(kappa[i]) >= KAPPA_THRESH) {
				if (std::abs(kappa[i]) > peak_abs) {
					peak_abs = std::abs(kappa[i]);
					apex_idx = i;
				}
				++i;
			}
			if (peak_abs < 1e-6f) continue;

			const float grad_a = wps[apex_idx].gradient;
			const float mu_eff = MU_LATERAL * std::cos(grad_a);
			const float radius = 1.f / peak_abs;
			const float v_apex = std::sqrt(mu_eff * G_ACC * radius);

			const float eff_d = std::max(1.f,  BIKE_DECEL + G_ACC * std::sin(grad_a));
			const float eff_a = std::max(0.3f, BIKE_ACCEL - G_ACC * std::sin(grad_a));

			// Rough initial zones for approach/exit lookup in the speed profile.
			const float bk_rough = glm::clamp(v_apex * v_apex / (2.f * eff_d), 5.f, 200.f);
			const float ac_rough = glm::clamp(v_apex * v_apex / (2.f * eff_a), 5.f, 250.f);

			const float v_app = speed[scan_back(apex_idx, bk_rough)];
			const float v_ex  = speed[scan_fwd (apex_idx, ac_rough)];
			const float v_c2  = v_apex * v_apex;

			const float bk_dist = glm::clamp(std::max(0.f, v_app*v_app - v_c2) / (2.f * eff_d), 5.f, 200.f);
			const float ac_dist = glm::clamp(std::max(0.f, v_ex *v_ex  - v_c2) / (2.f * eff_a), 5.f, 250.f);

			corners.push_back({ apex_idx, kappa[apex_idx], bk_dist, ac_dist });
		}

		// Seam-crossing corner merge for loops: keep the sharper apex.
		if (loop && corners.size() >= 2) {
			const Corner& first = corners.front();
			const Corner& last  = corners.back();
			const float d_first = wps[first.apex_idx].dist_from_start;
			const float d_last  = wps[last.apex_idx].dist_from_start;
			const bool same_sign = (first.apex_kappa >= 0.f) == (last.apex_kappa >= 0.f);
			if (same_sign && d_last + last.accel_dist >= total_len && d_first <= first.braking_dist) {
				if (std::abs(last.apex_kappa) >= std::abs(first.apex_kappa))
					corners.erase(corners.begin());
				else
					corners.pop_back();
			}
		}
	}

	// ----------------------------------------------------------------
	// Phase 4 — Outside-inside-outside lateral target offsets.
	//   sign_k =  1 (left turn):  outside = +hw, inside = −hw
	//   sign_k = −1 (right turn): outside = −hw, inside = +hw
	// Dominant-corner rule: largest |offset| wins at each waypoint so
	// consecutive corners naturally produce a compromise line.
	// ----------------------------------------------------------------
	std::vector<float> target(n, 0.f);

	for (const Corner& c : corners) {
		const float d_apex = wps[c.apex_idx].dist_from_start;
		const float sign_k = (c.apex_kappa >= 0.f) ? 1.f : -1.f;

		for (int i = 0; i < n; ++i) {
			const float hw = wps[i].road_half_width * strength;

			float d_rel = wps[i].dist_from_start - d_apex;
			if (loop && total_len > 0.f) {
				while (d_rel >  0.5f * total_len) d_rel -= total_len;
				while (d_rel < -0.5f * total_len) d_rel += total_len;
			}

			const float out_off =  sign_k * hw;
			const float in_off  = -sign_k * hw;
			float t_offset = 0.f;

			if (d_rel >= -c.braking_dist && d_rel <= 0.f) {
				const float t = (d_rel + c.braking_dist) / c.braking_dist;
				t_offset = glm::mix(out_off, in_off, glm::smoothstep(0.f, 1.f, t));
			} else if (d_rel > 0.f && d_rel <= c.accel_dist) {
				const float t = d_rel / c.accel_dist;
				t_offset = glm::mix(in_off, out_off, glm::smoothstep(0.f, 1.f, t));
			}

			if (std::abs(t_offset) > std::abs(target[i]))
				target[i] = t_offset;
		}
	}

	// ----------------------------------------------------------------
	// Phase 5 — Wide box-blur to smooth entry/exit transitions and
	// blend chicanes/consecutive corners naturally.
	// ----------------------------------------------------------------

	for (int i = 0; i < n; ++i)
		target[i] = glm::clamp(target[i],
		                       -wps[i].road_half_width * strength,
		                        wps[i].road_half_width * strength);

	// ----------------------------------------------------------------
	// Phase 6 — Gauss-Seidel minimum-curvature relaxation.
	// Seeded from the physics-derived corner targets; converges to the
	// globally minimum-curvature path inside the road corridor.
	// ----------------------------------------------------------------
	std::vector<glm::vec3> rl(n);
	for (int i = 0; i < n; ++i)
		rl[i] = wps[i].position + wps[i].right * target[i];

	const int gs_s = loop ? 0 : 1;
	const int gs_e = loop ? n : n - 1;

	for (int iter = 0; iter < num_iters; ++iter) {
		for (int i = gs_s; i < gs_e; ++i) {
			const int prev = loop ? (i + n - 1) % n : i - 1;
			const int nxt  = loop ? (i + 1) % n     : i + 1;

			const glm::vec3 ab    = rl[nxt] - rl[prev];
			const float     ab_sq = glm::dot(ab, ab);
			glm::vec3 ideal;
			if (ab_sq < 1e-8f)
				ideal = rl[i];
			else
				ideal = rl[prev] + glm::dot(rl[i] - rl[prev], ab) / ab_sq * ab;

			const float hw  = wps[i].road_half_width * strength;
			const float lat = glm::clamp(glm::dot(ideal - wps[i].position, wps[i].right), -hw, hw);
			rl[i] = wps[i].position + wps[i].right * lat;
		}
	}

	// ----------------------------------------------------------------
	// Phase 7 — Speed profile on the actual racing line.
	//
	// Re-derives curvature from rl[] (tighter than the centerline), then
	// runs the same gradient-aware forward/backward passes to produce a
	// physically accurate speed target at every waypoint.
	// Stored in wps[i].speed_mps for AI throttle/brake targeting.
	// ----------------------------------------------------------------
	{
		std::vector<float> rl_kappa(n, 0.f);
		for (int i = 0; i < n; ++i) {
			const int im = loop ? (i - 1 + n) % n : std::max(0, i - 1);
			const int ip = loop ? (i + 1) % n     : std::min(n - 1, i + 1);
			if (im == i || ip == i) continue;
			const glm::vec3 ba = rl[im] - rl[i], bc = rl[ip] - rl[i];
			const float la = glm::length(ba), lc = glm::length(bc);
			const float ac = glm::distance(rl[im], rl[ip]);
			if (la < 1e-4f || lc < 1e-4f || ac < 1e-4f) continue;
			rl_kappa[i] = glm::length(glm::cross(ba / la, bc / lc)) / (0.5f * ac);
		}
		rl_smooth_scalar(rl_kappa, loop, 3);

		std::vector<float> rl_ds(n, 0.f);
		for (int i = 0; i < n; ++i) {
			const int nxt = loop ? (i + 1) % n : std::min(i + 1, n - 1);
			rl_ds[i] = glm::distance(rl[i], rl[nxt]);
		}

		std::vector<float> rl_speed(n);
		for (int i = 0; i < n; ++i) {
			const float kabs   = std::max(kappa_floor, rl_kappa[i]);
			const float mu_eff = MU_LATERAL * std::cos(std::abs(wps[i].gradient));
			rl_speed[i] = std::sqrt(mu_eff * G_ACC / kabs);
		}

		for (int pass = 0; pass < (loop ? 2 : 1); ++pass) {
			for (int i = sw_s; i < sw_e; ++i) {
				const int cur  = loop ? i % n : i;
				const int prev = loop ? (i - 1 + n) % n : cur - 1;
				const float a_eff = std::max(0.3f, BIKE_ACCEL - G_ACC * std::sin(wps[cur].gradient));
				rl_speed[cur] = std::min(rl_speed[cur],
					std::sqrt(rl_speed[prev] * rl_speed[prev] + 2.f * a_eff * rl_ds[prev]));
			}
			for (int i = sw_e - 1; i >= sw_s; --i) {
				const int cur = loop ? i % n : i;
				const int nxt = loop ? (i + 1) % n : cur + 1;
				const float d_eff = std::max(1.f, BIKE_DECEL + G_ACC * std::sin(wps[cur].gradient));
				rl_speed[cur] = std::min(rl_speed[cur],
					std::sqrt(rl_speed[nxt] * rl_speed[nxt] + 2.f * d_eff * rl_ds[cur]));
			}
		}

		for (int i = 0; i < n; ++i) {
			wps[i].racing_line_pos     = rl[i];
			wps[i].racing_line_lateral = glm::dot(rl[i] - wps[i].position, wps[i].right);
			wps[i].speed_mps           = rl_speed[i];
		}
	}
}
