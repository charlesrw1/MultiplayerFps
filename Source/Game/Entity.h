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


class Model;
class PhysicsActor;

// A gobally unique identifier in the game level
// every entity is assigned one, and its used to reference others
template<typename T>
class EntityPtr
{
public:
	static_assert(std::is_base_of<Entity, T>::value, "EntityPtr must derive from Entity");

	bool is_valid() const { return get() != nullptr; }
	T* get() const {
		if (handle == 0) return nullptr;
		auto e = eng->get_entity(handle);
		return e ? e->cast_to<T>() : nullptr;
	}
	T& operator*() const {
		return *get();
	}
	operator bool() const {
		return is_valid();
	}
	T* operator->() const {
		return get();
	}
	template<typename K>
	bool operator==(const EntityPtr<K>& other) {
		return handle == other.handle;
	}
	template<typename K>
	bool operator!=(const EntityPtr<K>& other) {
		return handle != other.handle;
	}

	uint64_t handle = 0;
};

template<typename T>
inline PropertyInfo make_entity_ptr_property(const char* name, uint16_t offset, uint32_t flags, EntityPtr<T>* dummy) {
	return make_struct_property(name, offset, flags, "EntityPtr", T::StaticType.classname);
}
#define REG_ENTITY_PTR(name, flags) make_entity_ptr_property(#name, offsetof(MyClassType,name),flags,&((MyClassType*)0)->name)


class Schema;

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

	// called every game tick when actor has tick enabled
	virtual void update() override {}
private:
	// called when entity spawns in game only
	virtual void start() {}
	// called when entity is destroyed in game only
	virtual void end() {}
public:

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

	// USE IN GAMEPLAY! use create_sub_component to setup object in the constructor
	// this calls on_init()
	template<typename T>
	T* create_and_attach_component_type(EntityComponent* parent = nullptr, StringName bone = {});
	EntityComponent* create_and_attach_component_type(const ClassTypeInfo* info, EntityComponent* parent = nullptr, StringName bone = {});

	// ONLY USE in Constructor!, use create_and_attach for normal usage
	template<typename T>
	T* create_sub_component(const std::string& name);
	// ONLY USE in serialization!
	void add_component_from_loading(EntityComponent* component);
	// create_sub_component<T>("name") is equivlent to:
	// T* component = new T()
	// component->name = "name"
	// add_component_from_loading(component)

	// DELETES the EntityComponent (and calls component->deinit()), dont use the ptr again after this call
	void remove_this_component(EntityComponent* component);

	// get/set world space transform  (transform of the root component)
	const glm::mat4& get_ws_transform() { return get_root_component()->get_ws_transform(); }
	glm::vec3 get_ws_position() { return get_root_component()->get_ws_position(); }
	glm::quat get_ws_rotation() { return get_root_component()->get_ws_rotation(); }
	glm::vec3 get_ws_scale() { return get_root_component()->get_ws_scale(); }
	void set_ws_transform(const glm::mat4& mat) { get_root_component()->set_ws_transform(mat); }
	void set_ws_transform(const glm::vec3& pos, const glm::quat& q, const glm::vec3& scale) { get_root_component()->set_ws_transform(pos,q,scale); }
	void set_ws_position(const glm::vec3& v) {  get_root_component()->set_ws_transform(v, get_ws_rotation(), get_ws_scale()); }
	void set_ws_rotation(const glm::quat& q) {  get_root_component()->set_ws_transform(get_ws_position(), q, get_ws_scale()); }
	void set_ws_scale(const glm::vec3& s) { get_root_component()->set_ws_transform(get_ws_position(), get_ws_rotation(), s); }

	// parent the root component of this to another entity
	// can use nullptr to unparent
	void parent_to_entity(Entity* parentEntity);	// parents to root component
	void parent_to_component(EntityComponent* parentEntityComponent);	// parent to specified component

	const std::vector<std::unique_ptr<EntityComponent>>& get_all_components() { return all_components; }

protected:

	EntityPtr<Entity> parentedEntity;	// what is root component parented to, this can be accessed inside it too, but this is used for serialization
	EntityComponent* root_component = nullptr;
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


#ifndef NO_EDITOR
	virtual bool editor_compile() { return true; }
	virtual bool editor_only() const { return false; }
	virtual void editor_begin() {}
	virtual void editor_tick() {}
	virtual void editor_on_change_property(const PropertyInfo& property_) {}

	//ObjPtr<EditorFolder> editor_folder;
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



	friend class EditorDoc;
	friend class SelectionState;
};

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
	ptr->attach_to_parent(parent == nullptr ? root_component : parent, bone);
	ptr->init();
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
	ec->attach_to_parent(parent == nullptr ? root_component : parent, bone);
	ec->init();
	return ec;
}


inline void Entity::add_component_from_loading(EntityComponent* component)
{
	component->set_owner(this);
	all_components.push_back(std::unique_ptr<EntityComponent>(component));
}