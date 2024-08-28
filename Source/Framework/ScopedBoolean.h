#pragma once
#include <cassert>
struct ScopedBooleanValue
{
	bool get_value() const {
		return value;
	}
private:
	friend class BooleanScope;
	bool value = false;
};
struct BooleanScope
{
	BooleanScope(ScopedBooleanValue& v) {
		assert(!v.value);
		ptr = &v;
		ptr->value = true;
	}
	~BooleanScope() {
		assert(ptr->value);
		ptr->value = false;
	}
private:
	ScopedBooleanValue* ptr = nullptr;
};