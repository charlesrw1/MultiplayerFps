#pragma once
#include "Framework/ClassBase.h"
#include "Framework/ReflectionProp.h"
#include "Framework/ReflectionMacros.h"

#include "glm/glm.hpp"
#include "glm/gtx/quaternion.hpp"

#include "Game/SerializePtrHelpers.h"

class Entity;
CLASS_H(EntityComponent, ClassBase)
public:
	const static bool CreateDefaultObject = true;

	virtual ~EntityComponent() {}

	// callbacks
	// called after component had properties unserialized
	// use to get handles, setup state
	virtual void on_init() {}
	// called when component is being removed, remove all handles
	virtual void on_deinit() {}

	// called when this components world space transform is changed (ie directly changed or a parents one was changed)
	virtual void on_changed_transform() {}

	// components are ticked every frame (todo)
	virtual void on_tick() {}

	void set_owner(Entity* owner) { entity_owner = owner; }
	Entity* get_owner() { return entity_owner; }

	// destruction functions
	void unlink_and_destroy();
	void destroy_children_no_unlink();
	void attach_to_parent(EntityComponent* parent_component, StringName point);
	void remove_this(EntityComponent* component);

	static PropertyInfo generate_prop_info(const char* name, uint16_t offset, uint32_t flags, const char* hint_str = "") {
		PropertyInfo pi;
		pi.name = name;
		pi.offset = offset;
		pi.custom_type_str = "EntityComponent";
		pi.flags = flags;
		pi.type = core_type_id::Struct;
		pi.range_hint = hint_str;
		return pi;
	}


#ifndef NO_EDITOR
	// compile any data relevant to the node
	virtual bool editor_compile() { return true; }
	virtual void editor_on_change_property(const PropertyInfo& property_) {}
	bool editor_is_selected = false;
	bool editor_is_editor_only = false;	// set in CTOR
#endif

	static const PropertyInfoList* get_props() {
		START_PROPS(EntityComponent)
			REG_VEC3(position, PROP_DEFAULT),
			REG_QUAT(rotation, PROP_DEFAULT),
			REG_VEC3(scale, PROP_DEFAULT),

			REG_ENTITY_COMPONENT_PTR(attached_parent, PROP_SERIALIZE ),
			REG_STDSTRING(eSelfNameString, PROP_DEFAULT | PROP_EDITOR_ONLY),
			REG_STDSTRING(eAttachedBoneName, PROP_DEFAULT | PROP_EDITOR_ONLY)
		END_PROPS(EntityComponent)
	}

	glm::mat4 get_local_transform();
	void set_local_transform(const glm::mat4& transform);
	void set_local_transform(const glm::vec3& v, const glm::quat& q, const glm::vec3& scale);
	void set_local_euler_rotation(const glm::vec3& euler);

	glm::mat4 get_world_transform();
	void set_world_transform(const glm::mat4& transform);

	bool get_is_native_component() const { return is_native_componenent; }

	StringName self_name;
#ifndef RUNTIME
	std::string eSelfNameString;
#endif // !RUNTIME
private:
	Entity* entity_owner = nullptr;

	ObjPtr<EntityComponent> attached_parent;

	StringName attached_bone_name;	// if non 0, determines
#ifndef RUNTIME
	std::string eAttachedBoneName;
#endif // !RUNTIME

	glm::vec3 position = glm::vec3(0.f);
	glm::quat rotation = glm::quat();
	glm::vec3 scale = glm::vec3(1.f);

	std::vector<EntityComponent*> children;


	std::vector<StringName> tags;

	bool is_native_componenent = true;
	bool is_editor_only = false;
	bool is_inherited = true;

	friend class Schema;
	friend class Entity;

	friend class EdPropertyGrid;
};

#define REG_COMPONENT(name, flags, hint) EntityComponent::generate_prop_info(\
#name, offsetof(TYPE_FROM_START,name), flags, hint)

