#pragma once
#include "Framework/ReflectionProp.h"
#include "Framework/Factory.h"
#include "Framework/DictWriter.h"
#include "Framework/DictParser.h"
#include <string>


template<typename BASE, typename PROPGETTER>
inline void write_object_properties(
	BASE* obj,
	TypedVoidPtr userptr,
	DictWriter& out
)
{
	std::vector<PropertyListInstancePair> props;
	PROPGETTER::get(props, obj);

	out.write_item_start();

	out.write_key_value("type", obj->get_typeinfo().name);
	for (auto& proplist : props) {
		if(proplist.list)
			write_properties(*proplist.list, proplist.instance, out, userptr);
	}

	out.write_item_end();
}

template<typename BASE, typename PROPGETTER>
inline BASE* read_object_properties(
	Factory<std::string,BASE>& factory,
	TypedVoidPtr userptr,
	DictParser& in,
	StringView tok
)
{
	if (!in.check_item_start(tok))
		return nullptr;
	in.read_string(tok);
	if (!tok.cmp("type"))
		return nullptr;
	in.read_string(tok);
	BASE* obj = factory.createObject(tok.to_stack_string().c_str());
	if (!obj)
		return nullptr;
	std::vector<PropertyListInstancePair> props;
	PROPGETTER::get(props, obj);

	auto ret = read_multi_properties(props, in, {}, userptr);
	tok = ret.first;

	if (!ret.second || !in.check_item_end(tok)) {
		delete obj;
		return nullptr;
	}
	return obj;
}