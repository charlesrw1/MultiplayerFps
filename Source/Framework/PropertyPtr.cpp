#include "PropertyPtr.h"
#include "StructReflection.h"

ArrayPropPtr::ArrayPropPtr(const PropertyInfo* info, void* ptr)
{
	assert(info->type == core_type_id::List);
	assert(info->list_ptr);
	assert(info->list_ptr->get_is_new_list_type());
	this->property = info;
	this->instance = ptr;
}

PropertyPtr ArrayPropPtr::get_array_index(int index)
{
	auto data = property->list_ptr->get_index(get_ptr(), index);
	return PropertyPtr(get_array_template_property(), data);
}

int ArrayPropPtr::get_array_size()
{
	return property->list_ptr->get_size(get_ptr());
}

void ArrayPropPtr::resize_array(int newsize)
{
	property->list_ptr->resize(get_ptr(), newsize);
}
ArrayPropPtr::Iterator ArrayPropPtr::begin() {
	return Iterator(*this);
}
ArrayPropPtr::Iterator ArrayPropPtr::end() {
	return Iterator(*this);
}
ArrayPropPtr::Iterator::Iterator(ArrayPropPtr& p) :owner(p){
	count = p.get_array_size();
}
bool ArrayPropPtr::Iterator::operator!=(const ArrayPropPtr::Iterator& other) {
	return index < count;
}
ArrayPropPtr::Iterator& ArrayPropPtr::Iterator::operator++() {
	index++;
	return *this;
}
PropertyPtr ArrayPropPtr::Iterator::operator*() {
	return owner.get_array_index(index);
}
ArrayPropPtr PropertyPtr::as_array()
{
	assert(is_array());
	return ArrayPropPtr(property,instance);
}
StructPropPtr PropertyPtr::as_struct()
{
	assert(is_struct());
	return StructPropPtr(property, instance);
}

StructPropPtr::StructPropPtr(const PropertyInfo* property, void* ptr)
{
	this->property = property;
	this->instance = ptr;
}

void StructPropPtr::call_serialize(Serializer& s)
{
	auto serializer = property->struct_type->custom_serialize;
	if (serializer) {
		serializer(instance, s);
	}
}

StructPropPtr::Iterator StructPropPtr::begin()
{
	return Iterator(*this);
}

StructPropPtr::Iterator StructPropPtr::end()
{
	return Iterator(*this);
}
StructPropPtr::Iterator::Iterator(StructPropPtr& self):self(self)
{

}
bool StructPropPtr::Iterator::operator!=(const StructPropPtr::Iterator& other) {
	if (!self.property->struct_type->properties)
		return false;
	return index < self.properties().count;
}
StructPropPtr::Iterator& StructPropPtr::Iterator::operator++() {
	index++;
	return *this;
}
PropertyPtr StructPropPtr::Iterator::operator*() {
	return PropertyPtr(&self.properties().list[index], self.get_ptr());
}

bool ClassPropPtr::Iterator::operator!=(const ClassPropPtr::Iterator& other)  {
	return info != nullptr;
}
ClassPropPtr::Iterator& ClassPropPtr::Iterator::operator++() {
	advance();
	return *this;
}
PropertyPtr ClassPropPtr::Iterator::operator*()  {
	assert(info);
	auto props = info->props;
	assert(index < props->count);
	return PropertyPtr(&props->list[index], obj);
}

ClassPropPtr::Iterator ClassPropPtr::begin()  {
	if (!obj) {
		assert(ti);
		return Iterator(ti);
	}
	else {
		return Iterator(obj);
	}
}
ClassPropPtr::Iterator ClassPropPtr::end()  {
	return Iterator();
}
ClassPropPtr::Iterator::Iterator(ClassBase* obj)
	: obj(obj)
{
	info = &obj->get_type();
	advance();
}
ClassPropPtr::Iterator::Iterator(const ClassTypeInfo* ti)
	: obj(nullptr)
{
	info = ti;
	advance();
}

ClassPropPtr::Iterator::Iterator()
	{}

void ClassPropPtr::Iterator::advance() {
	++index;
	while (true) {
		if (!info)
			return;
		auto props = info->props;
		if (!props) {
			info = info->super_typeinfo;
			index = 0;
			continue;
		}
		if (index >= props->count) {
			info = info->super_typeinfo;
			index = 0;
			continue;
		}
		break;
	}
}


bool PropertyPtr::is_string() const
{
	return core_type_id::StdString == get_type();
}

std::string& PropertyPtr::as_string()
{
	assert(is_string());
	return *(std::string*)get_ptr();
}

bool PropertyPtr::is_float() const
{
	return core_type_id::Float == get_type();
}

float& PropertyPtr::as_float()
{
	assert(is_float());
	return *(float*)get_ptr();
}

bool PropertyPtr::is_numeric() const
{
	return property->is_integral_type();
}

int64_t PropertyPtr::get_integer_casted()
{
	return property->get_int(instance);
}

void PropertyPtr::set_integer_casted(int64_t i)
{
	property->set_int(instance,i);
}

bool PropertyPtr::is_boolean() const
{
	return core_type_id::Bool == get_type();
}

bool& PropertyPtr::as_boolean()
{
	assert(is_boolean());
	return *(bool*)get_ptr();
}

bool PropertyPtr::is_enum() const
{
	return core_type_id::Enum8==get_type()||core_type_id::Enum16==get_type()||core_type_id::Enum32==get_type();
}

EnumPropPtr PropertyPtr::as_enum()
{
	return EnumPropPtr();
}

bool PropertyPtr::is_vec3() const {
	return core_type_id::Vec3 == get_type();
}
glm::vec3& PropertyPtr::as_vec3() {
	assert(is_vec3());
	return *(glm::vec3*)get_ptr();
}
bool PropertyPtr::is_vec2() const {
	return core_type_id::Vec2 == get_type();
}
glm::vec2& PropertyPtr::as_vec2() {
	assert(is_vec2());
	return *(glm::vec2*)get_ptr();
}
bool PropertyPtr::is_quat() const {
	return core_type_id::Quat == get_type();
}
glm::quat& PropertyPtr::as_quat() {
	assert(is_quat());
	return *(glm::quat*)get_ptr();
}