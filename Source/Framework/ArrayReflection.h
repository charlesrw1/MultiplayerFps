#pragma once
#include "Framework/ReflectionProp.h"
#include "Framework/InlineVec.h"

#include <string>

template<typename T>
class StdVectorCallback : public IListCallback
{
public:
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
	using IListCallback::IListCallback;

	// Inherited via IListCallback
	virtual uint8_t* get_index(void* inst, int index) override
	{
		InlineVec<T,SIZE>* list = (InlineVec<T, SIZE>*)inst;
		return (uint8_t*)&(*list)[index];
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
template<typename T, uint32_t COUNT>
inline InlineVectorCallback<T, COUNT> get_inlinevec_callback(InlineVec<T, COUNT>* abc, PropertyInfo atom) {
	return InlineVectorCallback<T, COUNT>(atom);
}


template<typename T>
struct GetAtomValueWrapper {
	static PropertyInfo get() {
		static_assert(sizeof(T) == 0, "GetAtomValueWrapper is not specialized for this type.");
		return {};
	}
};
template<>
struct GetAtomValueWrapper<uint32_t> {
	static PropertyInfo get();
};
template<>
struct GetAtomValueWrapper<std::string> {
	static PropertyInfo get();
};
template<>
struct GetAtomValueWrapper<bool> {
	static PropertyInfo get();
};
template<>
struct GetAtomValueWrapper<float> {
	static PropertyInfo get();
};
template<>
struct GetAtomValueWrapper<int> {
	static PropertyInfo get();
};
template<>
struct GetAtomValueWrapper<uint16_t> {
	static PropertyInfo get();
};
template<typename T>
PropertyInfo get_atom_value() {
	return GetAtomValueWrapper<T>::get();
}

#define MAKE_VECTORCALLBACK( type, name ) static StdVectorCallback<type> vecdef_##name( type::get_props() );
#define MAKE_VECTORCALLBACK_ATOM( type, name ) static StdVectorCallback<type> vecdef_##name( get_atom_value<type>() );

#define REG_STDVECTOR(name, flags ) make_list_property(#name, offsetof(TYPE_FROM_START, name), flags, &vecdef_##name)
#define REG_STDVECTOR_W_CUSTOM(name, flags, custom ) make_list_property(#name, offsetof(TYPE_FROM_START, name), flags, &vecdef_##name, custom)
#define MAKE_INLVECTORCALLBACK( type, name, owner_type ) static auto vecdef_##name = get_inlinevec_callback( &((owner_type*)0)->name, type::get_props() );
#define MAKE_INLVECTORCALLBACK_TYPE( type_list, name, owner_type ) static auto vecdef_##name = get_inlinevec_callback( &((owner_type*)0)->name, type_list );
#define MAKE_INLVECTORCALLBACK_ATOM( type, name, owner_type ) static auto vecdef_##name = get_inlinevec_callback( &((owner_type*)0)->name, get_atom_value<type>() );
