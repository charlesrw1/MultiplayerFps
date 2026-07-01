#include "MathLib.h"
#include <algorithm>

Bounds transform_bounds(glm::mat4 transform, Bounds b) {
	glm::vec3 corners[8];
	corners[0] = glm::vec3(b.bmin);
	corners[1] = glm::vec3(b.bmax.x, b.bmin.y, b.bmin.z);
	corners[2] = glm::vec3(b.bmax.x, b.bmax.y, b.bmin.z);
	corners[3] = glm::vec3(b.bmin.x, b.bmax.y, b.bmin.z);

	corners[4] = glm::vec3(b.bmin.x, b.bmin.y, b.bmax.z);
	corners[5] = glm::vec3(b.bmax.x, b.bmin.y, b.bmax.z);
	corners[6] = glm::vec3(b.bmax.x, b.bmax.y, b.bmax.z);
	corners[7] = glm::vec3(b.bmin.x, b.bmax.y, b.bmax.z);
	for (int i = 0; i < 8; i++) {
		corners[i] = transform * glm::vec4(corners[i], 1.0f);
	}

	Bounds out;
	out.bmin = corners[0];
	out.bmax = corners[0];
	for (int i = 1; i < 8; i++) {
		out.bmax = glm::max(out.bmax, corners[i]);
		out.bmin = glm::min(out.bmin, corners[i]);
	}
	return out;
}

bool to_barycentric(glm::vec3 p1, glm::vec3 p2, glm::vec3 p3, glm::vec3 point, float& u, float& v, float& w) {
	glm::vec3 e1 = p2 - p1;
	glm::vec3 e2 = p3 - p1;
	glm::vec3 N = glm::cross(e1, e2);
	float area = glm::length(N) / 2.f;

	e1 = p2 - p1;
	e2 = point - p1;
	glm::vec3 c = glm::cross(e1, e2);
	if (glm::dot(c, N) < -0.0001)
		return false;

	e1 = p3 - p2;
	e2 = point - p2;
	c = glm::cross(e1, e2);
	if (glm::dot(c, N) < -0.0001)
		return false;
	u = (glm::length(c) / 2.f) / area;

	e1 = p1 - p3;
	e2 = point - p3;
	c = glm::cross(e1, e2);
	if (glm::dot(c, N) < -0.0001)
		return false;
	v = (glm::length(glm::cross(e1, e2)) / 2.f) / area;

	w = 1 - u - v;

	return true;
}

bool barycentric_2d(glm::vec2 a, glm::vec2 b, glm::vec2 c, glm::vec2 p, float& u, float& v, float& w) {
	const glm::vec2 v0 = b - a, v1 = c - a, v2 = p - a;
	const float d00 = glm::dot(v0, v0), d01 = glm::dot(v0, v1), d11 = glm::dot(v1, v1);
	const float d20 = glm::dot(v2, v0), d21 = glm::dot(v2, v1);
	const float denom = d00 * d11 - d01 * d01;
	if (glm::abs(denom) < 1e-8f) { // degenerate (collinear) triangle
		u = 1.f;
		v = 0.f;
		w = 0.f;
		return false;
	}
	v = (d11 * d20 - d01 * d21) / denom;
	w = (d00 * d21 - d01 * d20) / denom;
	u = 1.f - v - w;
	const float eps = 0.0001f;
	return u >= -eps && v >= -eps && w >= -eps;
}

static bool circumcircle_contains_2d(glm::vec2 a, glm::vec2 b, glm::vec2 c, glm::vec2 p) {
	const double ax = a.x, ay = a.y, bx = b.x, by = b.y, cx = c.x, cy = c.y;
	const double d = 2.0 * (ax * (by - cy) + bx * (cy - ay) + cx * (ay - by));
	if (std::abs(d) < 1e-12)
		return false; // degenerate triangle contains nothing
	const double ux = ((ax * ax + ay * ay) * (by - cy) + (bx * bx + by * by) * (cy - ay) +
					   (cx * cx + cy * cy) * (ay - by)) /
					  d;
	const double uy = ((ax * ax + ay * ay) * (cx - bx) + (bx * bx + by * by) * (ax - cx) +
					   (cx * cx + cy * cy) * (bx - ax)) /
					  d;
	const double r2 = (ax - ux) * (ax - ux) + (ay - uy) * (ay - uy);
	const double dist2 = (p.x - ux) * (p.x - ux) + (p.y - uy) * (p.y - uy);
	return dist2 <= r2 + 1e-7;
}

