#pragma once
#include "Framework/ClassBase.h"
extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}
#include <cstdint>
#include <string>
#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"
#include "Framework/StructReflection.h"
#include "Framework/ClassBase.h"
#include "Animation/Runtime/Easing.h"
#include "Framework/Rect2d.h"
#include "Framework/MathLib.h"

struct lVec3 {
	STRUCT_BODY();
	lVec3() = default;
	lVec3(float x, float y, float z) :x(x), y(y), z(z) {}

	lVec3(const glm::vec3& v) : x(v.x), y(v.y), z(v.z) {}
	// implicit conversion to vec3*
	operator glm::vec3() const {
		return glm::vec3(x, y, z);
	}
	REF float x = 0;
	REF float y = 0;
	REF float z = 0;
};
struct lQuat {
	STRUCT_BODY();
	lQuat() = default;
	lQuat(const glm::quat& v) : x(v.x), y(v.y), z(v.z), w(v.w) {}
	// implicit conversion to vec3*
	operator glm::quat() const {
		return glm::quat(w, x, y, z);
	}
	REF float w = 1;
	REF float x = 0;
	REF float y = 0;
	REF float z = 0;
};

struct lTransform {
	STRUCT_BODY();
	REF lVec3 pos;
	REF lQuat rot;
};

struct lRect {
	STRUCT_BODY();
	lRect() = default;
	lRect(const Rect2d& rect) {
		x = rect.x;
		y = rect.y;
		w = rect.w;
		h = rect.h;
	}
	Rect2d to_rect2d() const {
		Rect2d out;
		out.x = x;
		out.y = y;
		out.w = w;
		out.h = h;
		return out;
	}

	REF float x = 0;
	REF float y = 0;
	REF float w = 0;
	REF float h = 0;
};

struct lVec3Pair {
	STRUCT_BODY();
	REF glm::vec3 side=glm::vec3(0.f);
	REF glm::vec3 up=glm::vec3(0.f);
};

// its not the best, but its simple ;)
class lMath : public ClassBase{
public:
	CLASS_BODY(lMath);
	REF static float length(const lVec3& v) {
		return glm::length(glm::vec3(v));
	}
	REF static float dot(const lVec3& v1, const lVec3& v2) {
		return glm::dot(glm::vec3(v1), glm::vec3(v2));
	}
	REF static lVec3 cross(const lVec3& v1, const lVec3& v2) {
		return glm::cross(glm::vec3(v1), glm::vec3(v2));
	}
	REF static lVec3 normalize(const lVec3& v) {
		return glm::normalize(glm::vec3(v));
	}
	REF static lVec3 vec_new(float f) {
		return lVec3(f,f,f);
	}
	REF static lVec3 vec_sub(const lVec3& v1, const lVec3& v2) {
		return glm::vec3(v1) - glm::vec3(v2);
	}
	REF static lVec3 vec_add(const lVec3& v1, const lVec3& v2) {
		return glm::vec3(v1) + glm::vec3(v2);
	}
	REF static lVec3 vec_mult(const lVec3& v1, const lVec3& v2) {
		return glm::vec3(v1) * glm::vec3(v2);
	}
	REF static lVec3 vec_multf(const lVec3& v1, float f) {
		return glm::vec3(v1) * f;
	}
	REF static lVec3 vec_clamp(const lVec3& v1, const lVec3& min, const lVec3& max) {
		return glm::clamp(glm::vec3(v1), glm::vec3(min), glm::vec3(max));
	}
	REF static lVec3 vec_min(const lVec3& v1, const lVec3& min) {
		return glm::min(glm::vec3(v1), glm::vec3(min));
	}
	REF static lVec3 vec_max(const lVec3& v1, const lVec3& max) {
		return glm::max(glm::vec3(v1), glm::vec3(max));
	}
	REF static lQuat from_euler(const lVec3& v) {
		return glm::quat(v);
	}
	REF static lVec3 to_euler(const lQuat& q) {
		return glm::eulerAngles(glm::quat(q));
	}
	REF static lQuat slerp(const lQuat& q1, const lQuat& q2, float alpha) {
		return glm::slerp(glm::quat(q1), glm::quat(q2), alpha);
	}
	REF static float eval_easing(Easing easing, float t) {
		return evaluate_easing(easing, t);
	}
	REF static lQuat quat_mult(const lQuat& q1, const lQuat& q2) {
		return glm::quat(q1) * glm::quat(q2);
	}
	REF static lQuat quat_inv(const lQuat& q) {
		return glm::inverse(glm::quat(q));
	}

