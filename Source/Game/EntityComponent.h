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

private:

	// callbacks
	// called after component had properties unserialized
	// use to get handles, setup state
	virtual void on_init() {}

	// called when component is being removed, remove all handles
	virtual void on_deinit() {}

public:
	// called when this components world space transform is changed (ie directly changed or a parents one was changed)
	virtual void on_changed_transform() {}

	// components are ticked every frame (todo)
	virtual void update() override {}

	// entity owner of this component
	Entity* get_owner() const { return entity_owner; }


#ifndef NO_EDITOR
	// compile any data relevant to the node
	virtual bool editor_compile() { return true; }
	virtual void editor_on_change_property() {}
	bool editor_is_selected = false;
	bool editor_is_editor_only = false;	// set in CTOR
#endif

	static const PropertyInfoList* get_props() = delete;

	bool get_is_native_component() const { return is_native_componenent; }

	bool dont_serialize_or_edit_this() const { return dont_serialize_or_edit; }

	const glm::mat4& get_ws_transform();
	glm::vec3 get_ws_position() {
		return get_ws_transform()[3];
	}

protected:

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

CLASS_H(EmptyComponent, EntityComponent)
public:
	~EmptyComponent() override {}

	static const PropertyInfoList* get_props() = delete;
};
