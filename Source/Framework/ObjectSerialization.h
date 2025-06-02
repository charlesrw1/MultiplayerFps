#pragma once
#include "Framework/ReflectionProp.h"
#include "Framework/Factory.h"
#include "Framework/DictWriter.h"
#include "Framework/DictParser.h"
#include "Framework/ClassBase.h"
#include <string>
#include <cassert>

inline void copy_object_properties( ClassBase* from, ClassBase* to, ClassBase* userptr, IAssetLoadingInterface* load)
{
	assert(from->get_type() == to->get_type());

	std::vector<const PropertyInfoList*> props;
	const ClassTypeInfo* typeinfo = &from->get_type();
	while (typeinfo) {
		if (typeinfo->props)
			props.push_back(typeinfo->props);
		typeinfo = typeinfo->super_typeinfo;
	}
	copy_properties(props, from, to, userptr,load);
}

inline void write_object_properties(
	ClassBase* obj,
	ClassBase* userptr,
	DictWriter& out
)
{
	if (!obj) {
		out.write_item_start();
		out.write_item_end();
		return;
	}

	std::vector<PropertyListInstancePair> props;
	const ClassTypeInfo* typeinfo = &obj->get_type();
	while (typeinfo) {
		if (typeinfo->props)
			props.push_back({ typeinfo->props, obj });
		typeinfo = typeinfo->super_typeinfo;
	}

	typeinfo = &obj->get_type();

	out.write_item_start();

	out.write_key_value("type", typeinfo->classname);
	for (auto& proplist : props) {
		if(proplist.list)
			write_properties(*const_cast<PropertyInfoList*>(proplist.list), proplist.instance, out, userptr);
	}

	out.write_item_end();
}


template<typename BASE>
inline BASE* read_object_properties(
	ClassBase* userptr,
	DictParser& in,
	StringView tok, IAssetLoadingInterface* load
)
{
	if (!in.check_item_start(tok))
		return nullptr;
	in.read_string(tok);
	if (!tok.cmp("type"))	// if the item is SUPPOSED to be nullptr, then that occurs here (as it will be a cmp with '}')
		return nullptr;
	in.read_string(tok);
	BASE* obj = ClassBase::create_class<BASE>(tok.to_stack_string().c_str());
	if (!obj)
		return nullptr;

	std::vector<PropertyListInstancePair> props;
	const ClassTypeInfo* typeinfo = &obj->get_type();
	while (typeinfo) {
		if (typeinfo->props)
			props.push_back({ typeinfo->props, obj });
		typeinfo = typeinfo->super_typeinfo;
	}

	auto ret = read_multi_properties(props, in, {}, userptr,load);
	tok = ret.first;

	if (!ret.second || !in.check_item_end(tok)) {
		delete obj;
		return nullptr;
	}
	return obj;
}

template<typename BASE>
inline BASE* read_object_properties_no_input_tok(
	ClassBase* userptr,
	DictParser& in,
	IAssetLoadingInterface* load)
{
	StringView tok;
	in.read_string(tok);
	return read_object_properties<BASE>(userptr, in, tok,load);
}