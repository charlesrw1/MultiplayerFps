#pragma once
#include "Framework/ClassBase.h"
#include "Framework/ReflectionProp.h"
#include "Framework/ReflectionMacros.h"
#include "IAsset.h"
#include <string>
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

class Entity;
// Schemas are just serialized Entities
CLASS_H(Schema, IAsset)
public:
	Entity* create_entity_from_properties() {
		return create_entity_from_properties_internal(false);
	}

private:
	bool check_validity_of_file();

	Entity* create_entity_from_properties_internal(bool just_check_validity = false/* always call with false, only called with true in check_validity_of_file*/);

	friend class SchemaLoader;
	std::string properties;	// this is a text serialized form of an entity, from disk
};

class SchemaLoader : public IAssetLoader
{
public:
	virtual IAsset* load_asset(const std::string& file) {
		return load_schema(file);
	}

	Schema* load_schema(const std::string& file);
private:
	std::unordered_map<std::string, Schema*> cached_files;
};
extern SchemaLoader g_schema_loader;