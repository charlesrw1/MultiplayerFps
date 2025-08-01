#ifndef MATHLIB_H
#define MATHLIB_H

#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"
#include "Framework/Util.h"

inline glm::vec3 AnglesToVector(float pitch, float yaw) {
	return glm::vec3(cos(yaw) * cos(pitch), sin(pitch), sin(yaw) * cos(pitch));
}
inline void vector_to_angles(const glm::vec3& v, float& pitch, float& yaw) {
	pitch = std::atan2(v.y, std::sqrt(v.x * v.x + v.z * v.z));
	yaw = std::atan2(v.x, v.z);
}

inline glm::mat4 MakeInfReversedZProjRH(float fovY_radians, float aspectWbyH, float zNear)
{
	float f = 1.0f / tan(fovY_radians / 2.0f);
	return glm::mat4(
		f / aspectWbyH, 0.0f, 0.0f, 0.0f,
		0.0f, f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, -1.0f,
		0.0f, 0.0f, zNear, 0.0f);
}

inline float linearize_depth(float d, float zNear, float zFar)
{
	float z_n = 2.0 * d - 1.0;
	return 2.0 * zNear * zFar / (zFar + zNear - z_n * (zFar - zNear));
}
inline glm::vec4 color32_to_vec4(Color32 c) {
	return glm::vec4(c.r / 255.0, c.g / 255.0, c.b / 255.0, c.a / 255.0);
}
inline glm::vec4 colorvec_srgb_to_linear(const glm::vec4& color) {
	auto to_linear = [](float x) {
		return glm::pow(x, 2.2f);
	};
	return glm::vec4(to_linear(color.x), to_linear(color.y), to_linear(color.z), color.w/* dont correct w?*/);
}
inline glm::vec4 colorvec_linear_to_srgb(const glm::vec4& color) {
	auto to_srgb = [](float x) {
		return glm::pow(x, 1.0/2.2f);
	};
	return glm::vec4(to_srgb(color.x), to_srgb(color.y), to_srgb(color.z), color.w/* dont correct w?*/);
}



// smoothing = [0,1] where 0 is no smoothing and 1.0 is max smoothing 
template<typename T>
static T damp_dt_independent(T a, T b, float smoothing, float dt)
{
	float alpha = pow(smoothing, dt);
	return glm::mix(a, b, alpha);
}

template<>
static glm::quat damp_dt_independent(glm::quat a, glm::quat b, float smoothing, float dt)
{
	float alpha = pow(smoothing, dt);
	return glm::slerp(a, b, alpha);
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


inline bool ray_plane_intersect(const Ray& r, const glm::vec3& plane_normal, const glm::vec3& plane_point,
                          glm::vec3& intersect) {
    float dot = glm::dot(r.dir, plane_normal);
    if (glm::abs(dot) < 0.000001) {
        return false; 
    }
    float distance = glm::dot(plane_normal, plane_point - r.pos) / dot;

    intersect = r.pos + distance * r.dir;

    return true;
}

struct Bounds
{
	Bounds() : bmin(INFINITY), bmax(-INFINITY) {}
	explicit Bounds(glm::vec3 pos) : bmin(pos), bmax(pos) {}
	Bounds(glm::vec3 min, glm::vec3 max) : bmin(min), bmax(max) {}

	float surface_area() const {
		glm::vec3 size = bmax - bmin;
		return 2.0f * (size.x * size.y + size.x * size.z + size.y * size.z);
	}

	bool inside(glm::vec3 p, float size) const {
		return bmin.x <= p.x + size && bmax.x >= p.x - size &&
			bmin.y <= p.y + size && bmax.y >= p.y - size &&
			bmin.z <= p.z + size && bmax.z >= p.z - size;
	}
	bool intersect(const Bounds& other) const {
		bool xoverlap = bmin.x <= other.bmax.x && bmax.x >= other.bmin.x;
		bool yoverlap = bmin.y <= other.bmax.y && bmax.y >= other.bmin.y;
		bool zoverlap = bmin.z <= other.bmax.z && bmax.z >= other.bmin.z;

		return xoverlap && yoverlap && zoverlap;
	}

	// To help with templated bvh
	bool intersect(const Bounds& other, float& t_out) const {
		t_out = 0.f;
		return
			bmin.x <= other.bmax.x && bmax.x >= other.bmin.x &&
			bmin.y <= other.bmax.y && bmax.y >= other.bmin.y &&
			bmin.z <= other.bmax.z && bmax.z >= other.bmin.z;
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

// damp function useable with variable update rates
template<typename T>
inline T damp(T source, T target, float smoothing, float dt)
{
	return glm::mix(source, target, 1 - glm::pow(smoothing, dt));
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

inline void decompose_transform(const glm::mat4& transform, glm::vec3& p, glm::quat& q, glm::vec3& s)
{
	p = transform[3];
	s = glm::vec3(glm::length(transform[0]), glm::length(transform[1]), glm::length(transform[2]));
	q = glm::quat_cast(glm::mat3(
		transform[0] / s.x,
		transform[1] / s.y,
		transform[2] / s.z
	));
	q = glm::normalize(q);
}
inline glm::mat4 compose_transform(const glm::vec3& v, const glm::quat& q, const glm::vec3& s)
{
	glm::mat4 model;
	model = glm::translate(glm::mat4(1), v);
	model = model * glm::mat4_cast(q);
	model = glm::scale(model, glm::vec3(s));
	return model;
}

#endif // !MATHLIB_H
