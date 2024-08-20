#pragma once

#include <cstdint>
#include "glm/glm.hpp"

#include "Framework/AddClassToFactory.h"
#include "Framework/StringName.h"
#include "Framework/ClassBase.h"
#include "Framework/ReflectionMacros.h"
#include "Framework/ReflectionProp.h"

#include "Game/EntityComponent.h"

#include "GameEnginePublic.h"
#include "Game/BaseUpdater.h"

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
	T* get() const {
		auto e = eng->get_entity(handle);
		return e ? e->cast_to<T>() : nullptr;
	}

	uint64_t handle = 0;
};

template<typename T>
inline PropertyInfo make_entity_ptr_property(const char* name, uint16_t offset, uint32_t flags, EntityPtr<T>* dummy) {
	return make_struct_property(name, offset, flags, "EntityPtr", T::StaticType.classname);
}
#define REG_ENTITY_PTR(name, flags) make_entity_ptr_property(#name, offsetof(MyClassType,name),flags,&((MyClassType*)0)->name)


class Schema;
class EditorFolder;
CLASS_H(Entity, BaseUpdater)
public:
	const static bool CreateDefaultObject = true;

	Entity();
	virtual ~Entity();

	// Entity lifecycle:
	// 
	// Contstructor						: set any default variable values here, setup native components here
	//		V
	// Unserialize						: loads data from disk in a level or a schema (this section can also be hooked to set properties when spawning during gameplay)
	//		V
	// register_components				: components (such as MeshComponent) initialize here like allocating handles into systems
	//		| ------------------|
	//		V				    |
	// ##################		|
	//	  *start()		#		V		: start() is called on the Entity to initialize itself before ticking
	//		V			#		|
	// @..*update().........*component ticks ...@
	//		V			#		|
	//    *end()		#		V		: end() is the counter part to start(), do any logic before deletion
	// ##################		|
	//		| ------------------|
	//		V					
	// unregister_components			: removes itself from systems
	//		V
	// Destructor
	// 
	// "*" are functions that are virtual
	// "#" boxed functions are only called during gameplay (NOT in the editor)
	//	update and component ticks are called per frame or on a regular interval (game and/or editor)
	// register_components + start and end + unregister_components are bundled in initialize and destroy

	void initialize();
	void destroy();

	// called every game tick when actor is ticking
	virtual void update() override {}

	// Editor calls tick on components but not on entity
	
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

	// USE IN GAMEPLAY! use create_sub_component to setup object in the constructor
	template<typename T>
	T* create_and_attach_component_type(EntityComponent* parent = nullptr, StringName bone = {});
	EntityComponent* create_and_attach_component_type(const ClassTypeInfo* info, EntityComponent* parent = nullptr, StringName bone = {});

	// ONLY USE in Constructor!, use create_and_attach for normal usage
	template<typename T>
	T* create_sub_component(const std::string& name);

	// DELETES the EntityComponent, dont use the ptr again after this call
	void remove_this_component(EntityComponent* component);

	// get/set world space transform  (transform of the root component)
	const glm::mat4& get_ws_transform() { return get_root_component()->get_ws_transform(); }
	glm::vec3 get_ws_position() { return get_root_component()->position; }
	glm::quat get_ws_rotation() { return get_root_component()->rotation; }
	glm::vec3 get_ws_scale() { return get_root_component()->scale; }
	void set_ws_transform(const glm::mat4& mat) { return get_root_component()->set_ws_transform(mat); }
	void set_ws_transform(const glm::vec3& pos, const glm::quat& q, const glm::vec3& scale) { return get_root_component()->set_ls_transform(pos,q,scale); }
	void set_ws_position(const glm::vec3& v) { return get_root_component()->set_ls_transform(v, get_ws_rotation(), get_ws_scale()); }
	void set_ws_rotation(const glm::quat& q) { return get_root_component()->set_ls_transform(get_ws_position(), q, get_ws_scale()); }

	const std::vector<std::unique_ptr<EntityComponent>>& get_all_components() { return all_components; }

	void add_component_from_loading(EntityComponent* component);
