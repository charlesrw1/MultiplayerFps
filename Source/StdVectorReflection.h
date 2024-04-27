#pragma once
#include "ReflectionProp.h"

template<typename T>
class StdVectorCallback : public IListCallback
{
	using IListCallback::IListCallback;

	// Inherited via IListCallback
	virtual uint8_t* get_index(void* inst, int index) override
	{
		std::vector<T>* list = (std::vector<T>*)inst;
		return (uint8_t*)&list->at(index);
	}
	virtual uint32_t get_size(void* inst) override
	{
		std::vector<T>* list = (std::vector<T>*)inst;
		return list->size();
	}
	virtual void resize(void* inst, uint32_t new_size) override
	{
		std::vector<T>* list = (std::vector<T>*)inst;
		list->resize(new_size);
	}
	virtual void swap_elements(void* inst, int item0, int item1) override
	{
		std::vector<T>* list = (std::vector<T>*)inst;
		std::swap(list->at(item0), list->at(item1));
	}
};

#define MAKE_VECTORCALLBACK( type, name ) static StdVectorCallback<type> vecdef_##name( type::get_props() );
#define REG_STDVECTOR(name, flags ) make_list_property(#name, offsetof(TYPE_FROM_START, name), flags, &vecdef_##name)
#define REG_STDVECTOR_W_CUSTOM(name, flags, custom ) make_list_property(#name, offsetof(TYPE_FROM_START, name), flags, &vecdef_##name, custom)