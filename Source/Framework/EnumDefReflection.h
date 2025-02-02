#pragma once
#include <cstdint>
#include <string>
#include "Framework/Util.h"

struct EnumIntPair {
	EnumIntPair(const char* n, const char* d, int64_t v) 
		: name(n), display_name(d), value(v) {}
	EnumIntPair() {}
	const char* name = "";
	const char* display_name = "";
	int64_t value = 0;
};


struct EnumTypeInfo;
struct EnumIterator {
	EnumIterator(int start, const EnumTypeInfo* owner) : index(start),owner(owner) {}
	const EnumIntPair& operator*() const;
	EnumIterator& operator++() {
		index++;
		return *this;
	}
	bool operator!=(const EnumIterator& other) {
		return index != other.index;
	}
private:
	int index = 0;
	const EnumTypeInfo* owner = nullptr;
};

struct EnumTypeInfo {
	EnumTypeInfo(const char* name, const EnumIntPair* values, size_t count);

	EnumIterator begin() const {
		return EnumIterator(0, this);
	}
	EnumIterator end() const {
		return EnumIterator(str_count,this);
	}
	const EnumIntPair* find_for_value(int64_t value) const;


	const char* name = "";
	const EnumIntPair* strs = nullptr;
	int str_count = 0;
};
inline const EnumIntPair& EnumIterator::operator*() const {
	ASSERT(index < owner->str_count&& index >= 0);
	return owner->strs[index];
}


template<typename T>
struct EnumTrait;

#define NEWENUM(type, inttype) \
	enum class type : inttype; \
	template<> \
	struct EnumTrait<type> {\
		static EnumTypeInfo StaticType;\
	}; \
	enum class type : inttype

struct EnumFindResult
{
	const EnumTypeInfo* typeinfo = nullptr;	// what enum
	int64_t value = 0;	// the actual enum value
	int enum_idx = -1;	// the index into typeinfo->strs[]
};

class EnumRegistry
{
public:
	static void register_enum(const EnumTypeInfo* eti);
	// finds an enum type
	// ex: "MyEnum"
	static const EnumTypeInfo* find_enum_type(const char* str);
	static const EnumTypeInfo* find_enum_type(const std::string& str);
	// includes type name seperated by '::'
	// ex: "MyEnum::Red", "MyEnum::Green"
	static EnumFindResult find_enum_by_name(const char* enum_value_name);
	static EnumFindResult find_enum_by_name(const std::string& str);
};