#pragma once
#include "Framework/ClassBase.h"
#include "Framework/ReflectionProp.h"
#include "Framework/ReflectionMacros.h"
#include "IAsset.h"
#include <string>
#include <unordered_set>
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

// Schemas/Archtypes represent an Entity with defined properties
// Its essentially a prefab, but only 1 entity
// Supports inheritance
// All schemas start with an Entity class as a root
// Then you can define schemas that inherit from other schemas, etc.
// Schemas can change properties and add dynamic components
// To load them: calls recursivley up the inheritance tree
// If a schema inherits from schema B, create an entity from B, if B has class Player as a root, 
// then create Player and overwrite properties

class Entity;
CLASS_H(Schema, IAsset)
public:
	Entity* create_entity_from_properties() const {
		return create_entity_from_properties_internal(false);
	}

	// temp
	void write_to_file(Entity* ent);
private:
	bool check_validity_of_file();

	Entity* create_entity_from_properties_internal(
		bool just_check_validity = false/* always call with false, only called with true in check_validity_of_file*/) const;

	friend class SchemaLoader;

	Entity* default_schema_obj = nullptr;	// used for diffing
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