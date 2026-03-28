#include "MathLib.h"

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
