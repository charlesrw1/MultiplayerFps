#pragma once

#include <cstdint>
#include "Game/BaseUpdater.h"
#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"
#include "Framework/StringName.h"
#include "EntityPtr.h"
#include <vector>
#include <string>
#include "Framework/Reflection2.h"

class Model;
class PhysicsActor;
class Level;
class MeshComponent;
class Component;
struct PropertyInfoList;



GENERATED_CLASS_INCLUDE("Game/EntityComponent.h");

class Entity : public BaseUpdater
{
public:
	CLASS_BODY(Entity);

	const static bool CreateDefaultObject = true;

	Entity();

	virtual ~Entity();

	// destroy already reflected in BaseUpdater
	void destroy();
	void serialize(Serializer& s) final;
	template<typename T>
	T* get_component() const {
		return (T*)get_component_typeinfo(&T::StaticType);
	}

	REFLECT(name="get_comp");
	Component* get_component_typeinfo(const ClassTypeInfo* ti) const;

	REFLECT(name="parent",getter)
	Entity* get_parent() const {
		return parent;
	}
	
	// USE IN RUNTIME! use create_sub_component to setup object in the constructor
	// this calls on_init()
	template<typename T>
	T* create_component();
	Component* create_component_type(const ClassTypeInfo* info);
	// will also parent to this
	Entity* create_child_entity();

	template<typename T>
	T* create_entity_with_component();

	// ONLY USE in serialization!
	void add_component_from_unserialization(Component* component);

	// ws = world space, ls = local space
	void set_ls_transform(const glm::mat4& transform);
	void set_ls_transform(const glm::vec3& v, const glm::quat& q, const glm::vec3& scale);
	REF void set_ls_position(glm::vec3 v);
	REF void set_ls_euler_rotation(glm::vec3 euler);
	REF void set_ls_scale(glm::vec3 scale);
	glm::mat4 get_ls_transform() const;
	glm::vec3 get_ls_position() const { return position; }
	glm::vec3 get_ls_scale() const { return scale; }
	glm::quat get_ls_rotation() const { return rotation; }
	void set_ws_transform(const glm::mat4& transform);
	void set_ws_transform(const glm::vec3& v, const glm::quat& q, const glm::vec3& scale);
	const glm::mat4& get_ws_transform();

	REF glm::vec3 get_ws_position()  { 
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
	REF glm::vec3 get_ws_scale() {
		if (!parent)
			return scale;
		// fixme
		return glm::vec3(1.f);
	}
	REF void set_ws_position(glm::vec3 v) {
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
	REF void parent_to(Entity* parentEntity);

	void move_child_entity_index(Entity* who, int move_to);
	int get_child_entity_index(Entity* who) const;

	const std::vector<Component*>& get_components() const { return all_components; }

	const std::vector<Entity*>& get_children() const { return children; }

	EntityPtr get_self_ptr() const { return EntityPtr(this); }


	void invalidate_transform(Component* skipthis);

	virtual void editor_on_change_properties() {}

#ifdef EDITOR_BUILD
	// returns wether this or any parent is selected
	bool get_is_any_selected_in_editor() const;
	// returns wether just this is selected
	bool get_selected_in_editor() const {
		return selected_in_editor;
	}
#endif

	// removes from list (use component->desroy() for real destruction)
	void remove_this_component_internal(Component* component);

	MeshComponent* get_cached_mesh_component() const {
		return cached_mesh_component;
	}
	void set_cached_mesh_component(MeshComponent* c) {
		cached_mesh_component = c;
	}

	glm::mat4 get_parent_transform() const;

	void set_parent_bone(StringName name) {
		parent_bone = name;
	}
	REF bool has_parent_bone() const {
		return !parent_bone.is_null();
	}
	REF StringName get_parent_bone() const {
		return parent_bone;
	}
	REF bool get_start_disabled() const {
		return start_disabled;
	}

	// this function only has an effect is called before inserting the entity into the level
	// either set it in the editor, or spawn_deferred and set it
	void set_start_disabled(bool b) {
		start_disabled = b;
	}
	// if start_disabled was true, then this function actually calls start()->pre_start() etc. on all sub entities
	// if this entity was already started, then this function does nothing
	REF void activate();

	StringName get_tag() const {
		return tag;
	}
	bool has_tag() const {
		return tag.get_hash() != 0;
	}
	void set_tag(StringName name) {
		tag = name;
	}

	bool get_is_top_level() const {
		return is_top_level;
	}
	void set_is_top_level(bool b);


	const std::string& get_editor_name() const {
		return editor_name;
	}
	void set_editor_name(const std::string& n) {
		editor_name = n;
	}
#ifdef EDITOR_BUILD
	bool get_hidden_in_editor() const {
		return hidden_in_editor;
	}
	void set_hidden_in_editor(bool b);

	void set_prefab_editable(bool b) {
		prefab_editable = b;
	}
	void set_nested_owner_prefab(const PrefabAsset* asset) {
		this->nested_owner_prefab = asset;
	}
	const PrefabAsset* get_nested_owner_prefab() const  {
		return this->nested_owner_prefab;
	}
#endif
	// fixme
	bool get_prefab_editable() const {
		return prefab_editable;
	}
private:
	static void set_active_R(Entity* e, bool b, bool step1);

	bool has_transform_parent() const {
		return !get_is_top_level() && get_parent() != nullptr;
	}

	void post_change_transform_R(bool ws_is_dirty = true, Component* skipthis = nullptr);

	// components created either in code or defined in schema or created per instance
	std::vector<Component*> all_components;

	Entity* parent = nullptr;
	std::vector<Entity*> children;

	REFLECT(type="EntityTagString");
	StringName tag;
	REF glm::vec3 position = glm::vec3(0.f);
	REF glm::quat rotation = glm::quat(1,0,0,0);
	REF glm::vec3 scale = glm::vec3(1.f);
	REF std::string editor_name;
	REFLECT(type="EntityBoneParentString")
	StringName parent_bone;
	REF bool start_disabled = false;
	REF bool prefab_editable = false;

	MeshComponent* cached_mesh_component = nullptr;	// for bone lookups

	glm::mat4 cached_world_transform = glm::mat4(1);

#ifdef EDITOR_BUILD
	bool selected_in_editor = false;
	bool hidden_in_editor = false;
	const PrefabAsset* nested_owner_prefab = nullptr;
#endif

	bool world_transform_is_dirty = true;

	bool is_top_level = false;	// if true, then local space is considered the world space transform, even if a parent exists


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
inline T* Entity::create_component() {
	static_assert(std::is_base_of<Component, T>::value, "Type not derived from EntityComponent");
	return (T*)create_component_type(&T::StaticType);
}

template<typename T>
inline T* Entity::create_entity_with_component()
{
	Entity* e = create_child_entity();
	return e->create_component<T>();
}
