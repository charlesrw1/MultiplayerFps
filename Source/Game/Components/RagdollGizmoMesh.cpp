#include "RagdollGizmoMesh.h"
#include "Render/Model.h"
#include "Framework/Util.h"
#include "Framework/MeshBuilder.h"
#include <vector>
#include <cmath>
#include <algorithm>

void ragdoll_make_basis(glm::vec3 dir, glm::vec3 up_ref, glm::vec3& tangent, glm::vec3& bitangent) {
	if (glm::length(glm::cross(up_ref, dir)) < 0.001f)
		up_ref = (glm::abs(dir.y) < 0.9f) ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
	tangent = glm::normalize(glm::cross(up_ref, dir));
	bitangent = glm::cross(dir, tangent);
}

// Swing limits near +-90 degrees (common for near-hinge joints like elbows/knees) would blow up
// a naive tan(angle)*length cone radius. Clamp the *visualized* angle and cap the resulting
// radius as a multiple of the cone length so the gizmo stays a reasonable size regardless of how
// wide the actual physics limit is.
void ragdoll_cone_radii(float angle_y, float angle_z, float length, float& out_ry, float& out_rz) {
	float ay = glm::clamp(angle_y, 0.02f, 1.1f);
	float az = glm::clamp(angle_z, 0.02f, 1.1f);
	out_ry = glm::min(tanf(ay) * length, length * 2.2f);
	out_rz = glm::min(tanf(az) * length, length * 2.2f);
}

void ragdoll_append_cone_solid(ModelBuilder& mb, glm::vec3 apex, glm::vec3 dir, float angle_y, float angle_z,
								float length) {
	const int segs = 24;
	dir = glm::normalize(dir);
	glm::vec3 tangent, bitangent;
	ragdoll_make_basis(dir, glm::vec3(0, 0, 1), tangent, bitangent);
	float ry, rz;
	ragdoll_cone_radii(angle_y, angle_z, length, ry, rz);

	glm::vec3 tip = apex + dir * length;
	uint16_t apex_idx = mb.add_vertex(apex, {0, 0}, -dir);
	std::vector<uint16_t> ring(segs);
	for (int i = 0; i < segs; i++) {
		float t = TWOPI * i / segs;
		glm::vec3 p = tip + tangent * (cosf(t) * ry) + bitangent * (sinf(t) * rz);
		ring[i] = mb.add_vertex(p, {0, 0}, glm::normalize(p - apex));
	}
	// Lateral (cone) surface only, apex -> rim -- deliberately open/hollow at the wide end (no
	// end cap) so the two-tone material shows the blue outside from any angle and the orange
	// interior is visible looking in through the open end, instead of being sealed off.
	for (int i = 0; i < segs; i++) {
		int ni = (i + 1) % segs;
		mb.add_triangle(apex_idx, ring[i], ring[ni]);
	}
}

void ragdoll_append_wedge_solid(ModelBuilder& mb, glm::vec3 center, glm::vec3 axis, glm::vec3 zero_ref,
								 float min_rad, float max_rad, float radius, float thickness) {
	axis = glm::normalize(axis);
	zero_ref = zero_ref - axis * glm::dot(zero_ref, axis);
	if (glm::length(zero_ref) < 0.0001f)
		zero_ref = (glm::abs(axis.y) < 0.9f) ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
	zero_ref = glm::normalize(zero_ref);
	glm::vec3 perp = glm::cross(axis, zero_ref);

	const bool full_circle = (max_rad - min_rad) >= (TWOPI - 0.001f);
	const int segs = full_circle ? 32 : std::max(4, (int)(24.f * (max_rad - min_rad) / TWOPI) + 1);

	glm::vec3 top_ofs = axis * (thickness * 0.5f);
	auto ring_point = [&](float t) { return center + zero_ref * (cosf(t) * radius) + perp * (sinf(t) * radius); };

	uint16_t top_center = mb.add_vertex(center + top_ofs, {0, 0}, axis);
	uint16_t bot_center = mb.add_vertex(center - top_ofs, {0, 0}, -axis);
	std::vector<uint16_t> top_ring(segs + 1), bot_ring(segs + 1);
	for (int i = 0; i <= segs; i++) {
		float t = min_rad + (max_rad - min_rad) * ((float)i / segs);
		glm::vec3 p = ring_point(t);
		top_ring[i] = mb.add_vertex(p + top_ofs, {0, 0}, axis);
		bot_ring[i] = mb.add_vertex(p - top_ofs, {0, 0}, -axis);
	}
	for (int i = 0; i < segs; i++) {
		mb.add_triangle(top_center, top_ring[i], top_ring[i + 1]);
		mb.add_triangle(bot_center, bot_ring[i + 1], bot_ring[i]);
		// side wall between top/bottom rim, gives the wedge real (front/back-facing) thickness
		mb.add_quad(top_ring[i], top_ring[i + 1], bot_ring[i + 1], bot_ring[i]);
	}
	if (!full_circle) {
		// close the two flat radial ends of the pie slice
		mb.add_quad(top_center, top_ring[0], bot_ring[0], bot_center);
		mb.add_quad(top_ring[segs], top_center, bot_center, bot_ring[segs]);
	}
}

