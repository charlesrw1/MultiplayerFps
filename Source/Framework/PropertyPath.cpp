#include "Framework/PropertyPath.h"
#include <span>
#include "Framework/StructReflection.h"

const PropertyInfo& PathStep::info() const {
	ASSERT(prop);
	return is_element() ? *prop->list().element : *prop;
}

PropertyPath PropertyPath::child(const PropertyInfo* member) const {
	ASSERT(member);
	ASSERT(steps.empty() || leaf().info().type == core_type_id::ActualStruct);
	PropertyPath p = *this;
	p.steps.push_back({member});
	return p;
}

PropertyPath PropertyPath::element(int index) const {
	ASSERT(index >= 0 && !leaf().is_element() && leaf().prop->type == core_type_id::List);
	PropertyPath p = *this;
	p.steps.back().index = index;
	return p;
}

std::string PropertyPath::to_string() const {
	std::string s;
	for (const PathStep& step : steps) {
		if (!s.empty())
			s += '.';
		s += step.prop->name;
		if (step.is_element())
			s += "[" + std::to_string(step.index) + "]";
	}
	return s;
}

namespace {
bool in_range(const PathStep& s, const void* inst) {
	return !s.is_element() || s.index < s.prop->list().size(*s.prop, inst);
}

// Calls fn with the object that holds the leaf step. False (fn not called) when an index is out of range.
bool walk(void* inst, std::span<const PathStep> steps, FunctionRef<void(void*)> fn) {
	ASSERT(inst && !steps.empty());
	const PathStep& s = steps.front();
	if (!in_range(s, inst))
		return false;
	if (steps.size() == 1) {
		fn(inst);
		return true;
	}
	bool ok = false;
	auto next = [&](StructRef r) { ok = walk(r.data, steps.subspan(1), fn); };
	if (s.is_element())
		s.prop->list().with_element(*s.prop, inst, s.index, next);
	else
		s.prop->with_struct(inst, next);
	return ok;
}

void walk_or_assert(const PropertyRef& ref, FunctionRef<void(void*)> fn) {
	const bool ok = walk(ref.root, ref.path.get_steps(), fn);
	ASSERT(ok && "the property path does not resolve");
}
} // namespace

bool PropertyRef::resolves() const {
	return walk(root, path.get_steps(), [](void*) {});
}

PropValue PropertyRef::get_value() const {
	PropValue v;
	const PathStep& s = path.leaf();
	walk_or_assert(*this, [&](void* inst) {
		if (s.is_element())
			s.prop->list().get(*s.prop, inst, s.index, v);
		else
			s.prop->get_value(inst, v);
	});
	return v;
}

void PropertyRef::set_value(const PropValue& v, SetMode mode) const {
	const PathStep& s = path.leaf();
	walk_or_assert(*this, [&](void* inst) {
		if (s.is_element())
			s.prop->list().set(*s.prop, inst, s.index, v, mode);
		else
			s.prop->set_value(inst, v, mode);
	});
}

void PropertyRef::with_struct(StructFn fn) const {
	const PathStep& s = path.leaf();
	walk_or_assert(*this, [&](void* inst) {
		if (s.is_element())
			s.prop->list().with_element(*s.prop, inst, s.index, fn);
		else
			s.prop->with_struct(inst, fn);
	});
}

int PropertyRef::list_size() const {
	const PathStep& s = path.leaf();
	ASSERT(!s.is_element());
	int n = 0;
	walk_or_assert(*this, [&](void* inst) { n = s.prop->list().size(*s.prop, inst); });
	return n;
}

void PropertyRef::list_resize(int count) const {
	const PathStep& s = path.leaf();
	ASSERT(!s.is_element());
	walk_or_assert(*this, [&](void* inst) { s.prop->list().resize(*s.prop, inst, count); });
}

void PropertyRef::list_move(int from, int to) const {
	const PathStep& s = path.leaf();
	ASSERT(!s.is_element());
	walk_or_assert(*this, [&](void* inst) { s.prop->list().move(*s.prop, inst, from, to); });
}

void for_each_value(const PropertyInfoList* props, void* inst,
					FunctionRef<void(const PropertyInfo&, const PropValue&)> fn) {
	if (!props)
		return;
	ASSERT(inst);
	auto members = [&](StructRef s) { for_each_value(s.type->properties, s.data, fn); };
	for (int i = 0; i < props->count; i++) {
		const PropertyInfo& p = props->list[i];
		if (p.type == core_type_id::List) {
			const ListAccess& list = p.list();
			const int n = list.size(p, inst);
			for (int j = 0; j < n; j++) {
				if (list.element->type == core_type_id::ActualStruct) {
					list.with_element(p, inst, j, members);
				} else {
					PropValue v;
					list.get(p, inst, j, v);
					fn(*list.element, v);
				}
			}
		} else if (p.type == core_type_id::ActualStruct) {
			p.with_struct(inst, members);
		} else if (p.has_value()) {
			fn(p, p.get_value(inst));
		}
	}
}
