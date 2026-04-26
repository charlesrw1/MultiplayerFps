#include "BikeCourse.h"
#include <glm/glm.hpp>
#include <cmath>
#include "Framework/Config.h"
#include "imgui.h"

void raceline_debug()
{
	ImGui::TextUnformatted("Racing line: hinge-spring physics simulation.");
	ImGui::TextUnformatted("Tune k / mass / dt / num_iters via the caller.");
}
ADD_TO_DEBUG_MENU(raceline_debug);

// ──────────────────────────────────────────────────────────────────────────────
// Racing-line optimisation: hinge-spring physics simulation
//
// Each waypoint is treated as a mass that can slide laterally (along its right
// vector) within the road corridor.  Three consecutive waypoints form a hinge:
//   m1 = wps[i-1],  m2 = wps[i],  m3 = wps[i+1]
//
// The hinge angle at m2 generates a restoring force that tries to straighten
// the chain.  At a corner the force pushes m2 to the inside, producing the
// outside-entry / late-apex / outside-exit pattern naturally.
//
// Per hinge (i-1, i, i+1):
//   v21      = P_i  - P_{i-1}            (m2→m1 direction)
//   v23      = P_i  - P_{i+1}            (m2→m3 direction)
//   binormal = normalize(cross(v21,v23))
//   F3       = -cross(normalize(v23), binormal) * hinge_angle * k / |v23|
//   F on m2  = -2 * F3
//
// Velocity is damped toward F/mass each step (lerp factor = dt/100).
// Only the projection of velocity onto right[i] changes alpha[i].
// alpha is clamped to ±road_half_width each step.
// ──────────────────────────────────────────────────────────────────────────────

void BikeCourse::compute_racing_line(std::vector<BikeWaypoint>& wps, bool loop,
                                     float k, float mass, float dt, int num_iters)
{
	const int n = (int)wps.size();
	if (n < 3) return;

	std::vector<float>     alpha(n, 0.f);
	std::vector<glm::vec3> vel(n, glm::vec3(0.f));

	const int first = loop ? 0 : 1;
	const int last  = loop ? n : n - 1;
	const float lerp = dt / 100.f;

	std::vector<glm::vec3> force(n);

	for (int iter = 0; iter < num_iters; ++iter) {
		// Accumulate hinge forces for all three masses per hinge (Jacobi).
		// Each hinge (im, i, ip) contributes:
		//   F3 on m3 (ip)  →  vel target = -F3/mass
		//   F1 on m1 (im)  →  vel target = -F3/mass  (F1 == F3)
		//   F2 on m2 (i)   →  vel target = -F2/mass = +2*F3/mass
		// Without the m1/m3 contributions the entry/exit never get pushed outward.
		std::fill(force.begin(), force.end(), glm::vec3(0.f));

		for (int i = first; i < last; ++i) {
			const int im = loop ? (i - 1 + n) % n : i - 1;
			const int ip = loop ? (i + 1) % n     : i + 1;

			const glm::vec3 P_im = wps[im].position + wps[im].right * alpha[im];
			const glm::vec3 P_i  = wps[i].position  + wps[i].right  * alpha[i];
			const glm::vec3 P_ip = wps[ip].position + wps[ip].right * alpha[ip];

			const glm::vec3 v21 = P_i - P_im;
			const glm::vec3 v23 = P_i - P_ip;

			const float v23_len = glm::length(v23);
			if (v23_len < 1e-6f) continue;
			const glm::vec3 v23n = v23 / v23_len;

			const glm::vec3 cross_raw = glm::cross(v21, v23);
			const float cross_len = glm::length(cross_raw);
			if (cross_len < 1e-8f) continue;
			const glm::vec3 binormal = cross_raw / cross_len;

			const float cos_a       = glm::dot(glm::normalize(-v21), v23n);
			const float hinge_angle = std::acos(glm::clamp(cos_a, -1.f, 1.f));

			const glm::vec3 F3 = -glm::cross(v23n, binormal) * (hinge_angle * k / v23_len);

			force[i]  += 2.f * F3;   // m2: target = -F2/mass = +2F3/mass → inward at apex
			force[im] -= F3;          // m1: target = -F3/mass → outward on approach
			force[ip] -= F3;          // m3: target = -F3/mass → outward on exit
		}

		for (int i = first; i < last; ++i) {
			vel[i] += (force[i] / mass - vel[i]) * lerp;
			const float d_alpha = glm::dot(vel[i], wps[i].right);
			// 0.9 keeps the racing line off the road edge — full-edge positions
			// cause AI crashes and produce hard-to-recover training episodes.
			const float hw      = wps[i].road_half_width * 0.9f;
			alpha[i] = glm::clamp(alpha[i] + d_alpha, -hw, hw);
		}
	}

	// For loop circuits: two-stage smoothing to eliminate the seam kink.
	if (loop) {
		// Stage 1: mild global passes to propagate any local imbalances.
		// The blend is small enough to preserve the physics solution everywhere else.
		for (int pass = 0; pass < 8; ++pass) {
			std::vector<float> a2(n);
			for (int i = 0; i < n; ++i) {
				const int im = (i - 1 + n) % n;
				const int ip = (i + 1) % n;
				const float hw = wps[i].road_half_width * 0.9f;
				a2[i] = glm::clamp(
					alpha[i] + 0.15f * (0.5f * (alpha[im] + alpha[ip]) - alpha[i]),
					-hw, hw);
			}
			alpha = std::move(a2);
		}

		// Stage 2: targeted seam stitching.
		// The global loop constraint (alpha[n-1] ↔ alpha[0]) converges slowest in
		// the Jacobi simulation; the 8 global passes above only reduce the boundary
		// jump by ~73%.  30 passes at w=0.3 on a ±sw window reduce it by 0.7^30 ≈ 2e-5
		// (essentially zero) without distorting the solution far from the seam.
		const int sw = std::min(n / 4, 12);
		for (int pass = 0; pass < 30; ++pass) {
			std::vector<float> a2 = alpha;
			for (int k = -sw; k <= sw; ++k) {
				const int i  = ((k % n) + n) % n;
				const int im = (i - 1 + n) % n;
				const int ip = (i + 1) % n;
				const float hw = wps[i].road_half_width * 0.9f;
				a2[i] = glm::clamp(
					alpha[i] + 0.3f * (0.5f * (alpha[im] + alpha[ip]) - alpha[i]),
					-hw, hw);
			}
			alpha = std::move(a2);
		}
	}

	for (int i = 0; i < n; ++i) {
		wps[i].racing_line_lateral = alpha[i];
		wps[i].racing_line_pos     = wps[i].position + wps[i].right * alpha[i];
	}
}
