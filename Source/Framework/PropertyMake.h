#pragma once
#include <string>
#include <type_traits>
#include <vector>
#include "Framework/ReflectionProp.h"
#include "Framework/StructReflection.h"
#include "Framework/ArrayReflection.h"
#include "Framework/StringName.h"

// make_property<&T::field, offset>(): the PropertyInfo of a C++ field. The compiler deduces the type tag, the
// enum/struct/asset/object type and the list element handling from the member type, and gives a typed accessor.
// A member type that has no property form is a compile error. Generated code (Scripts/codegen_generate.py) uses it.

class IAsset;
template <typename T> class AssetPtr;
template <typename T> class obj;
template <typename T> struct ClassTypePtr;

namespace prop_make {
template <typename T> struct asset_ptr : std::false_type {};
template <typename A> struct asset_ptr<AssetPtr<A>> : std::true_type { using type = A; };
template <typename T> struct object_handle : std::false_type {};
template <typename O> struct object_handle<obj<O>> : std::true_type { using type = O; };
template <typename T> struct class_type_ptr : std::false_type {};
template <typename B> struct class_type_ptr<ClassTypePtr<B>> : std::true_type { using type = B; };
template <typename T> struct list : std::false_type {};
template <typename E> struct list<std::vector<E>> : std::true_type {
	using element = E;
	using callback = StdVectorCallback<E>;
};
template <typename E, uint32_t N> struct list<InlineVec<E, N>> : std::true_type {
	using element = E;
	using callback = InlineVectorCallback<E, N>;
};
template <typename T> constexpr bool is_struct = requires { T::StructType; };
template <typename T> constexpr bool is_raw_asset = std::is_pointer_v<T> && requires { std::remove_pointer_t<T>::StaticType; };
template <typename T> using raw_asset_t = std::remove_cv_t<std::remove_pointer_t<T>>;
template <typename T> constexpr bool has_value = !list<T>::value && !is_struct<T>;

template <typename T> struct member;
template <typename C, typename T> struct member<T C::*>
{
	using owner = C;
	using type = T;
};

// T to and from its domain value (PropValue).
template <typename T> PropValue to_domain(const T& v) {
	if constexpr (std::is_same_v<T, StringName>) {
		const char* s = v.get_c_str();
		return std::string(s ? s : "");
	} else if constexpr (std::is_same_v<T, Color32>)
		return v;
	else if constexpr (asset_ptr<T>::value)
		return AssetRef{v.ptr, &asset_ptr<T>::type::StaticType};
	else if constexpr (is_raw_asset<T>)
		return AssetRef{const_cast<raw_asset_t<T>*>(v), &raw_asset_t<T>::StaticType};
	else if constexpr (object_handle<T>::value)
		return EntityRef{v.handle};
	else if constexpr (class_type_ptr<T>::value)
		return v.ptr;
	else
		return to_prop_value(v);
}

template <typename T> void from_domain(T& v, const PropValue& in) {
	if constexpr (std::is_same_v<T, StringName>)
		v = StringName(prop_value_get<std::string>(in).c_str());
	else if constexpr (asset_ptr<T>::value || is_raw_asset<T>) {
		using A = std::conditional_t<is_raw_asset<T>, raw_asset_t<T>, typename asset_ptr<T>::type>;
		IAsset* asset = prop_value_get<AssetRef>(in).asset;
		A* typed = static_cast<A*>(asset);
		ASSERT(!typed || typed->get_type().is_a(A::StaticType));
		if constexpr (is_raw_asset<T>)
			v = typed;
		else
			v.ptr = typed;
	} else if constexpr (object_handle<T>::value)
		v.handle = prop_value_get<EntityRef>(in).handle;
	else if constexpr (class_type_ptr<T>::value) {
		const ClassTypeInfo* type = prop_value_get<const ClassTypeInfo*>(in);
		ASSERT(!type || type->is_a(class_type_ptr<T>::type::StaticType));
		v.ptr = type;
	} else
		v = prop_value_as<T>(in);
}

// The accessor of a value at Locate(inst).
template <typename T, auto Locate> struct TypedAccess
{
	static void get(const PropertyInfo&, const void* inst, PropValue& out) {
		out = to_domain(*Locate(const_cast<void*>(inst)));
	}
	static void set(const PropertyInfo&, void* inst, const PropValue& in, SetMode) { from_domain(*Locate(inst), in); }
	static constexpr PropertyAccess table{&get, &set};
};
template <auto Member> typename member<decltype(Member)>::type* locate_member(void* inst) {
	return &(static_cast<typename member<decltype(Member)>::owner*>(inst)->*Member);
}
template <typename T> T* locate_self(void* inst) {
	return static_cast<T*>(inst);
}
template <typename T, auto Locate> const PropertyAccess* value_access() {
	if constexpr (has_value<T>)
		return &TypedAccess<T, Locate>::table;
	else
		return nullptr;
}

template <typename T> PropertyInfo element_property();

// Type tag and type infos of T; access is the accessor for values (unused for lists and structs).
template <typename T> void describe(PropertyInfo& p, const PropertyAccess* access) {
	if constexpr (list<T>::value) {
		static typename list<T>::callback callback(element_property<typename list<T>::element>());
		p.type = core_type_id::List;
		p.list_ptr = &callback;
		return;
	} else if constexpr (is_struct<T>) {
		p.type = core_type_id::ActualStruct;
		p.struct_type = &T::StructType;
		return;
	} else if constexpr (std::is_same_v<T, bool>)
		p.type = core_type_id::Bool;
	else if constexpr (std::is_same_v<T, Color32>) {
		static_assert(sizeof(Color32) == 4);
		p.type = core_type_id::Int32;
		if (!*p.custom_type_str)
			p.custom_type_str = "ColorUint";
	} else if constexpr (std::is_enum_v<T>) {
		constexpr core_type_id tags[] = {core_type_id::Enum8, core_type_id::Enum16, core_type_id::Enum32,
										 core_type_id::Enum32};
		static_assert(sizeof(T) <= 4, "64-bit enums have no property form");
		p.type = tags[sizeof(T) / 2];
		p.enum_type = &EnumTrait<T>::StaticEnumType;
	} else if constexpr (std::is_integral_v<T>) {
		constexpr core_type_id tags[] = {core_type_id::Int8, core_type_id::Int16, core_type_id::Int32,
										 core_type_id::Int32, core_type_id::Int64};
		p.type = tags[sizeof(T) == 8 ? 4 : sizeof(T) / 2];
	} else if constexpr (std::is_same_v<T, float>)
		p.type = core_type_id::Float;
	else if constexpr (std::is_same_v<T, double>)
		p.type = core_type_id::Double;
	else if constexpr (std::is_same_v<T, std::string>)
		p.type = core_type_id::StdString;
	else if constexpr (std::is_same_v<T, StringName>)
		p.type = core_type_id::StringName;
	else if constexpr (std::is_same_v<T, glm::vec2>)
		p.type = core_type_id::Vec2;
	else if constexpr (std::is_same_v<T, glm::vec3>)
		p.type = core_type_id::Vec3;
	else if constexpr (std::is_same_v<T, glm::quat>)
		p.type = core_type_id::Quat;
	else if constexpr (asset_ptr<T>::value) {
		p.type = core_type_id::AssetPtr;
		p.class_type = &asset_ptr<T>::type::StaticType;
	} else if constexpr (is_raw_asset<T>) {
		static_assert(std::is_base_of_v<IAsset, raw_asset_t<T>>, "a raw class pointer must be an asset");
		p.type = core_type_id::AssetPtr;
		p.class_type = &raw_asset_t<T>::StaticType;
	} else if constexpr (object_handle<T>::value) {
		p.type = core_type_id::ObjHandlePtr;
		p.class_type = &object_handle<T>::type::StaticType;
	} else if constexpr (class_type_ptr<T>::value) {
		p.type = core_type_id::ClassTypeInfo;
		p.class_type = &class_type_ptr<T>::type::StaticType;
	} else
		static_assert(sizeof(T) == 0, "this member type has no property form");
	p.access = access;
}

template <typename T> PropertyInfo element_property() {
	PropertyInfo p("", 0, PROP_DEFAULT);
	describe<T>(p, value_access<T, &locate_self<T>>());
	return p;
}
} // namespace prop_make

template <auto Member, uint16_t Offset>
PropertyInfo make_property(const char* name, uint32_t flags, const char* custom_type = "", const char* hint = "",
						   const char* tooltip = "", const char* former_name = nullptr) {
	using T = typename prop_make::member<decltype(Member)>::type;
	PropertyInfo p(name, Offset, flags);
	p.custom_type_str = custom_type;
	p.range_hint = hint;
	p.tooltip = tooltip;
	p.former_name = former_name;
	prop_make::describe<T>(p, prop_make::value_access<T, &prop_make::locate_member<Member>>());
	return p;
}
