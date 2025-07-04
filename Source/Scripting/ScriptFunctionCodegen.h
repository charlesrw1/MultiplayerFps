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