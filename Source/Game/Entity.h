#pragma once

#include <cstdint>
#include "glm/glm.hpp"

#include "Framework/AddClassToFactory.h"
#include "Framework/StringName.h"
#include "Framework/ClassBase.h"
#include "Framework/ReflectionMacros.h"
#include "Framework/ReflectionProp.h"

#include "Game/EntityComponent.h"

#include "Game/BaseUpdater.h"
#include "GameEnginePublic.h"

class Model;
class PhysicsActor;
class Level;

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
		auto e = eng->get_object(handle);
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
template<>
struct GetAtomValueWrapper<EntityPtr<Entity>> {
	static PropertyInfo get();
};

template<typename T>
inline PropertyInfo make_entity_ptr_property(const char* name, uint16_t offset, uint32_t flags, EntityPtr<T>* dummy) {
	return make_struct_property(name, offset, flags, "EntityPtr", T::StaticType.classname);
}
#define REG_ENTITY_PTR(name, flags) make_entity_ptr_property(#name, offsetof(MyClassType,name),flags,&((MyClassType*)0)->name)


class MeshComponent;

struct BoneParentStruct
{
	std::string string;	// only in editor
	StringName name;	
};

CLASS_H(Entity, BaseUpdater)
public:
	const static bool CreateDefaultObject = true;

	Entity();

	virtual ~Entity();

	void destroy();

	template<typename T>
	T* get_first_component() {
		for (int i = 0; i < all_components.size(); i++)
			if (all_components[i]->is_a<T>())
				return (T*)all_components[i];
		return nullptr;
	}

	Entity* get_entity_parent() const {
		return parent;
	}

	template<typename T>
	void get_all_components(std::vector<T*>& array) {
		for (int i = 0; i < all_components.size(); i++)
			if (all_components[i]->is_a<T>())
				array.push_back(all_components[i]);
	}
	
	// USE IN RUNTIME! use create_sub_component to setup object in the constructor
	// this calls on_init()
	template<typename T>
	T* create_and_attach_component_type();
	EntityComponent* create_and_attach_component_type(const ClassTypeInfo* info);
	template<typename T>
	T* create_and_attach_entity();
	Entity* create_and_attach_entity(const ClassTypeInfo* info);

	// USE IN CONSTRUCTOR!, use create_and_attach for normal usage
	template<typename T>
	T* construct_sub_component(const char* name);
	template<typename T>
	T* construct_sub_entity(const char* name);

	// ONLY USE in serialization!
	void add_component_from_unserialization(EntityComponent* component);

	// ws = world space, ls = local space
	void set_ls_transform(const glm::mat4& transform);
	void set_ls_transform(const glm::vec3& v, const glm::quat& q, const glm::vec3& scale);
	void set_ls_euler_rotation(const glm::vec3& euler);
	glm::mat4 get_ls_transform() const;
	glm::vec3 get_ls_position() const { return position; }
	glm::vec3 get_ls_scale() const { return scale; }
	glm::quat get_ls_rotation() const { return rotation; }
	void set_ws_transform(const glm::mat4& transform);
	void set_ws_transform(const glm::vec3& v, const glm::quat& q, const glm::vec3& scale);
	const glm::mat4& get_ws_transform();
	glm::vec3 get_ws_position()  { 
		if (!parent)
			return position;
		auto& ws = get_ws_transform();
		return ws[3];
	}
	glm::quat get_ws_rotation() { 
		if (!parent)
			return rotation;
		auto& ws = get_ws_transform();
		return glm::quat_cast(ws);
	}
	glm::vec3 get_ws_scale() {
		if (!parent)
			return scale;
		// fixme
		return glm::vec3(1.f);
	}
	void set_ws_position(const glm::vec3& v) {
		set_ws_transform(v, get_ws_rotation(), get_ws_scale());
	}
	void set_ws_rotation(const glm::quat& q) {
		set_ws_transform(get_ws_position(), q, get_ws_scale());
	}
	void set_ws_scale(const glm::vec3& s) {
		set_ws_transform(get_ws_position(), get_ws_rotation(), s);
	}

	// parent the root component of this to another entity
	// can use nullptr to unparent
	void parent_to_entity(Entity* parentEntity);

	const std::vector<EntityComponent*>& get_all_components() const { return all_components; }

	const std::vector<Entity*>& get_all_children() const { return children; }

	EntityPtr<Entity> get_self_ptr() const { return { get_instance_id() }; }

	static const PropertyInfoList* get_props();

	void invalidate_transform(EntityComponent* skipthis);

	virtual void editor_on_change_properties() {}
	std::string editor_name;

	bool is_selected_in_editor() const {
		return selected_in_editor;
	}

	// removes from list (use component->desroy() for real destruction)
	void remove_this_component_internal(EntityComponent* component);

	MeshComponent* get_cached_mesh_component() const {
		return cached_mesh_component;
	}
	void set_cached_mesh_component(MeshComponent* c) {
		cached_mesh_component = c;
	}

	glm::mat4 get_parent_transform() const;

	void set_parent_bone(const std::string& bone) {
		parent_bone.string = bone;
		if (!bone.empty())
			parent_bone.name = StringName(bone.c_str());
		else
			parent_bone.name = StringName();
	}
	bool has_parent_bone() const {
		return parent_bone.name.get_hash() != 0;
	}

	void set_active(bool active);

	bool get_start_disabled() const {
		return start_disabled;
	}
