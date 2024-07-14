#pragma once

#include <cstdint>
#include "glm/glm.hpp"
#include "RenderObj.h"

#include "BaseComponents.h"
#include "Interaction.h"

#include "Framework/TypeInfo.h"
#include "Framework/AddClassToFactory.h"
#include "Framework/StringName.h"
#include "Framework/ClassBase.h"
#include "Framework/ReflectionRegisterDefines.h"
#include "Framework/ReflectionProp.h"

class Model;
class GeomContact;


struct EntityFlags
{
	enum Enum
	{
		Dead = 1,
		Hidden = 2,
	};
};


typedef uint32_t entityhandle;


struct UpdateFlags
{
	enum Enum : uint8_t
	{
		UPDATE = 1,	// pre physics
		PRESENT = 2,	// post physics
	};
};

class PhysicsActor;
class Dict;


// EntityComponents are used for shared data/logic between Entities
// such as Meshes, Physics, Sounds, Fx, etc.
// is ordered in a heirarchy
class Entity;
CLASS_H(EntityComponent, ClassBase)
public:
	virtual ~EntityComponent() {}

	void destroy();

	virtual void tick() {}

	template<typename T>
	static PropertyInfo generate_prop_info(T* dummyptr, const char* name, uint16_t offset, uint32_t flags, const char* hint_str="") {
		static_assert(std::is_base_of<EntityComponent, T>::value, "Type not derived from EntityComponent");
		PropertyInfo pi;
		pi.name = name;
		pi.offset = offset;
		pi.custom_type_str = "EntityComponent";
		pi.flags = flags;
		pi.type = core_type_id::Struct;
		pi.range_hint = hint_str;
		return pi;
	}

	Entity* get_owner() { return entity_owner; }

	void attach_to_component(EntityComponent* parent_component, StringName point);

	void remove_this(EntityComponent* component) {}

#ifdef EDITOR_ONLY
	// compile any data relevant to the node
	virtual bool editor_compile() { return true; }
	virtual bool editor_only() const { return false; }
	virtual void editor_begin()  {}
	virtual void editor_tick() {}
	virtual void editor_on_change_property(const PropertyInfo& property_) {}
	uint64_t editor_uid = 0;
	bool editor_is_selected = false;
#endif

private:
	Entity* entity_owner = nullptr;

	EntityComponent* attached_parent = nullptr;
	StringName attached_bone_name;	// if non 0, determines 
	glm::vec3 location = glm::vec3(0.f);
	glm::quat rotation = glm::quat();
	glm::vec3 scale = glm::vec3(1.f);

	std::vector<EntityComponent*> children;

	StringName self_name;
	std::vector<StringName> tags;
};


#define REG_COMPONENT(name, flags, hint) EntityComponent::generate_prop_info( \
&((TYPE_FROM_START*)0)->name, \
#name, offsetof(TYPE_FROM_START,name), flags, hint)


CLASS_H(EmptyComponent, EntityComponent)
public:
	~EmptyComponent() override {}
};

CLASS_H(MeshComponent, EntityComponent)
public:
	MeshComponent();
	~MeshComponent() override;

	void tick() override;

	void set_model(const char* model_path);
	
	template<typename T>
	void set_animator_class() {
		set_animator_class(&T::StaticType);
	}
	void set_animator_class(const ClassTypeInfo* ti);

	bool is_simulating = false;
	bool is_hidden = false;
private:
	const ClassTypeInfo* animator_type = nullptr;
	unique_ptr<AnimatorInstance> animator;
	handle<Render_Object> draw_handle;
	Render_Object renderable;
	PhysicsActor* physics_actor = nullptr;
	Model* model = nullptr;
};

CLASS_H(CapsuleComponent, EntityComponent)
public:

};
CLASS_H(BoxComponent, EntityComponent)
public:

};
// sound,particle,light, etc. components

CLASS_H(Entity, ClassBase)
	Entity();
	virtual ~Entity();

	// called after properties were copied over
	virtual void post_load_properties() {}

	// called on spawn
	virtual void start() {}

	// called every game tick when actor is ticking
	virtual void update() {}

	void update_entity_and_components();

	entityhandle selfid = 0;	// eng->ents[]
	std::string name_id;		// name of entity frome editor

	glm::vec3 scale = glm::vec3(1.f);
	glm::vec3 position = glm::vec3(0.0);
	glm::quat rotation = glm::quat();

	glm::vec3 esimated_accel = glm::vec3(0.f);
	EntityFlags::Enum flags = EntityFlags::Enum(0);

	virtual glm::vec3 get_velocity() const {
		return glm::vec3(0.f);
	}

	bool has_flag(EntityFlags::Enum flag) const {
		return flags & flag;
	}
	void set_flag(EntityFlags::Enum flag, bool val) {
		if (val)
			flags = EntityFlags::Enum(flags | flag);
		else
			flags = EntityFlags::Enum(flags & (~flag));
	}
	glm::mat4 get_world_transform();


	void attach_to_entity(Entity* parent, StringName bone) {
		attach_to_component(parent->get_root_component(), bone);
	}
	void attach_to_component(EntityComponent* parent_comp, StringName bone);

	template<typename T>
	T* get_first_component() {
		for (int i = 0; i < all_components.size(); i++)
			if (all_components[i]->is_a<T>())
				return all_components[i];
		return nullptr;
	}
	EntityComponent* get_root_component() {
		return root_component;
	}
	template<typename T>
	void get_all_components(std::vector<T*>& array) {
		for (int i = 0; i < all_components.size(); i++)
			if (all_components[i]->is_a<T>())
				array.push_back(all_components[i]);
	}

	void remove_this_component(EntityComponent* component) {}
private:
	// if no components are set to root, this is used as a substitute
	// thus an entity always has a root component to attach stuff to
	EmptyComponent default_root;

	EntityComponent* root_component = nullptr;
	std::vector<EntityComponent*> all_components;
	// components created either in code or defined in schema or created per instance
	std::vector<unique_ptr<EntityComponent>> dynamic_components;
public:
	template<typename T>
	static PropertyInfo generate_entity_ref_property(T* dummy, const char* name, uint16_t offset, uint32_t flags) {
		static_assert(std::is_base_of<Entity, T>::value, "Type not derived from Entity");
		PropertyInfo pi;
		pi.name = name;
		pi.offset = offset;
		pi.flags = flags;
		pi.range_hint = T::StaticType.classname;
		pi.custom_type_str = "EntityRef";
		pi.type = core_type_id::Struct;
		return pi;
	}

#ifdef EDITOR_ONLY
	virtual bool editor_compile() { return true; }
	virtual bool editor_only() const { return false; }
	virtual void editor_begin() {}
	virtual void editor_tick() {}
	virtual void editor_on_change_property(const PropertyInfo& property_) {}
	uint64_t editor_uid = 0;
	bool editor_is_selected = false;
#endif

};
#define REG_ENTITY_REF(name, flags) Entity::generate_entity_ref_property( \
&((TYPE_FROM_START*)0)->name, \
#name, offsetof(TYPE_FROM_START,name), flags)