#pragma once
#include <cstdint>
class Entity;
class EntityPtr
{
public:
	bool is_valid() const { return get() != nullptr; }
	Entity* get() const;
	Entity& operator*() const {
		return *get();
	}
	operator bool() const {
		return is_valid();
	}
	Entity* operator->() const {
		return get();
	}

	bool operator==(const EntityPtr& other) {
		return handle == other.handle;
	}

	bool operator!=(const EntityPtr& other) {
		return handle != other.handle;
	}

	uint64_t handle = 0;
};