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
#include "Framework/ReflectionMacros.h"
#include "Framework/ReflectionProp.h"

#include "Game/EntityComponentTypes.h"

#ifndef NO_EDITOR
#include "EditorFolder.h"
#endif // !NO_EDITOR

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


// A gobally unique identifier in the game level
// every entity is assigned one, and its used to reference others
template<typename T>
class EntityPtr
{
public:
	static_assert(std::is_base_of<Entity, T>::value, "EntityPtr must derive from Entity");

	bool is_valid() const { return handle != 0; }
	T* get() const;

	uint64_t handle = 0;
};

template<typename T>
inline PropertyInfo make_entity_ptr_property(const char* name, uint16_t offset, uint32_t flags, EntityPtr<T>* dummy) {
	return make_struct_property(name, offset, flags, "EntityPtr", T::StaticType.classname);
}
#define REG_ENTITY_PTR(name, flags) make_entity_ptr_property(#name, offsetof(MyClassType,name),flags,&((MyClassType*)0)->name)




// Destroy all components at end
//		de_init()
// Destroy an added dynamic component (and all subchildren)
// Move a dynamic component to a different parent


// EntityComponents are used for shared data/logic between Entities
// such as Meshes, Physics, Sounds, Fx, etc.
// is ordered in a heirarchy



// Entity::CTOR,Component::CTOR: set any flags, booleans
// Entity::PostLoad: after loading properties from disk
// Component::Register: after PostLoad, registers components
// Entity::Start,Component::Start: called before object is spawned in

// In Editor, CTOR,PostLoad, and Register
// Editor also calls tick function on components if its variable is set

// Spawning entities in gameplay: CreateEntity<EntityClass>("schemaname", transform)
// set any variables etc. FinishEntitySpawn(ptr)
// Or: CreateAndSpawnEntity("schemaname",transform)

class Schema;
class EditorFolder;
CLASS_H(Entity, ClassBase)
public:
	const static bool CreateDefaultObject = true;

	Entity();
	virtual ~Entity();

	// both game and editor:
	// Object constructors
	// Sets any default values, these values are also what drives the override/inherit system of class types

	// This is where stuff gets unserialized from an asset/level
	// Fields are overwritten by inherited members, dynamic components are added and attached to the heirarchy
	// After this is called, dynamic_components will be finialized, components will be in a heirarchy, root_component is valid
	// However: all_components is empty still and no components have run on_init(); these are done inside register_components()
	// At this point there is still no logic being run that is dependent on the game state
	// load_props()

	// called after properties were copied over
	// use for any extra logic to change property values etc.
	virtual void post_load_properties() {}

	// called after post_load
	// registers and initializes any components that were serialized with object
	void register_components();

	// GAME ONLY: (editor doesnt call any of these)

	// called when spawning entity, all variables are initialized, components are initialized
	// execute any logic to start up this entity
	virtual void start() {}

	// called every game tick when actor is ticking
	virtual void update() {}
	void update_entity_and_components();

	// called on entity exit
	// free resources used by the entity that were aquired in start() or update() calls
	virtual void end() {}

	// BOTH game and editor:
	// called after end(), frees component data
	void destroy();
	// delete is called then DTOR is called


	// Editor calls tick on components but not on entity

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

	template<typename T>
	T* get_first_component() {
		for (int i = 0; i < all_components.size(); i++)
			if (all_components[i]->is_a<T>())
				return all_components[i];
		return nullptr;
	}
	EntityComponent* get_root_component() {
		return root_component.get();
	}
	template<typename T>
	void get_all_components(std::vector<T*>& array) {
		for (int i = 0; i < all_components.size(); i++)
			if (all_components[i]->is_a<T>())
				array.push_back(all_components[i]);
	}

	template<typename T>
	T* create_and_attach_component_type(EntityComponent* parent = nullptr, StringName bone = {}) {
		static_assert(std::is_base_of<EntityComponent, T>::value, "Type not derived from EntityComponent");
		T* ptr = (T*)T::StaticType.allocate();
		ptr->set_owner(this);
		all_components.push_back(ptr);
		dynamic_components.push_back(std::unique_ptr<EntityComponent>(ptr));
		ptr->attach_to_parent(parent == nullptr ? root_component.get() : parent, bone);
		ptr->on_init();
		return ptr;
	}

	// DELETES the EntityComponent, dont use the ptr again after this call
	void remove_this_component(EntityComponent* component) {
		assert(component != root_component.get() && "cant delete the root component");
		bool found = false;
		for (int i = 0; i < all_components.size(); i++) {
			if (all_components[i] == component) {
				all_components.erase(all_components.begin() + i);
				found = true;
				break;
			}
		}
		assert(found && "component not found in remove_this_component");
		if (component->get_is_native_component()) {
			found = false;
			for (int i = 0; i < dynamic_components.size(); i++) {
				if (dynamic_components[i].get() == component) {
					dynamic_components.erase(dynamic_components.begin() + i);
					found = true;
					break;
				}
			}
			assert(found && "dynamic component not found in remove_this_component");
		}
	}

	const std::vector<EntityComponent*>& get_all_components() { return all_components; }
protected:

	ObjPtr<EntityComponent> root_component;
	std::vector<EntityComponent*> all_components;
	// components created either in code or defined in schema or created per instance
	std::vector<std::unique_ptr<EntityComponent>> dynamic_components;

	friend class Schema;
public:

	static const PropertyInfoList* get_props();

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

#ifndef NO_EDITOR
	virtual bool editor_compile() { return true; }
	virtual bool editor_only() const { return false; }
	virtual void editor_begin() {}
	virtual void editor_tick() {}
	virtual void editor_on_change_property(const PropertyInfo& property_) {}
	uint64_t editor_uid = 0;
	bool editor_is_selected = false;
	ObjPtr<EditorFolder> editor_folder;
#endif
	EntityPtr<Entity> self_id;			// global identifier for this entity
	AssetPtr<Schema> schema_type;		// what spawned type are we ( could be editor only or not )

};
#define REG_ENTITY_REF(name, flags) Entity::generate_entity_ref_property( \
&((TYPE_FROM_START*)0)->name, \
#name, offsetof(TYPE_FROM_START,name), flags)
