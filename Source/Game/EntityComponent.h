#pragma once
#include "Game/BaseUpdater.h"
#include "Framework/StringName.h"
#include "Framework/Reflection2.h"
#include "glm/glm.hpp"
#include "glm/gtx/quaternion.hpp"

GENERATED_CLASS_INCLUDE("Game/Entity.h");

struct PropertyInfoList;
class Entity;
NEWCLASS(EntityComponent, BaseUpdater)
public:
	const static bool CreateDefaultObject = true;

	virtual ~EntityComponent() override;

	virtual void pre_start() {}
	virtual void start() {}
	virtual void update() {}
	virtual void end() {}

	void init_updater();
	void shutdown_updater();
	void set_ticking(bool shouldTick);
	void set_call_init_in_editor(bool b) {
		call_init_in_editor = b;
	}
	bool get_call_init_in_editor() const {
		return call_init_in_editor;
	}

	void destroy();

	REFLECT(name="owner",getter);
	Entity* get_owner() const { return entity_owner; }

	bool dont_serialize_or_edit_this() const { return dont_serialize_or_edit; }

	const glm::mat4& get_ws_transform();
	glm::vec3 get_ws_position() {
		return get_ws_transform()[3];
	}

	// helper function which calls eng->get_level()->add_to_sync_render_data_list(this)
	void sync_render_data();


#ifdef EDITOR_BUILD
	virtual const char* get_editor_outliner_icon() const { return ""; }
#endif
protected:
	// called when this components world space transform is changed (ie directly changed or a parents one was changed)
	virtual void on_changed_transform() {}
	// called when syncing data to renderer
	virtual void on_sync_render_data() {}

#ifndef NO_EDITOR
	// compile any data relevant to the node
	virtual bool editor_compile() { return true; }
	virtual void editor_on_change_property() {}
	bool editor_is_selected = false;
	bool editor_is_editor_only = false;	// set in CTOR
#endif

private:

	REFLECT(name = "is_type");
	bool is_type_for_script(const ClassTypeInfo* t) {
		if (!t) return false;
		return get_type().is_a(*t);
	}
	REFLECT(name = "type", getter);
	const ClassTypeInfo* get_type_for_script() {
		return &get_type();
	}


	void set_owner(Entity* e) {
		ASSERT(entity_owner == nullptr);
		entity_owner = e;
	}

	void activate_internal_step1();
	void activate_internal_step2();
	void deactivate_internal();

	void initialize_internal_step1();
	void initialize_internal_step2();
	void destroy_internal();

	Entity* entity_owner = nullptr;

	friend class Schema;
	friend class Entity;
	friend class EdPropertyGrid;
	friend class LevelSerialization;
	friend class Level;

	bool call_init_in_editor = false;
	bool tick_enabled = false;

public:
};