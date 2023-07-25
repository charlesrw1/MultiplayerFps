#ifndef MATHLIB_H
#define MATHLIB_H

#include "glm/glm.hpp"

inline glm::vec3 AnglesToVector(float pitch, float yaw) {
	return glm::vec3(cos(yaw) * cos(pitch), sin(pitch), sin(yaw) * cos(pitch));
}

struct Ray
{
	Ray() : pos(glm::vec3(0)), dir(glm::vec3(1, 0, 0)) {}
	Ray(glm::vec3 pos, glm::vec3 dir) :pos(pos), dir(dir) {}

	glm::vec3 at(float t) const {
		return pos + dir * t;
	}

	glm::vec3 pos;
	glm::vec3 dir;
};

struct Bounds
{
	Bounds() : bmin(INFINITY), bmax(-INFINITY) {}
	explicit Bounds(glm::vec3 pos) : bmin(pos), bmax(pos) {}
	Bounds(glm::vec3 min, glm::vec3 max) : bmin(min), bmax(max) {}

	float surface_area() const {
		glm::vec3 size = bmax - bmin;
		return 2.0 * (size.x * size.y + size.x * size.z + size.y * size.z);
	}

	bool inside(glm::vec3 p, float size) const {
		return bmin.x <= p.x + size && bmax.x >= p.x - size &&
			bmin.y <= p.y + size && bmax.y >= p.y - size &&
			bmin.z <= p.z + size && bmax.z >= p.z - size;
	}
	bool intersect(const Bounds& other) const {
		return bmin.x <= other.bmax.x && bmax.x >= other.bmin.x &&
			bmin.y <= other.bmax.y && bmax.y >= other.bmin.y &&
			bmin.z <= other.bmax.z && bmax.x >= other.bmin.z;
	}

	bool intersect(const Ray& r, float& t_out) const {
		glm::vec3 inv_dir = 1.f / r.dir;
		glm::vec3 tmin = (bmin - r.pos) * inv_dir;
		glm::vec3 tmax = (bmax - r.pos) * inv_dir;
		glm::vec3 t1_ = glm::min(tmin, tmax);
		glm::vec3 t2_ = glm::max(tmin, tmax);
		float tnear = glm::max(glm::max(t1_.x, t1_.y), t1_.z);
		float tfar = glm::min(glm::min(t2_.x, t2_.y), t2_.z);


		if (tnear > tfar || tfar < 0)
			return false;
		t_out = tnear;
		return true;

	}

	glm::vec3 get_center() const {
		return (bmin + bmax) / 2.f;
	}
	int longest_axis() const {
		glm::vec3 lengths = bmax - bmin;
		int max_num = 0;
		if (lengths[1] > lengths[max_num])
			max_num = 1;
		if (lengths[2] > lengths[max_num])
			max_num = 2;
		return max_num;
	}

	glm::vec3 bmin, bmax;
};
inline Bounds bounds_union(const Bounds& b1, const Bounds& b2) {
	Bounds b;
	b.bmin = glm::min(b1.bmin, b2.bmin);
	b.bmax = glm::max(b1.bmax, b2.bmax);
	return b;
}
inline Bounds bounds_union(const Bounds& b1, const glm::vec3& v) {
	Bounds b;
	b.bmin = glm::min(b1.bmin, v);
	b.bmax = glm::max(b1.bmax, v);
	return b;
}
inline bool bounds_intersect(const Bounds& b1, const Bounds& b2)
{
	return (b1.bmin.x < b2.bmax.x&& b2.bmin.x < b1.bmax.x)
		&& (b1.bmin.y < b2.bmax.y&& b2.bmin.y < b1.bmax.y)
		&& (b1.bmin.z < b2.bmax.z&& b2.bmin.z < b1.bmax.z);
}


inline glm::vec3 closest_point_on_line(const glm::vec3& A, const glm::vec3& B, const glm::vec3& point)
{
	glm::vec3 AB = B - A;
	float t = glm::dot(point - A, AB) / glm::dot(AB, AB);
	return A + glm::min(glm::max(t, 0.f), 1.f) * AB;
}

inline bool to_barycentric(glm::vec3 p1, glm::vec3 p2, glm::vec3 p3, glm::vec3 point, float& u, float& v, float& w)
{
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


inline unsigned int wang_hash(unsigned int seed)
{
	seed = (seed ^ 61) ^ (seed >> 16);
	seed *= 9;
	seed = seed ^ (seed >> 4);
	seed *= 0x27d4eb2d;
	seed = seed ^ (seed >> 15);
	return seed;
}

inline glm::vec3 hashed_color(int num)
{
	int has = wang_hash(num);
	return glm::vec3(has & 0xFF, (has >> 8) & 0xFF, (has >> 16) & 0xFF) / 255.f;
}

class Random
{
public:
	Random(unsigned int seed) {
		state = wang_hash(seed);
	}
	unsigned int Rand() {
		unsigned int x = state;
		x ^= x << 13;
		x ^= x >> 17;
		x ^= x << 5;
		state = x;
		return x;
	}
	float RandF() {
		return (float)Rand() * (1.0 / 4294967296.0);
	}
	float RandF(float min, float max) {
		return min + (max - min) * RandF();
	}
	int RandI(int start, int end) {
		int range = end - start;
		return start + Rand() % (range + 1);
	}

	unsigned int state;
};

#endif // !MATHLIB_H
