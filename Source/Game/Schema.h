#pragma once
#include "Framework/ClassBase.h"
#include "Framework/ReflectionProp.h"
#include "Framework/ReflectionMacros.h"
#include "Assets/IAsset.h"
#include <string>
#include <unordered_set>

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
		return create_entity_from_properties_internal();
	}

	const Entity* get_default_schema_obj() const { return default_schema_obj; }

	// IAsset overrides
	void move_construct(IAsset* o) {
		auto other = (Schema*)o;
		delete default_schema_obj;
		default_schema_obj = other->default_schema_obj;
		properties = std::move(other->properties);
	}
	bool load_asset(ClassBase*& user);
	void post_load(ClassBase*) {}
	void uninstall() override {
		delete default_schema_obj;
		default_schema_obj = nullptr;
	}
	void sweep_references() const override;
private:

	Entity* create_entity_from_properties_internal() const;

	Entity* default_schema_obj = nullptr;	// used for diffing
	std::string properties;	// this is a text serialized form of an entity, from disk

	friend class SchemaLoadJob;
};
