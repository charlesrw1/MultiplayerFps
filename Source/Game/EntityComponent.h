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

	virtual ~EntityComponent() {}

	void init();
	void deinit();

	// callbacks
	// called after component had properties unserialized
	// use to get handles, setup state
	virtual void on_init() {}
	// called when component is being removed, remove all handles
	virtual void on_deinit() {}

	// called when this components world space transform is changed (ie directly changed or a parents one was changed)
	virtual void on_changed_transform() {}

	// components are ticked every frame (todo)
	virtual void update() override {}

	// physics functions, only used if the item is adding itself to the physics system
	// kinda an OOP bubble up issue but whatev


	void set_owner(Entity* owner) { entity_owner = owner; }
	Entity* get_owner() const { return entity_owner; }

	// destruction functions
	void attach_to_parent(EntityComponent* parent_component, StringName point = StringName());
	void remove_this(EntityComponent* child_component);	// not destructuve, better name is unlink_this()
	void unlink_from_parent();
	void post_unserialize_created_component(Entity* owner_ent);

#ifndef NO_EDITOR
	// compile any data relevant to the node
	virtual bool editor_compile() { return true; }
	virtual void editor_on_change_property() {}
	bool editor_is_selected = false;
	bool editor_is_editor_only = false;	// set in CTOR
#endif

	static const PropertyInfoList* get_props() {
		START_PROPS(EntityComponent)
			REG_VEC3(position, PROP_DEFAULT),
			REG_QUAT(rotation, PROP_DEFAULT),
			REG_VEC3(scale, PROP_DEFAULT),
			REG_BOOL(is_force_root, PROP_SERIALIZE, "0"),
			REG_ENTITY_COMPONENT_PTR(attached_parent, PROP_SERIALIZE ),
			REG_STDSTRING(eSelfNameString, PROP_DEFAULT | PROP_EDITOR_ONLY),
			REG_STDSTRING(eAttachedBoneName, PROP_DEFAULT | PROP_EDITOR_ONLY)
		END_PROPS(EntityComponent)
	}

	// ws = world space, ls = local space
	glm::mat4 get_ls_transform() const;
	void set_ls_transform(const glm::mat4& transform);
	void set_ls_transform(const glm::vec3& v, const glm::quat& q, const glm::vec3& scale);
	void set_ls_euler_rotation(const glm::vec3& euler);
	const glm::mat4& get_ws_transform();
	glm::vec3 get_ls_position() const { return position; }
	glm::vec3 get_ls_scale() const { return scale; }
	glm::quat get_ls_rotation() const { return rotation; }
	glm::vec3 get_ws_position()  { return get_ws_transform()[3]; }
	void set_ws_transform(const glm::mat4& transform);
	const EntityComponent* get_parent_component() const {
		return attached_parent.get();
	}


	bool get_is_native_component() const { return is_native_componenent; }

#ifndef RUNTIME
#endif // !RUNTIME
	void post_change_transform_R(bool ws_is_dirty = true);

	std::string eSelfNameString;
	std::string eAttachedBoneName;

	bool dont_serialize_or_edit_this() const { return dont_serialize_or_edit; }
private:

	glm::vec3 position = glm::vec3(0.f);
	glm::quat rotation = glm::quat();
	glm::vec3 scale = glm::vec3(1.f);

	glm::mat4 cached_world_transform = glm::mat4(1);

	Entity* entity_owner = nullptr;
	ObjPtr<EntityComponent> attached_parent;
	std::vector<EntityComponent*> children;
	std::vector<StringName> tags;

	bool world_transform_is_dirty = true;
	bool is_native_componenent = true;
	bool is_editor_only = false;
	bool is_inherited = true;
	bool is_force_root = false;

	friend class Schema;
	friend class Entity;
	friend class EdPropertyGrid;
	friend class LevelSerialization;

public:
	// expose this publically so you can set this in constructors or wherever
	bool dont_serialize_or_edit = false;	// if true, then this component (and its children) wont be serialized or be editable in the editor
};

CLASS_H(EmptyComponent, EntityComponent)
public:
	~EmptyComponent() override {}

	static const PropertyInfoList* get_props() = delete;
};
