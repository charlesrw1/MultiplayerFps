#include "EntityTypes.h"
#include "Framework/Factory.h"
#include "Physics/Physics2.h"

#include "Animation/Runtime/Animation.h"

CLASS_IMPL(Entity);

CLASS_IMPL(Door);
CLASS_IMPL(Grenade);
CLASS_IMPL(NPC);

CLASS_IMPL(EntityComponent);

CLASS_IMPL(EmptyComponent);
CLASS_IMPL(MeshComponent);
CLASS_IMPL(BoxComponent);
CLASS_IMPL(CapsuleComponent);

// Database:
// map asset path to id, id gets incremented every time a new asset is made


// global object table:
// 1 handle, no bullshit
// any models, sounds, etc. that get referenced are put into the table

// entity ptrs get serialized as an index into a descriptor table
// on unserialization 

template<typename T>
class RawPtr
{
public:
	static_assert(std::is_base_of<ClassBase, T>::value, "RawPtr must derive from ClassBase");
	T* ptr = nullptr;
};


template<typename T>
class EntityPtr
{
public:
	static_assert(std::is_base_of<Entity, T>::value, "EntityPtr must derive from Entity");

	bool is_valid() const;
	T* get() const;
	void assign(T* ptr);

	uint32_t handle=0;
	uint32_t id=0;
};

CLASS_H(MyEntityComp, EntityComponent)
public:
	EntityPtr<Door> Model;
};
// database to map an integer to any type of object, for example models or entities, automatically resolved and editable in the editor

MeshComponent::~MeshComponent()
{
	if (draw_handle.is_valid())
		idraw->remove_obj(draw_handle);
	if (physics_actor)
		g_physics->free_physics_actor(physics_actor);
}
MeshComponent::MeshComponent() {}

Entity::Entity()
{
	// initialize native components
	// this should be moved out into ClassTypeInfo itself on static init
	const ClassTypeInfo* ti = &get_type();
	InlineVec<const PropertyInfoList*, 10> allprops;
	for (; ti; ti = ti->super_typeinfo) {
		if (ti->props)
			allprops.push_back(ti->props);
	}
	for (int i = 0; i < allprops.size(); i++) {
		auto props = allprops[i];
		for (int j = 0; j < props->count; j++) {
			if (strcmp(props->list[j].custom_type_str, "EntityComponent") == 0) {
				all_components.push_back((EntityComponent*)props->list[j].get_ptr(this));
			}
		}
	}
	// TODO: root+parenting components
}


void Entity::update_entity_and_components() {
	// call the entity tick function
	update();

	// tick components, update renderables, animations etc.
	for (int i = 0; i < all_components.size(); i++)
		all_components[i]->tick();
}

void EntityComponent::destroy()
{
	attached_parent->remove_this(this);
	entity_owner->remove_this_component(this);
}

void MeshComponent::tick()
{
	// stuff to do:
	// if parent transform not dirty and state not dirty, return
	// allocate render model / animator if not already done so
	// get transform from parent
}