private:

	void post_change_transform_R(bool ws_is_dirty = true, EntityComponent* skipthis = nullptr);

	// components created either in code or defined in schema or created per instance
	std::vector<EntityComponent*> all_components;

	Entity* parent = nullptr;
	std::vector<Entity*> children;

	BoneParentStruct parent_bone;
	MeshComponent* cached_mesh_component = nullptr;	// for bone lookups

	glm::vec3 position = glm::vec3(0.f);
	glm::quat rotation = glm::quat();
	glm::vec3 scale = glm::vec3(1.f);
	glm::mat4 cached_world_transform = glm::mat4(1);

	bool selected_in_editor = false;
	bool world_transform_is_dirty = true;

	bool start_disabled = false;

	// called by Level for init/destruct
	void initialize_internal();
	void destroy_internal();

	// called by child entity to remove it from children list
	void remove_this(Entity* child_entity);

	friend class EditorDoc;
	friend class UnserializedSceneFile;
	friend class SelectionState;
	friend class Level;
	friend class Schema;
	friend class LevelSerialization;
	friend class DeferredSpawnScope;
	friend class EdPropertyGrid;
	friend class SerializeTestWorkbench;
};

template<typename T>
inline T* Entity::construct_sub_component(const char* name) {
	ASSERT(init_state == initialization_state::CONSTRUCTOR);
	static_assert(std::is_base_of<EntityComponent, T>::value, "Type not derived from EntityComponent");
	auto ptr = new T;
	ptr->set_owner(this);
	ptr->unique_file_id = uint32_t(StringName(name).get_hash());
	ptr->creator_source = this;
	ptr->is_native_created = true;
	all_components.push_back(ptr);
	return (T*)all_components.back();
}
template<typename T>
inline T* Entity::construct_sub_entity(const char* name) {
	ASSERT(init_state == initialization_state::CONSTRUCTOR);
	static_assert(std::is_base_of<Entity, T>::value, "Type not derived from Entity");
	auto ptr = new T;
	ptr->unique_file_id = uint32_t(StringName(name).get_hash());
	ptr->creator_source = this;
	ptr->parent = this;
	ptr->is_native_created = true;
	children.push_back(ptr);
	return (T*)children.back();
}

template<typename T>
inline T* Entity::create_and_attach_component_type() {
	static_assert(std::is_base_of<EntityComponent, T>::value, "Type not derived from EntityComponent");
	return (T*)create_and_attach_component_type(&T::StaticType);
}
template<typename T>
inline T* Entity::create_and_attach_entity() {
	static_assert(std::is_base_of<Entity, T>::value, "Type not derived from EntityComponent");
	return (T*)create_and_attach_entity(&T::StaticType);
}