void ragdoll_append_capsule_solid(ModelBuilder& mb, glm::vec3 p0, glm::vec3 p1, float radius) {
	const int segs = 12;
	const int hemi_rings = 3; // latitude bands per hemisphere, excluding the pole and the equator
	glm::vec3 axis = p1 - p0;
	float len = glm::length(axis);
	glm::vec3 dir = (len > 0.0001f) ? axis / len : glm::vec3(0, 1, 0);
	glm::vec3 tangent, bitangent;
	ragdoll_make_basis(dir, glm::vec3(0, 0, 1), tangent, bitangent);

	// phi in [0, PI/2]: 0 = equator, PI/2 = pole. sign = -1 for the p0-side hemisphere (bulges
	// toward -dir), +1 for the p1-side (bulges toward +dir).
	auto add_ring = [&](glm::vec3 center, float phi, float sign) {
		float ring_radius = radius * cosf(phi);
		glm::vec3 ring_center = center + dir * (radius * sinf(phi) * sign);
		std::vector<uint16_t> ring(segs);
		for (int i = 0; i < segs; i++) {
			float t = TWOPI * i / segs;
			glm::vec3 n = tangent * cosf(t) + bitangent * sinf(t);
			glm::vec3 normal = glm::normalize(n * cosf(phi) + dir * (sinf(phi) * sign));
			ring[i] = mb.add_vertex(ring_center + n * ring_radius, {0, 0}, normal);
		}
		return ring;
	};

	uint16_t pole0 = mb.add_vertex(p0 - dir * radius, {0, 0}, -dir);
	std::vector<std::vector<uint16_t>> rings;
	for (int j = 1; j < hemi_rings; j++) // p0 hemisphere, near-pole -> equator
		rings.push_back(add_ring(p0, (PI * 0.5f) * (1.f - (float)j / hemi_rings), -1.f));
	rings.push_back(add_ring(p0, 0.f, -1.f)); // p0 equator == bottom of the cylindrical body
	rings.push_back(add_ring(p1, 0.f, 1.f));  // p1 equator == top of the cylindrical body
	for (int j = 1; j < hemi_rings; j++)      // p1 hemisphere, equator -> near-pole
		rings.push_back(add_ring(p1, (PI * 0.5f) * ((float)j / hemi_rings), 1.f));
	uint16_t pole1 = mb.add_vertex(p1 + dir * radius, {0, 0}, dir);

	for (int i = 0; i < segs; i++) { // pole0 -> first ring
		int ni = (i + 1) % segs;
		mb.add_triangle(pole0, rings.front()[i], rings.front()[ni]);
	}
	for (size_t r = 0; r + 1 < rings.size(); r++) { // consecutive latitude bands, including the
													 // p0-equator -> p1-equator cylinder band
		auto& a = rings[r];
		auto& b = rings[r + 1];
		for (int i = 0; i < segs; i++) {
			int ni = (i + 1) % segs;
			mb.add_quad(a[i], b[i], b[ni], a[ni]);
		}
	}
	for (int i = 0; i < segs; i++) { // last ring -> pole1 (reversed winding, matches the far-cap
									  // convention used elsewhere in this file)
		int ni = (i + 1) % segs;
		mb.add_triangle(pole1, rings.back()[ni], rings.back()[i]);
	}
}

void ragdoll_append_capsule_lines(MeshBuilder& mb, glm::vec3 p0, glm::vec3 p1, float radius, Color32 color) {
	const int segs = 16;
	glm::vec3 axis = p1 - p0;
	float len = glm::length(axis);
	glm::vec3 dir = (len > 0.0001f) ? axis / len : glm::vec3(0, 1, 0);
	glm::vec3 tangent, bitangent;
	ragdoll_make_basis(dir, glm::vec3(0, 0, 1), tangent, bitangent);

	auto ring_point = [&](glm::vec3 center, float t) {
		return center + tangent * (cosf(t) * radius) + bitangent * (sinf(t) * radius);
	};
	// equator ring at each cap
	for (int i = 0; i < segs; i++) {
		float t0 = TWOPI * i / segs;
		float t1 = TWOPI * (i + 1) / segs;
		mb.PushLine(ring_point(p0, t0), ring_point(p0, t1), color);
		mb.PushLine(ring_point(p1, t0), ring_point(p1, t1), color);
	}
	// A handful of longitude arcs running pole -> equator -> (straight through the cylinder) ->
	// equator -> pole, Unity-collider-gizmo style.
	const int longitudes = 4;
	const int arc_segs = 8;
	for (int L = 0; L < longitudes; L++) {
		float t = TWOPI * L / longitudes;
		glm::vec3 n = tangent * cosf(t) + bitangent * sinf(t);

		glm::vec3 prev = p0 - dir * radius; // p0-side pole
		for (int k = 1; k <= arc_segs; k++) {
			float phi = (PI * 0.5f) * (float)k / arc_segs; // 0=pole .. PI/2=equator
			glm::vec3 pt = p0 - dir * (radius * cosf(phi)) + n * (radius * sinf(phi));
			mb.PushLine(prev, pt, color);
			prev = pt;
		}
		glm::vec3 p1_eq = p1 + n * radius;
		mb.PushLine(prev, p1_eq, color); // straight run through the cylindrical body
		prev = p1_eq;
		for (int k = 1; k <= arc_segs; k++) {
			float psi = (PI * 0.5f) * (float)k / arc_segs; // 0=equator .. PI/2=pole
			glm::vec3 pt = p1 + dir * (radius * sinf(psi)) + n * (radius * cosf(psi));
			mb.PushLine(prev, pt, color);
			prev = pt;
		}
	}
}
