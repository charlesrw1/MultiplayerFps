#pragma once
#include "Game/BaseUpdater.h"
#include "Framework/StringName.h"
#include "Framework/Reflection2.h"
#include "glm/glm.hpp"
#include "glm/gtx/quaternion.hpp"

class IComponentEditorUi
{
public:
	virtual ~IComponentEditorUi() {}
	// return true if mouse is grabbed
	virtual bool draw() { return false; }
};
struct PropertyInfoList;
class Entity;
class Model;
class LuaClassTypeInfo;
class Component : public BaseUpdater
{
public:
	CLASS_BODY(Component, scriptable);
	const static bool CreateDefaultObject = true;
	virtual ~Component() override;
	// ClassBase override
	void serialize(Serializer& s) override;
	// called when start, only called in editor when set_call_init_in_editor(true) in ctor
	REF virtual void start() {}
	// called when set_ticking(true)
	REF virtual void update() {}
	// called when destroyed (if start() was called)
	REF virtual void stop() {}
	// always called for every component in editor
	REF virtual void editor_start() {}

	// callbacks for stuff. func called on every sibling component of caller.
	REF virtual void on_collider_trigger(Entity* other, bool entered) {}

	// Called on every sibling component when this entity's physics body collides with
	// another in the simulation. Requires set_send_hit(true)
	// on the PhysicsBody. `other` is the entity that was hit; `position` is the
	// world-space contact point; `normal` is the world-space contact normal pointing
	// toward `other`; `impulse` is the applied normal-impulse magnitude (0 if unknown).
	REF virtual void on_collider_hit(Entity* other, glm::vec3 position, glm::vec3 normal, float impulse) {}


	REF void set_ticking(bool shouldTick);
	REF void destroy();

	void init_updater();
	void shutdown_updater();
	void set_call_init_in_editor(bool b) { call_init_in_editor = b; }
	bool get_call_init_in_editor() const { return call_init_in_editor; }

	// Invoked by the scene walk in Model::post_load (reload path).  Default no-op.
	// Override if this component caches anything derived from a Model's contents
	// (bone indices, retarget maps, mesh-derived data) that becomes invalid when
	// the named model is hot-reloaded.  Implementations must NOT subscribe to a
	// delegate — the scene walk drives the call.
	virtual void refresh_after_model_reload(Model* reloaded) {}
	REFLECT(no_nil)
	Entity* get_owner() const { return entity_owner; }
	const glm::mat4& get_ws_transform();
	glm::vec3 get_ws_position() { return get_ws_transform()[3]; }
	// helper function which calls eng->get_level()->add_to_sync_render_data_list(this)
	void sync_render_data();
	REF void set_owner_dont_serialize_or_edit(bool b);
#ifdef EDITOR_BUILD
	virtual const char* get_editor_outliner_icon() const { return ""; }
	virtual std::unique_ptr<IComponentEditorUi> create_editor_ui() { return nullptr; }
	virtual void editor_on_draw_gizmos_selected() {}
	REF virtual void editor_on_change_property() {}
#endif
protected:
	// called when this components world space transform is changed (ie directly changed or a parents one was changed)
	virtual void on_changed_transform() {}
	// called when syncing data to renderer
	virtual void on_sync_render_data() {}
#ifdef EDITOR_BUILD
	bool editor_is_selected = false;
	bool editor_is_editor_only = false; // set in CTOR
#endif

private:
	std::unique_ptr<uint8_t[]> lua_field_shadow;
	void set_owner(Entity* e) {
		ASSERT(entity_owner == nullptr);
		entity_owner = e;
	}

	void activate_internal_step2();
	void deactivate_internal();

	void destroy_internal();

public:
	// Per-instance byte buffer that backs PROP_LUA_BACKED fields for Lua-defined
	// Component subclasses. Set once by LuaClassTypeInfo::lua_class_alloc when
	// instantiating a scriptable Component; left null for plain C++ components.
	// Layout (offsets, sizes) is owned by lua_owner_type.
	//
	// lua_owner_type is captured at alloc time and used during ~Component to drive
	// cleanup, because get_type() inside a base-class dtor returns Component's
	// StaticType (virtual dispatch is downgraded as derived dtors finish), making
	// the live-instance unregister unreachable otherwise.
	uint8_t* get_lua_field_shadow() const override { return lua_field_shadow.get(); }
	void ensure_lua_shadow() override;
	void take_lua_field_shadow(std::unique_ptr<uint8_t[]> buf) { lua_field_shadow = std::move(buf); }
	void set_lua_owner_type(const LuaClassTypeInfo* t) { lua_owner_type = t; }
	const LuaClassTypeInfo* get_lua_owner_type() const { return lua_owner_type; }

	bool get_draw_text_in_editor() const { return draw_text_in_editor; }
	void editor_set_model(std::string_view modelname, bool draw_text = false);
	void editor_set_billboard(std::string_view billboard_texture, bool draw_text = false, float scale = 1.f);
private:
	const LuaClassTypeInfo* lua_owner_type = nullptr;
	Entity* entity_owner = nullptr;
	bool call_init_in_editor = false;
	bool tick_enabled = false;
	bool draw_text_in_editor = false;

	friend class Entity;
	friend class EdPropertyGrid;
	friend class LevelSerialization;
	friend class Level;
};