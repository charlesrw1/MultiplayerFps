#pragma once
#include "Framework/ReflectionProp.h"
#include "Framework/InlineVec.h"

#include <string>
#include <vector>

template <typename T> class StdVectorCallback : public IListCallback
{
public:
	using IListCallback::IListCallback;

	// Inherited via IListCallback
	virtual uint8_t* get_index(void* inst, int index) override {
		std::vector<T>* list = (std::vector<T>*)inst;
		ASSERT(index >= 0 && index < (int)list->size());
		return (uint8_t*)&(*list)[index];
	}
	virtual int get_size(void* inst) override {
		std::vector<T>* list = (std::vector<T>*)inst;
		return list->size();
	}
	virtual void resize(void* inst, int new_size) override {
		std::vector<T>* list = (std::vector<T>*)inst;
		list->resize((size_t)new_size);
	}
	virtual void swap_elements(void* inst, int item0, int item1) override {
		std::vector<T>* list = (std::vector<T>*)inst;
		ASSERT(item0 >= 0 && item0 < (int)list->size() && item1 >= 0 && item1 < (int)list->size());
		std::swap((*list)[item0], (*list)[item1]);
	}
};

template <typename T, uint32_t SIZE> class InlineVectorCallback : public IListCallback
{
public:
	using IListCallback::IListCallback;

	// Inherited via IListCallback
	virtual uint8_t* get_index(void* inst, int index) override {
		InlineVec<T, SIZE>* list = (InlineVec<T, SIZE>*)inst;
		return (uint8_t*)&(*list)[index];
	}
	virtual int get_size(void* inst) override {
		InlineVec<T, SIZE>* list = (InlineVec<T, SIZE>*)inst;
		return (int)list->size();
	}
	virtual void resize(void* inst, int new_size) override {
		InlineVec<T, SIZE>* list = (InlineVec<T, SIZE>*)inst;
		list->resize((size_t)new_size);
	}
	virtual void swap_elements(void* inst, int item0, int item1) override {
		InlineVec<T, SIZE>* list = (InlineVec<T, SIZE>*)inst;
		std::swap((*list)[item0], (*list)[item1]);
	}
};

#define REG_STDVECTOR(name, flags) make_list_property(#name, offsetof(TYPE_FROM_START, name), flags, &vecdef_##name)
