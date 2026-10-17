#pragma once
#include <cstdint>
#include <string>
#include <type_traits>
#include <variant>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include "Framework/Util.h"

class IAsset;
class ClassTypeInfo;
struct PropertyInfo;

// An asset reference. type is the asset class that the property accepts (the picker filters by it).
struct AssetRef
{
	IAsset* asset = nullptr;
	const ClassTypeInfo* type = nullptr;
	bool operator==(const AssetRef&) const = default;
};

// An object reference by instance id (EntityPtr::handle). 0 is null.
struct EntityRef
{
	int64_t handle = 0;
	bool operator==(const EntityRef&) const = default;
};

// The value of a property in its domain, independent of its storage: a float field and a LuaSharp double both give
// a double, std::string and a GC string both give a std::string. Editors and the serializer use only these.
using PropValue = std::variant<std::monostate, bool, int64_t, double, std::string, glm::vec2, glm::vec3, glm::quat,
							   Color32, AssetRef, EntityRef, const ClassTypeInfo*>;

enum class SetMode : uint8_t
{
	Edit, // an editor, a script or undo changes the value
	Load, // the serializer restores the value; the owner re-syncs once after the load
};

// Reads and writes the value of one property. Each storage form (C++ field, Lua shadow buffer, LuaSharp field)
// supplies one. inst is the owner object of the property (a struct for struct members).
struct PropertyAccess
{
	void (*get)(const PropertyInfo& prop, const void* inst, PropValue& out) = nullptr;
	void (*set)(const PropertyInfo& prop, void* inst, const PropValue& in, SetMode mode) = nullptr;
};

template <typename D> const D& prop_value_get(const PropValue& value) {
	const D* d = std::get_if<D>(&value);
	ASSERT(d && "the property value has a different domain type");
	return *d;
}

// A C++ type from its domain value: integers and enums are int64_t, floating point is double, IAsset* is an AssetRef.
template <typename T> T prop_value_as(const PropValue& value) {
	if constexpr (std::is_same_v<T, bool>)
		return prop_value_get<bool>(value);
	else if constexpr (std::is_integral_v<T> || std::is_enum_v<T>)
		return static_cast<T>(prop_value_get<int64_t>(value));
	else if constexpr (std::is_floating_point_v<T>)
		return static_cast<T>(prop_value_get<double>(value));
	else if constexpr (std::is_same_v<T, IAsset*>)
		return prop_value_get<AssetRef>(value).asset;
	else
		return prop_value_get<T>(value);
}

template <typename T> PropValue to_prop_value(const T& value) {
	if constexpr (std::is_same_v<T, bool>)
		return value;
	else if constexpr (std::is_integral_v<T> || std::is_enum_v<T>)
		return static_cast<int64_t>(value);
	else if constexpr (std::is_floating_point_v<T>)
		return static_cast<double>(value);
	else if constexpr (std::is_convertible_v<T, std::string>)
		return std::string(value);
	else
		return value;
}
