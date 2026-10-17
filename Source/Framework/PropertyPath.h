#pragma once
#include <string>
#include <vector>
#include "Framework/ReflectionProp.h"

// The route from an owner object to one property: "items[3].tint". Code keeps a path, never a pointer into the
// owner; each access walks the path again and checks every list index.
struct PathStep
{
	const PropertyInfo* prop = nullptr; // a member of the current object
	int index = -1;						// >= 0: element index of the list prop
	bool is_element() const { return index >= 0; }
	// What the step reaches: the list's element description for elements, else prop.
	const PropertyInfo& info() const;
};

class PropertyPath
{
public:
	PropertyPath() = default;
	explicit PropertyPath(const PropertyInfo* prop) { steps.push_back({prop}); }

	// The path to a member of the struct that this path reaches.
	PropertyPath child(const PropertyInfo* member) const;
	// The path to an element of the list that this path reaches.
	PropertyPath element(int index) const;

	bool empty() const { return steps.empty(); }
	const PathStep& leaf() const {
		ASSERT(!steps.empty());
		return steps.back();
	}
	const std::vector<PathStep>& get_steps() const { return steps; }
	std::string to_string() const;

private:
	std::vector<PathStep> steps;
};

// A property of one owner object. root is the object that holds the first step (a ClassBase for class properties).
struct PropertyRef
{
	void* root = nullptr;
	PropertyPath path;

	const PropertyInfo& info() const { return path.leaf().info(); }
	// False when a list index on the path is out of range.
	bool resolves() const;

	PropValue get_value() const;
	void set_value(const PropValue& v, SetMode mode = SetMode::Edit) const;
	template <typename T> T get() const { return prop_value_as<T>(get_value()); }
	template <typename T> void set(const T& value, SetMode mode = SetMode::Edit) const {
		if constexpr (std::is_same_v<T, IAsset*>)
			set_value(AssetRef{value, info().class_type}, mode);
		else
			set_value(to_prop_value(value), mode);
	}
	// Struct properties and struct elements: lends the struct for one call.
	void with_struct(StructFn fn) const;

	// List properties.
	int list_size() const;
	void list_resize(int count) const;
	void list_move(int from, int to) const;
};

// Calls fn with the value of each value property under props of inst, through structs and lists.
void for_each_value(const PropertyInfoList* props, void* inst, FunctionRef<void(const PropertyInfo&, const PropValue&)> fn);
