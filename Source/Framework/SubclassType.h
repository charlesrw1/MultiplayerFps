#pragma once
#include "ClassBase.h"
template<typename T>
class SubclassType
{
public:
	template<typename K>
	static SubclassType create() {
		static_assert(std::is_base_of<T, K>::value, "Subclasstype must derive from Base");
		SubclassType<T> out;
		out.type = &K::StaticType;
	}

	const ClassTypeInfo* type = nullptr;
private:
	SubclassClassType() {
	}
};