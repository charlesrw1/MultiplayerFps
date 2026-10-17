#include "PropertyPtr.h"

bool ClassPropPtr::Iterator::operator!=(const ClassPropPtr::Iterator& other) {
	return info != nullptr;
}
ClassPropPtr::Iterator& ClassPropPtr::Iterator::operator++() {
	advance();
	return *this;
}
PropertyPtr ClassPropPtr::Iterator::operator*() {
	assert(info);
	auto props = info->props;
	assert(index < props->count);
	return PropertyPtr(&props->list[index], obj);
}

ClassPropPtr::Iterator ClassPropPtr::begin() {
	if (!obj) {
		assert(ti);
		return Iterator(ti);
	} else {
		return Iterator(obj);
	}
}
ClassPropPtr::Iterator ClassPropPtr::end() {
	return Iterator();
}
ClassPropPtr::Iterator::Iterator(ClassBase* obj) : obj(obj) {
	info = &obj->get_type();
	advance();
}
ClassPropPtr::Iterator::Iterator(const ClassTypeInfo* ti) : obj(nullptr) {
	info = ti;
	advance();
}

ClassPropPtr::Iterator::Iterator() {}

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
