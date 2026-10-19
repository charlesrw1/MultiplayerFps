#pragma once
// Helpers for the LuaSharp thunks that Scripts/codegen_luasharp.py appends to each *MEGA.gen.cpp.
// Thunk signatures use C types only (see LuaSharp/docs/native-abi.md, "Extern classes").
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include "Framework/ClassBase.h"
#include "Framework/ReflectionProp.h"
#include "Framework/StringName.h"
#include "Framework/Util.h"

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
// The host object behind a proxy that a slot returns; null for null or destroyed.
void* native_of(void* proxy);

uint16_t field_offset(const PropertyInfoList* props, const char* name);
template <typename T, typename C> T& field(C* object, uint16_t offset) {
	return *reinterpret_cast<T*>(reinterpret_cast<char*>(object) + offset);
}

// LuaSharp enums are ordinals; values[ordinal] is the C++ value.
int64_t enum_value(const int64_t* values, size_t count, int64_t ordinal);
int64_t enum_ordinal(const int64_t* values, size_t count, int64_t value);

void report_error(const char* message);

// LS_List in runtime/luasharp/runtime.h. Lists cross by copy: to_list makes a new list, from_list a new vector.
struct List
{
	int64_t count;
	int64_t capacity;
	uint64_t element_size;
	uint64_t element_kind;
	const void* element_type;
	void* data;
};
// LS_KIND of the elements.
enum class ElementKind : uint64_t { Int = 1, Float = 2, Bool = 3, String = 4, Object = 5, Struct = 9 };
// A list of count zeroed elements. type names the extern class of Object elements and the struct of Struct elements.
List* new_list(ElementKind kind, const char* type, size_t element_size, size_t count);

// E is the element as the LuaSharp C layout stores it; convert makes one from an item.
template <typename E, typename Items, typename Convert>
void* to_list(const Items& items, ElementKind kind, const char* type, Convert convert) {
	List* list = new_list(kind, type, sizeof(E), items.size());
	E* data = static_cast<E*>(list->data);
	for (const auto& item : items)
		*data++ = convert(item);
	return list;
}
// A null list is empty.
template <typename E, typename Convert> auto from_list(const void* list, Convert convert) {
	std::vector<decltype(convert(std::declval<const E&>()))> result;
	if (!list)
		return result;
	const auto& header = *static_cast<const List*>(list);
	ASSERT(header.element_size == sizeof(E));
	const E* data = static_cast<const E*>(header.data);
	result.reserve(static_cast<size_t>(header.count));
	for (int64_t i = 0; i < header.count; i++)
		result.push_back(convert(data[i]));
	return result;
}

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
// A CLASS_BODY(X, scriptable) class. ScriptImpl_X calls slots[i] for the script override of slot_names[i].
struct Scriptable
{
	const char* name;
	std::span<const char* const> slot_names;
	// object was made by X's scriptable_allocate. A hot reload attaches the slots of the new module.
	void (*attach)(ClassBase* object, void* const* slots);
};
template <typename Impl> void attach(ClassBase* object, void* const* slots) {
	auto* impl = dynamic_cast<Impl*>(object);
	ASSERT(impl && slots);
	impl->ls_slots = slots;
}

// A declared struct as its C++ mirror lays it out. LuaSharpEngine checks it against each module's type info.
struct StructField
{
	const char* name;
	size_t offset;
};
struct StructLayout
{
	const char* name;
	size_t size;
	std::span<const StructField> fields;
};

// The output of one codegen pass. Declarations are LuaSharp source chunks.
struct Bindings
{
	const char* pass;
	std::span<const char* const> declarations;
	std::span<const Symbol> symbols;
	std::span<const Class> classes;
	std::span<const Scriptable> scriptables;
	std::span<const StructLayout> structs; // the structs this pass declares
};
std::span<const Bindings* const> all_bindings();
// The declarations of all passes as one source, in registration order.
std::string declaration_source();

// Static registration from each generated file.
struct Registrar
{
	Registrar(const char* pass, std::span<const char* const> declarations, std::span<const Symbol> symbols,
			  std::span<const Class> classes, std::span<const Scriptable> scriptables, std::span<const StructLayout> structs);
	Bindings bindings;
};

} // namespace lsbind
