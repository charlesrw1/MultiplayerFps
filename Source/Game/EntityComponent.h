#pragma once
#include "Framework/ClassBase.h"
#include "Framework/ReflectionProp.h"
#include "Framework/ReflectionMacros.h"

#include "glm/glm.hpp"
#include "glm/gtx/quaternion.hpp"

#include "Game/SerializePtrHelpers.h"

#include "Framework/StringName.h"
#include "Game/BaseUpdater.h"

class Entity;
CLASS_H(EntityComponent, BaseUpdater)
public:
	const static bool CreateDefaultObject = true;

	virtual ~EntityComponent() override;

	void destroy();

	Entity* get_owner() const { return entity_owner; }

	bool get_is_native_component() const { return is_native_componenent; }

	bool dont_serialize_or_edit_this() const { return dont_serialize_or_edit; }

	const glm::mat4& get_ws_transform();
	glm::vec3 get_ws_position() {
		return get_ws_transform()[3];
	}

	static const PropertyInfoList* get_props() = delete;
protected:
	// called when this components world space transform is changed (ie directly changed or a parents one was changed)
	virtual void on_changed_transform() {}

#ifndef NO_EDITOR
	// compile any data relevant to the node
	virtual bool editor_compile() { return true; }
	virtual void editor_on_change_property() {}
	bool editor_is_selected = false;
	bool editor_is_editor_only = false;	// set in CTOR
#endif

private:

	void set_owner(Entity* e) {
		ASSERT(entity_owner == nullptr);
		entity_owner = e;
	}

	void initialize_internal();

	void destroy_internal();

	Entity* entity_owner = nullptr;

	bool is_native_componenent = true;
	bool is_editor_only = false;
	bool is_inherited = true;
	bool is_force_root = false;

	friend class Schema;
	friend class Entity;
	friend class EdPropertyGrid;
	friend class LevelSerialization;
	friend class Level;

public:
};