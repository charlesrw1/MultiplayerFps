#pragma once
#include <cstdint>
#include "GameEnginePublic.h"
class Entity;

template<typename T>
class obj
{
public:
	obj() {}
	obj(const T* e) {
		if (e)
			handle = e->get_instance_id();
		else
			handle = 0;
	}
	explicit obj(uint64_t handle) : handle(handle) {}

	// implicit conversion to T*
	operator T* () const {
		return get();
	}


	bool is_valid() const { return get() != nullptr; }
	T* get() const {
		if (handle == 0) 
			return nullptr;
		auto e = eng->get_object(handle);
		return e ? e->cast_to<T>() : nullptr;
	}
	T& operator*() const {
		return *get();
	}
	operator bool() const {
		return is_valid();
	}
	T* operator->() const {
		return get();
	}

	bool operator==(const obj<T>& other) {
		return handle == other.handle;
	}

	bool operator!=(const obj<T>& other) {
		return handle != other.handle;
	}

	uint64_t handle = 0;
};

using EntityPtr = obj<Entity>;
