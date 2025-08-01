#pragma once
#include "Game/BaseUpdater.h"
#include "Framework/StringName.h"
#include "Framework/Reflection2.h"
#include "glm/glm.hpp"
#include "glm/gtx/quaternion.hpp"


class IComponentEditorUi {
public:
	virtual ~IComponentEditorUi() {}
	// return true if mouse is grabbed
	virtual bool draw() { return false; }
};
struct PropertyInfoList;
class Entity;
class Component : public BaseUpdater
{
public:
	CLASS_BODY(Component, scriptable);
	const static bool CreateDefaultObject = true;
	virtual ~Component() override;
	// ClassBase override
	void serialize(Serializer& s) override;
	REF virtual void pre_start() {}
	REF virtual void start() {}
	REF virtual void update() {}
	REF virtual void stop() {}
	void init_updater();
	void shutdown_updater();
	REF void set_ticking(bool shouldTick);
	void set_call_init_in_editor(bool b) { call_init_in_editor = b; }
	bool get_call_init_in_editor() const { return call_init_in_editor; }
	REF void destroy();
	REF Entity* get_owner() const { return entity_owner; }
	const glm::mat4& get_ws_transform();
	glm::vec3 get_ws_position() { return get_ws_transform()[3]; }
	// helper function which calls eng->get_level()->add_to_sync_render_data_list(this)
	void sync_render_data();
	void set_owner_dont_serialize_or_edit(bool b);
#ifdef EDITOR_BUILD
	virtual const char* get_editor_outliner_icon() const { return ""; }
	virtual std::unique_ptr<IComponentEditorUi> create_editor_ui() { return nullptr; }
#endif
protected:
	// called when this components world space transform is changed (ie directly changed or a parents one was changed)
	virtual void on_changed_transform() {}
	// called when syncing data to renderer
	virtual void on_sync_render_data() {}
#ifdef EDITOR_BUILD
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

	void activate_internal_step1();
	void activate_internal_step2();
	void deactivate_internal();
	void initialize_internal_step1();
	void initialize_internal_step2();
	void destroy_internal();

	Entity* entity_owner = nullptr;
	bool call_init_in_editor = false;
	bool tick_enabled = false;

	friend class Schema;
	friend class Entity;
	friend class EdPropertyGrid;
	friend class LevelSerialization;
	friend class Level;
};