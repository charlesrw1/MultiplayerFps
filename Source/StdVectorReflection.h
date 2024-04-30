#pragma once
#include "ReflectionProp.h"
#include "InlineVec.h"
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

template<typename T, uint32_t SIZE>
class InlineVectorCallback : public IListCallback
{
public:
	InlineVectorCallback(PropertyInfoList* struct_) : IListCallback(struct_) {}

	// Inherited via IListCallback
	virtual uint8_t* get_index(void* inst, int index) override
	{
		InlineVec<T,SIZE>* list = (InlineVec<T, SIZE>*)inst;
		return (uint8_t*)&list[index];
	}
	virtual uint32_t get_size(void* inst) override
	{
		InlineVec<T, SIZE>* list = (InlineVec<T, SIZE>*)inst;
		return list->size();
	}
	virtual void resize(void* inst, uint32_t new_size) override
	{
		InlineVec<T, SIZE>* list = (InlineVec<T, SIZE>*)inst;
		list->resize(new_size);
	}
	virtual void swap_elements(void* inst, int item0, int item1) override
	{
		InlineVec<T, SIZE>* list = (InlineVec<T, SIZE>*)inst;
		std::swap((*list)[item0], (*list)[item1]);
	}
};

template<typename T, uint32_t COUNT>
inline InlineVectorCallback<T,COUNT> get_inlinevec_callback(InlineVec<T, COUNT>* abc, PropertyInfoList* struct_) {
	return InlineVectorCallback<T, COUNT>(struct_);
}

#define MAKE_VECTORCALLBACK( type, name ) static StdVectorCallback<type> vecdef_##name( type::get_props() );
#define REG_STDVECTOR(name, flags ) make_list_property(#name, offsetof(TYPE_FROM_START, name), flags, &vecdef_##name)
#define REG_STDVECTOR_W_CUSTOM(name, flags, custom ) make_list_property(#name, offsetof(TYPE_FROM_START, name), flags, &vecdef_##name, custom)
#define MAKE_INLVECTORCALLBACK( type, count, name, owner_type ) static auto vecdef_##name = get_inlinevec_callback( &((owner_type*)0)->name, type::get_props() );