std::vector<Triangle2D> delaunay_triangulate_2d(const std::vector<glm::vec2>& points) {
	std::vector<Triangle2D> triangles;
	const int n = (int)points.size();
	if (n < 3)
		return triangles;

	glm::vec2 mn = points[0], mx = points[0];
	for (auto& p : points) {
		mn = glm::min(mn, p);
		mx = glm::max(mx, p);
	}
	const glm::vec2 size = mx - mn;
	const float deltaMax = glm::max(size.x, size.y) * 10.f + 1.f;
	const glm::vec2 mid = (mn + mx) * 0.5f;

	// Super-triangle large enough to contain every input point; its vertices are appended
	// after the real points and any triangle still touching them gets discarded at the end.
	std::vector<glm::vec2> pts(points);
	// Tiny deterministic per-point jitter, well under any epsilon a caller would care about.
	// Breaks exact-cocircular ties (e.g. 4 corners of a rectangular parameter grid, a very
	// common blend-space layout) that would otherwise make the circumcircle test flip
	// depending on insertion order and produce overlapping triangles.
	const float jitter = glm::max(deltaMax * 0.00001f, 1e-6f);
	for (int i = 0; i < n; i++) {
		pts[i].x += jitter * ((float)((i * 2654435761u) % 1000u) / 1000.f - 0.5f);
		pts[i].y += jitter * ((float)((i * 40503u) % 1000u) / 1000.f - 0.5f);
	}
	const int i0 = n, i1 = n + 1, i2 = n + 2;
	pts.push_back(glm::vec2(mid.x - 2.f * deltaMax, mid.y - deltaMax));
	pts.push_back(glm::vec2(mid.x, mid.y + 2.f * deltaMax));
	pts.push_back(glm::vec2(mid.x + 2.f * deltaMax, mid.y - deltaMax));
	triangles.push_back({i0, i1, i2});

	struct Edge2 {
		int a, b;
		bool operator==(const Edge2& o) const { return (a == o.a && b == o.b) || (a == o.b && b == o.a); }
	};

	for (int pi = 0; pi < n; pi++) {
		const glm::vec2 p = pts[pi];

		std::vector<Triangle2D> bad;
		for (auto& t : triangles)
			if (circumcircle_contains_2d(pts[t.a], pts[t.b], pts[t.c], p))
				bad.push_back(t);

		// Boundary of the union of bad triangles: edges that appear in exactly one bad triangle.
		std::vector<Edge2> polygon;
		for (size_t bi = 0; bi < bad.size(); bi++) {
			const Edge2 edges[3] = {{bad[bi].a, bad[bi].b}, {bad[bi].b, bad[bi].c}, {bad[bi].c, bad[bi].a}};
			for (auto& e : edges) {
				int shared_count = 0;
				for (size_t bj = 0; bj < bad.size(); bj++) {
					if (bj == bi)
						continue;
					const Edge2 others[3] = {{bad[bj].a, bad[bj].b}, {bad[bj].b, bad[bj].c}, {bad[bj].c, bad[bj].a}};
					for (auto& e2 : others)
						if (e2 == e)
							shared_count++;
				}
				if (shared_count == 0)
					polygon.push_back(e);
			}
		}

		triangles.erase(std::remove_if(triangles.begin(), triangles.end(),
										[&](const Triangle2D& t) {
											for (auto& b : bad)
												if (b.a == t.a && b.b == t.b && b.c == t.c)
													return true;
											return false;
										}),
						 triangles.end());

		for (auto& e : polygon)
			triangles.push_back({e.a, e.b, pi});
	}

	triangles.erase(std::remove_if(triangles.begin(), triangles.end(),
									[&](const Triangle2D& t) { return t.a >= n || t.b >= n || t.c >= n; }),
					 triangles.end());

	return triangles;
}

bool line_plane_intersect(Ray r, glm::vec3 plane, float planed, glm::vec3& intersect) {
	float denom = dot(plane, r.dir);

	if (abs(denom) > 0.00001) { // such a high epsilon to deal with weird issues
		float planedist = dot(plane, r.pos) + planed;
		float time = -planedist / denom;
		intersect = r.pos + r.dir * time;
		return true;
	}
	return false;
}

glm::vec3 project_onto_line(glm::vec3 a, glm::vec3 b, glm::vec3 p) {
	glm::vec3 ap = p - a;
	glm::vec3 ab = b - a;
	return a + dot(ap, ab) / dot(ab, ab) * ab;
}
