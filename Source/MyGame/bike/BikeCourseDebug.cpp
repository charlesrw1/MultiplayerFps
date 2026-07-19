#include "BikeCourse.h"
#include "Debug.h"
#include <glm/glm.hpp>
#include <cmath>
#include <glm/gtc/constants.hpp>
void BikeCourse::debug_draw() const
{
	if (!is_built || (int)waypoints.size() < 2) return;

	const int n        = (int)waypoints.size();
	const int num_segs = is_loop ? n : n - 1;

	for (int i = 0; i < num_segs; ++i) {
		const BikeWaypoint& wp   = waypoints[i];
		const BikeWaypoint& next = waypoints[(i + 1) % n];

		// Colour by gradient: yellow = uphill, blue = downhill, green = flat
		const float grad_deg = glm::degrees(wp.gradient);
		Color32 line_color;
		if      (grad_deg >  3.f) line_color = Color32(0xff, 0xff, 0x00, 0xff); // yellow
		else if (grad_deg < -2.f) line_color = Color32(0x66, 0xaa, 0xff, 0xff); // blue
		else                      line_color = COLOR_GREEN;

		// Highlight the wrap-around segment in pink so it's obvious
		if (is_loop && i == n - 1)
			line_color = Color32(0xff, 0x66, 0xff, 0xff);

		Debug::add_line(wp.position, next.position, line_color, -1.f);

		// Road-width tick marks every 5th waypoint
		if (1) {
			const glm::vec3 left_edge  = wp.position - wp.right * wp.road_half_width;
			const glm::vec3 right_edge = wp.position + wp.right * wp.road_half_width;
			Debug::add_line(left_edge, right_edge, Color32(0xff, 0xff, 0xff, 0x66), -1.f);
		}

		// Racing line — use absolute world positions to avoid junction artefacts
		Debug::add_line(wp.racing_line_pos, next.racing_line_pos,
		                Color32(0xff, 0x99, 0x00, 0xff), -1.f);
	}
}

void BikeCourse::debug_draw_fillets() const
{
	for (const auto& f : debug_fillets) {
		// Center: red sphere + vertical pole
		Debug::add_sphere(f.center, 0.6f, Color32(0xff, 0x00, 0x00, 0xff), -1.f);
		Debug::add_line(f.center, f.center + glm::vec3(0, 3, 0), Color32(0xff, 0x00, 0x00, 0xff), -1.f);

		// pt_in (orange) and pt_out (yellow) markers
		Debug::add_sphere(f.pt_in,  0.4f, Color32(0xff, 0x80, 0x00, 0xff), -1.f);
		Debug::add_sphere(f.pt_out, 0.4f, Color32(0xff, 0xff, 0x00, 0xff), -1.f);

		// Radius lines: center → pt_in (orange) and center → pt_out (yellow)
		Debug::add_line(f.center, f.pt_in,  Color32(0xff, 0x80, 0x00, 0xaa), -1.f);
		Debug::add_line(f.center, f.pt_out, Color32(0xff, 0xff, 0x00, 0xaa), -1.f);

		// Arc approximation: step in small increments and draw chords
		const glm::vec3 from_c_in  = f.pt_in  - f.center;
		const glm::vec3 from_c_out = f.pt_out - f.center;
		const float angle_in  = std::atan2(from_c_in.z,  from_c_in.x);
		const float angle_out = std::atan2(from_c_out.z, from_c_out.x);
		float delta = angle_out - angle_in;
		if (f.left_turn) { if (delta < 0.f) delta += 2.f * glm::pi<float>(); }
		else             { if (delta > 0.f) delta -= 2.f * glm::pi<float>(); }

		constexpr int ARC_SEGS = 16;
		glm::vec3 prev_pt = f.pt_in;
		for (int k = 1; k <= ARC_SEGS; ++k) {
			const float t  = (float)k / ARC_SEGS;
			const float a  = angle_in + t * delta;
			const float y  = glm::mix(f.pt_in.y, f.pt_out.y, t);
			const glm::vec3 cur_pt = { f.center.x + f.radius * std::cos(a),
			                           y,
			                           f.center.z + f.radius * std::sin(a) };
			Debug::add_line(prev_pt, cur_pt, Color32(0x00, 0xff, 0xff, 0xff), -1.f);
			prev_pt = cur_pt;
		}
	}
}