protected:

	ObjPtr<EntityComponent> root_component;
	// components created either in code or defined in schema or created per instance
	std::vector<std::unique_ptr<EntityComponent>> all_components;

	friend class Schema;
	friend class LevelSerialization;
public:
	EntityComponent* find_component_for_string_name(const std::string& name) const {
		for (auto& c : all_components)
			if (c->eSelfNameString == name)
				return c.get();
		return nullptr;
	}

	static const PropertyInfoList* get_props();

	template<typename T>
	static PropertyInfo generate_entity_ref_property(T* dummy, const char* name, uint16_t offset, uint32_t flags);
#ifndef NO_EDITOR
	virtual bool editor_compile() { return true; }
	virtual bool editor_only() const { return false; }
	virtual void editor_begin() {}
	virtual void editor_tick() {}
	virtual void editor_on_change_property(const PropertyInfo& property_) {}

	ObjPtr<EditorFolder> editor_folder;
	std::string editor_name;
#endif
	EntityPtr<Entity> self_id;			// global identifier for this entity
	AssetPtr<Schema> schema_type;		// what spawned type are we ( could be editor only or not )

	bool is_selected_in_editor() const
	{
		return selectedInEditor;
	}
private:
	// editor only obviously
	bool selectedInEditor = false;

	virtual void start() {}
	virtual void end() {}

	friend class EditorDoc;
	friend class SelectionState;
};
#define REG_ENTITY_REF(name, flags) Entity::generate_entity_ref_property( \
&((TYPE_FROM_START*)0)->name, \
#name, offsetof(TYPE_FROM_START,name), flags)

inline void Entity::remove_this_component(EntityComponent* component) {
	assert(component != root_component.get() && "cant delete the root component");
	bool found = false;
	for (int i = 0; i < all_components.size(); i++) {
		if (all_components[i].get() == component) {
			all_components.erase(all_components.begin() + i);
			found = true;
			break;
		}
	}

	assert(found && "component not found in remove_this_component");
}

template<typename T>
inline T* Entity::create_sub_component(const std::string& name) {
	static_assert(std::is_base_of<EntityComponent, T>::value, "Type not derived from EntityComponent");
	auto ptr = std::make_unique<T>();
	ptr->eSelfNameString = name;
	ptr->set_owner(this);
	all_components.push_back(std::move(ptr));
	return (T*)all_components.back().get();
}

template<typename T>
inline T* Entity::create_and_attach_component_type(EntityComponent* parent, StringName bone) {
	static_assert(std::is_base_of<EntityComponent, T>::value, "Type not derived from EntityComponent");
	T* ptr = new T;
	ptr->set_owner(this);
	all_components.push_back(std::unique_ptr<EntityComponent>(ptr));
	ptr->attach_to_parent(parent == nullptr ? root_component.get() : parent, bone);
	ptr->on_init();
	return ptr;
}
inline EntityComponent* Entity::create_and_attach_component_type(const ClassTypeInfo* info, EntityComponent* parent, StringName bone)
{
	if (!info->is_a(EntityComponent::StaticType)) {
		sys_print("!!! create_and_attach_component_type not subclass of entity component\n");
		return nullptr;
	}
	EntityComponent* ec = (EntityComponent*)info->allocate();
	ec->set_owner(this);
	all_components.push_back(std::unique_ptr<EntityComponent>(ec));
	ec->attach_to_parent(parent == nullptr ? root_component.get() : parent, bone);
	ec->on_init();
	return ec;
}


template<typename T>
inline static PropertyInfo Entity::generate_entity_ref_property(T* dummy, const char* name, uint16_t offset, uint32_t flags) {
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
inline void Entity::add_component_from_loading(EntityComponent* component)
{
	component->set_owner(this);
	all_components.push_back(std::unique_ptr<EntityComponent>(component));
}