	REF static lQuat quat_delta(const lQuat& from, const lQuat& to) {
		return glm::quat(to) * glm::inverse(glm::quat(from));
	}
	REF static lVec3 quat_multv(const lQuat& q, const lVec3& v) {
		return glm::quat(q) * glm::vec3(v);
	}



	REF static float damp_float(float a, float b, float smoothing, float dt) {
		return damp_dt_independent<float>(a, b, smoothing, dt);
	}
	REF static lVec3 damp_vector(const lVec3& a, const lVec3& b, float smoothing, float dt) {
		return damp_dt_independent<glm::vec3>(glm::vec3(a), glm::vec3(b), smoothing, dt);
	}
	REF static lQuat damp_quat(const lQuat& a, const lQuat& b, float smoothing, float dt) {
		return damp_dt_independent<glm::quat>(glm::quat(a), glm::quat(b), smoothing, dt);
	}

	// in radians
	REF static glm::vec3 angles_to_vector(float pitch, float yaw) {
		return AnglesToVector(pitch, yaw);
	}
	REF static float fmod(float f, float m) {
		return std::fmodf(f, m);
	}
	REF static float atan2f(float y, float x) {
		return std::atan2f(y, x);
	}
	// returns pitch in x, yaw in y. radians
	REF static glm::vec3 vector_to_angles(glm::vec3 v) {
		glm::vec3 out{};
		::vector_to_angles(v, out.x, out.y);	// name clashing, source this fram Framework/Mathlib.h
		return out;
	}
	REF static float pow(float x, float p) {
		return std::powf(x, p);
	}

	REF static lVec3Pair make_orthogonal_vectors(glm::vec3 v) {
		glm::vec3 up(0, 1, 0);
		if (glm::dot(v, up) > 0.999) {
			up = glm::vec3(1, 0, 0);
		}
		glm::vec3 side = glm::cross(v, up);
		lVec3Pair out;
		out.up = glm::cross(side, v);
		out.side = side;
		return out;
	}

};

class ClassBase;
void push_bool_to_lua(lua_State* L, bool b);
void push_float_to_lua(lua_State* L, float f);
void push_int_to_lua(lua_State* L, int64_t i);
void push_std_string_to_lua(lua_State* L, const std::string& str);
void push_object_to_lua(lua_State* L, const ClassBase* ptr);
bool get_bool_from_lua(lua_State* L, int index);
float get_float_from_lua(lua_State* L, int index);
int64_t get_int_from_lua(lua_State* L, int index);
std::string get_std_string_from_lua(lua_State* L, int index);
ClassBase* get_object_from_lua(lua_State* L, int index);

template<typename T>
ClassBase* allocate_script_impl_internal(const ClassTypeInfo* info) {
	T* ptr = new T();
	ptr->type = info;
	return (ClassBase*)ptr;
}
template<typename T>
ClassTypeInfo::CreateObjectFunc get_allocate_script_impl_internal() {
	return allocate_script_impl_internal<T>;
}

template<typename T, typename FUNCTOR>
void push_std_vector_to_lua(lua_State* L,const std::vector<T>& v, FUNCTOR&& func) {
	lua_newtable(L);
	for (int i = 0; i < v.size(); i++) {
		func(v[i]);
		lua_seti(L, -2, i + 1);
	}
}//
template<typename T,typename FUNCTOR>
std::vector<T> get_std_vector_from_lua(lua_State* L, int index, FUNCTOR&& func) {
	std::vector<T> result;
	if (!lua_istable(L, index)) {
		lua_error(L);
		return result;
	}
	if (index < 0)
		index = lua_gettop(L) + index + 1;
	lua_Integer i = 1;
	while (true) {
		lua_geti(L, index, i);
		if (lua_isnil(L, -1)) {
			lua_pop(L, 1);
			break; 
		}
		result.push_back(func());
		lua_pop(L, 1); // remove value
		++i;
	}
	return result;
}