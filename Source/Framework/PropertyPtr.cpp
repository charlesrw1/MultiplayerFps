#include "PropertyPtr.h"

ArrayPropPtr::ArrayPropPtr(const PropertyInfo* info, void* ptr)
{
	assert(info->type == core_type_id::List);
}

PropertyPtr ArrayPropPtr::get_array_index(int index)
{
	auto data = property->list_ptr->get_index(instance, index);
	return PropertyPtr(nullptr, nullptr);
}

int ArrayPropPtr::get_array_size()
{
	return 0;
}

void ArrayPropPtr::resize_array(int newsize)
{
}
ArrayPropPtr::Iterator ArrayPropPtr::begin() {
	return Iterator(property,instance);
}
ArrayPropPtr::Iterator ArrayPropPtr::end() {
	return Iterator();
}
ArrayPropPtr::Iterator::Iterator(const PropertyInfo* p, void* inst) :p(p),inst(inst){
	assert(p->type == core_type_id::List);
	count = p->list_ptr->get_size(inst);
}
ArrayPropPtr::Iterator::Iterator() {
}
bool ArrayPropPtr::Iterator::operator!=(const ArrayPropPtr::Iterator& other) {
	return index < count;
}
ArrayPropPtr::Iterator& ArrayPropPtr::Iterator::operator++() {
	index++;
	return *this;
}
PropertyPtr ArrayPropPtr::Iterator::operator*() {
	auto data = p->list_ptr->get_index(inst, index);
	return PropertyPtr(nullptr,nullptr);// p->list_ptr->props_in_list, data);
}
