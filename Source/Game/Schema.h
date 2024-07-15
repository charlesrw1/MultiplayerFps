#pragma once
#include "Entity.h"

// A class that is serizlied along with Entities and Components
// its used to allow pointers to member fields and not standalone objects
// sort of hacky
CLASS_H(InlinePtrFixup, ClassBase)
public:
	int object_index = 0;
	std::string property_name;
	static const PropertyInfoList* get_props() {
		START_PROPS(InlinePtrFixup) 
			REG_INT(object_index, PROP_SERIALIZE,""),
			REG_STDSTRING(property_name, PROP_SERIALIZE) END_PROPS(InlinePtrFixup)
	}
};

class SchemaLoader
{
public:
	static SchemaLoader& get() {
		static SchemaLoader inst;
		return inst;
	}
	Entity* create_but_dont_instantiate_schema(const std::string& schemafile);
private:
	std::unordered_map<std::string, std::string> cached_files;
};