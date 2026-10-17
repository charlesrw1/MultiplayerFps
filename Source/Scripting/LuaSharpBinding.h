#pragma once
// Helpers for the LuaSharp thunks that Scripts/codegen_luasharp.py appends to each *MEGA.gen.cpp.
// Thunk signatures use C types only (see LuaSharp/docs/native-abi.md, "Extern classes").
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <string_view>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include "Framework/ClassBase.h"
#include "Framework/ReflectionProp.h"
#include "Framework/StringName.h"

namespace lsbind {

// C layouts of the LuaSharp math types (runtime/luasharp/vecmath.h). glm types have constructors, so MSVC does not
// return them in registers like C does; thunks use these instead.
struct Vec2 { float x, y; };
struct Vec3 { float x, y, z; };
struct Quat { float x, y, z, w; };
struct Mat4 { float m[16]; };
static_assert(sizeof(Vec2) == sizeof(glm::vec2) && sizeof(Vec3) == sizeof(glm::vec3) && sizeof(Mat4) == sizeof(glm::mat4));

inline glm::vec2 to_glm(Vec2 v) { return {v.x, v.y}; }
inline glm::vec3 to_glm(Vec3 v) { return {v.x, v.y, v.z}; }
inline glm::quat to_glm(Quat q) { return glm::quat(q.w, q.x, q.y, q.z); }
inline glm::mat4 to_glm(const Mat4& m) { glm::mat4 r; std::memcpy(&r, m.m, sizeof(m.m)); return r; }
inline Vec2 to_ls(const glm::vec2& v) { return {v.x, v.y}; }
inline Vec3 to_ls(const glm::vec3& v) { return {v.x, v.y, v.z}; }
inline Quat to_ls(const glm::quat& q) { return {q.x, q.y, q.z, q.w}; }
inline Mat4 to_ls(const glm::mat4& m) { Mat4 r; std::memcpy(r.m, &m, sizeof(r.m)); return r; }

// A LuaSharp string (LS_String*); null is the empty string.
std::string_view view(const void* string);
StringName name(const void* string);
// An interned LuaSharp string.
void* string(std::string_view text);
inline void* string(const char* text) { return string(std::string_view(text ? text : "")); }

// The host object behind a proxy is the ClassBase*; the script checker guarantees its class.
template <typename T> T* object(void* native) { return static_cast<T*>(static_cast<ClassBase*>(native)); }
// The one proxy of an object; null for null.
void* proxy_of(const ClassBase* object);

uint16_t field_offset(const PropertyInfoList* props, const char* name);
template <typename T, typename C> T& field(C* object, uint16_t offset) {
	return *reinterpret_cast<T*>(reinterpret_cast<char*>(object) + offset);
}

// LuaSharp enums are ordinals; values[ordinal] is the C++ value.
int64_t enum_value(const int64_t* values, size_t count, int64_t ordinal);
int64_t enum_ordinal(const int64_t* values, size_t count, int64_t value);

void report_error(const char* message);

struct Symbol
{
	const char* name;
	const void* address;
};
struct Class
{
	const char* name;
	const char* base; // empty for ClassBase
};
// The output of one codegen pass. Declarations are LuaSharp source chunks.
struct Bindings
{
	const char* pass;
	std::span<const char* const> declarations;
	std::span<const Symbol> symbols;
	std::span<const Class> classes;
};
std::span<const Bindings* const> all_bindings();

// Static registration from each generated file.
struct Registrar
{
	Registrar(const char* pass, std::span<const char* const> declarations, std::span<const Symbol> symbols,
			  std::span<const Class> classes);
	Bindings bindings;
};

} // namespace lsbind
