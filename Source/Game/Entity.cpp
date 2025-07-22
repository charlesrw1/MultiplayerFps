#include "Entity.h"

#include "Framework/ArrayReflection.h"
#include "Framework/PropertyEd.h"
#include "Render/Model.h"
#include "Animation/SkeletonData.h"
#include "glm/gtx/euler_angles.hpp"

#include "Assets/AssetRegistry.h"

#include "Level.h"
#include "GameEnginePublic.h"

#include "Scripting/FunctionReflection.h"

#include "Game/EntityPtr.h"
#include "Game/Components/MeshComponent.h"

#include "Framework/ReflectionProp.h"
#include "Framework/ReflectionMacros.h"
#include "Framework/AddClassToFactory.h"


#include "Framework/Serializer.h"

// create native entities as a fake "Asset" for drag+drop and double click open to create instance abilities
#ifdef EDITOR_BUILD
extern IEditorTool* g_editor_doc;	// EditorDocPublic.h
class EntityTypeMetadata : public AssetMetadata
{
public:
	// Inherited via AssetMetadata
	virtual Color32 get_browser_color() const  override
	{
		return { 117, 75, 242 };
	}

	virtual std::string get_type_name() const  override
	{
		return "EntityClass";
	}

	virtual void fill_extra_assets(std::vector<std::string>& filepaths) const  override {
		auto subclasses = ClassBase::get_subclasses<Entity>();
		for (; !subclasses.is_end(); subclasses.next()) {
			if (subclasses.get_type()->has_allocate_func()) {
				std::string path = subclasses.get_type()->classname;
				//auto parent = subclasses.get_type();
				//while (parent && parent != &Entity::StaticType) {
				//	path.insert(0, std::string(parent->super_typeinfo->classname) + "/");
				//	parent = parent->super_typeinfo;
				//}
				
				filepaths.push_back(path);
			}
		}
	}

	virtual const ClassTypeInfo* get_asset_class_type() const { return &Entity::StaticType; }

};
REGISTER_ASSETMETADATA_MACRO(EntityTypeMetadata);
#endif




Entity::Entity()
{
}

#ifdef EDITOR_BUILD
void Entity::set_hidden_in_editor(bool b)
{
	hidden_in_editor = b;
	for (int i = 0; i < children.size(); i++) {
		children[i]->set_hidden_in_editor(b);
	}
	for (int i = 0; i < all_components.size(); i++)
		all_components[i]->sync_render_data();
}
const PrefabAsset& Entity::get_object_prefab() const {
	assert(this->what_prefab && get_object_prefab_spawn_type()==EntityPrefabSpawnType::RootOfPrefab);
	return *this->what_prefab;
}
void Entity::set_spawned_by_prefab() {
	if (what_prefab)
		what_prefab = nullptr;
	set_editor_transient(true);	// make it not editable
}
void Entity::set_root_object_prefab(const PrefabAsset& asset) {
	if (what_prefab) {
		sys_print(Warning, "Entity::set_root_object_prefab: already had prefab assigned?\n");
	}
	what_prefab = &asset;
}
void Entity::set_prefab_no_owner_after_being_root()
{
	assert(get_object_prefab_spawn_type() == EntityPrefabSpawnType::RootOfPrefab);
	what_prefab = nullptr;
	assert(get_object_prefab_spawn_type() == EntityPrefabSpawnType::None);
}
EntityPrefabSpawnType Entity::get_object_prefab_spawn_type() const {
	if (what_prefab) {
		return EntityPrefabSpawnType::RootOfPrefab;
	}
	else
		return EntityPrefabSpawnType::None;
}
void Entity::check_for_transform_nans()
{
	if (position.x != position.x || position.y != position.y || position.z != position.z) {
		sys_print(Warning, "detected NaN in entity position\n");
		position = glm::vec3(0.f);
	}
	if (rotation.x != rotation.x || rotation.y != rotation.y || rotation.z != rotation.z || rotation.w != rotation.w) {
		sys_print(Warning, "detected NaN in entity rotation\n");
		rotation = glm::quat();
	}
	if (scale.x != scale.x || scale.y != scale.y || scale.z != scale.z) {
		sys_print(Warning, "detected NaN in entity scale\n");
		scale = glm::vec3(1.f);
	}
}
void Entity::validate_check()
{
	assert(get_instance_id() != 0);
	assert(eng->get_object(get_instance_id()) == this);
	for (auto c : children)
		c->validate_check();
	for (auto comp : all_components){
		assert(comp->get_instance_id() != 0);
		assert(eng->get_object(comp->get_instance_id()) == comp);
	}

}
bool Entity::get_is_any_selected_in_editor() const {
	if (get_selected_in_editor())
		return true;
	if (!get_parent())
		return false;
	return get_parent()->get_is_any_selected_in_editor();
}
#endif


void Entity::set_active_R(Entity* e, bool b, bool step1)
{
	if (e->is_activated() != b)
	{
		for (auto c : e->get_components()) {
			if (b) {
				if (step1)
					c->activate_internal_step1();
				else
					c->activate_internal_step2();
			}
			else {
				c->deactivate_internal();
			}
		}
	}
	for (auto c : e->get_children())
		set_active_R(c, b, step1);
}

#include "LevelAssets.h"

void Entity::initialize_internal()
{
	ASSERT(init_state == initialization_state::HAS_ID);
	init_state = initialization_state::CALLED_START;
	check_for_transform_nans();


	if (what_prefab) {
		assert(get_object_prefab_spawn_type() == EntityPrefabSpawnType::RootOfPrefab);
		what_prefab->finish_prefab_setup(this);
	}
}


void Entity::remove_this(Entity* child)
{
	ASSERT(child);
	ASSERT(child->parent == this);
	child->parent = nullptr;

	auto count_in_children = [&](Entity* child) -> int {
		int count = 0;
		for (auto e : children)
			if (e == child)
				count++;
		return count;
	};
	ASSERT(count_in_children(child) == 1);

	for (int i = 0; i < children.size(); i++) {
		if (children[i] == child) {
			children.erase(children.begin() + i);
			return;
		}
	}

	ASSERT(!"unreachable");
}


void Entity::destroy()
{
	eng->get_level()->destroy_entity(this);
}

void Entity::serialize(Serializer& s)
{
	Entity* myparent = get_parent();
	bool hasparent = s.serialize_class_reference("parent", myparent);
	if (s.is_loading() && hasparent && myparent) {
		parent_to(myparent);
	}
}

void Entity::destroy_internal()
{
	ASSERT(init_state != initialization_state::CALLED_PRE_START);	// invalid state
	if (init_state == initialization_state::CALLED_START) {
		init_state = initialization_state::HAS_ID;
	}


	if (get_parent())
		get_parent()->remove_this(this);
	ASSERT(parent == nullptr);

	int loop_count = 0;
	while (!children.empty()) {
		ASSERT(loop_count < 100);
		int pre_count = children.size();
		children.front()->destroy();	// destroy() unparents entity, which shrinks this vec, assert this
		ASSERT(children.size() <= pre_count - 1);
		loop_count++;
	}
	ASSERT(children.empty());

	loop_count = 0;
	while (!all_components.empty()) {
		ASSERT(loop_count < 100);
		int presize = all_components.size();
		all_components.front()->destroy();
		ASSERT(all_components.size() <= presize - 1);	// can be less than if components desroy other components
		loop_count++;
	}
	ASSERT(all_components.empty());
}

void Entity::parent_to(Entity* other)
{
	if (other == this) {
		sys_print(Warning, "cant parent entity to self\n");
	}

	if (other == parent)
		return;

	// prevents circular parent creations
	// checks the node we are parenting to's tree to see if THIS is one of the parent nodes
	{
		Entity* cur_node = other;
		while (cur_node) {

			if (cur_node->get_parent() == this) {
				remove_this(cur_node);
				cur_node->parent_to(parent);	// parent circular child entity to my parent
				break;
			}
			cur_node = cur_node->get_parent();
		}
	}

	// this sets our parent to nullptr
	if (get_parent())
		get_parent()->remove_this(this);
	ASSERT(parent == nullptr);
	
	
	if (other) {
		// check if 'other' has 'this' as a parent (circular) 
		auto check_circular = [&]() -> bool {
			Entity* check = other;
			int loop_count = 0;
			while (check) {
				if (check == this)
					return false;
				check = check->get_parent();
				loop_count++;
				ASSERT(loop_count < 100);
			};
			return true;
		};
		ASSERT(check_circular());

		other->children.push_back(this);
		parent = other;
	}

	invalidate_transform(nullptr);
}

void Entity::transform_look_at(glm::vec3 pos, glm::vec3 look_pos)
{
	set_ws_transform(glm::inverse(glm::lookAt(pos, look_pos, glm::vec3(0, 1, 0))));
}
void Entity::set_ls_rotation(glm::quat q) {
	set_ls_transform(get_ls_position(), q, get_ls_scale());
}

void Entity::remove_this_component_internal(Component* component_to_remove)
{
	ASSERT(component_to_remove);
	ASSERT(component_to_remove->entity_owner == this);
	component_to_remove->entity_owner = nullptr;

	auto check_count = [&]() -> int {
		int count = 0;
		for (auto c : all_components)
			if (c == component_to_remove)
				count++;
		return count;
	};
	ASSERT(check_count() == 1);

	for (int i = 0; i < all_components.size(); i++) {
		if (all_components[i] == component_to_remove) {
			all_components.erase(all_components.begin() + i);
			return;
		}
	}
	ASSERT(!"unreachable");
}

void Entity::move_child_entity_index(Entity* who, int move_to)
{
	if (move_to < 0 || move_to >= children.size()) {
		sys_print(Warning, "move_child_entity_index out of range\n");
		return;
	}
	const int my_index = get_child_entity_index(who);
	if (my_index == -1) {
		sys_print(Warning, "move_child_entity_index child doesnt exist\n");
		return;
	}
	int actual_move_to = move_to;
	if (move_to > my_index)
		actual_move_to += 1;

	ASSERT(move_to <= children.size()); // allowed to == size

	children.insert(children.begin() + actual_move_to, who);
	int where_to_remove = (actual_move_to <= my_index) ? my_index + 1 : my_index;
	children.erase(children.begin() + where_to_remove);

	assert(get_child_entity_index(who) == move_to);
}
int Entity::get_child_entity_index(Entity* who) const
{
	auto only_once_in_children = [&]()->bool {
		int c = 0;
		for (int i = 0; i < children.size(); i++) {
			if (children[i] == who)
				c++;
		}
		return c <= 1;
	};
	assert(only_once_in_children());

	for (int i = 0; i < children.size(); i++) {
		if (children[i] == who)
			return i;
	}
	return -1;
}

Entity::~Entity()
{
	ASSERT(init_state != initialization_state::CALLED_START&&init_state!=initialization_state::CALLED_PRE_START);
	//ASSERT(all_components.empty());
}

Component* Entity::create_component(const ClassTypeInfo* info)
{
	ASSERT(init_state != initialization_state::CONSTRUCTOR);
	if (!info) {
		sys_print(Error, "Entity::create_component: null type info\n");
		return nullptr;
	}
	if (!info->is_a(Component::StaticType)) {
		sys_print(Error, "Entity::create_component: not subtype of Component %s\n",info->classname);
		return nullptr;
	}
	Component* ec = (Component*)info->allocate_this_type();
	if (!ec) {
		sys_print(Error, "Entity::create_component: allocate returned null %s\n",info->classname);
		return nullptr;
	}

	ASSERT(ec);

	ec->entity_owner = this;
	all_components.push_back(ec);
	eng->get_level()->add_and_init_created_runtime_component(ec);
	ASSERT(ec->init_state == initialization_state::CALLED_START);
	return ec;
}

Entity* Entity::create_child_entity()
{
	ASSERT(init_state != initialization_state::CONSTRUCTOR);

	Entity* e = eng->get_level()->spawn_entity();
	ASSERT(e);
	e->parent_to(this);

	return e;
}


void Entity::add_component_from_unserialization(Component* component)
{
	ASSERT(component);
	ASSERT(init_state == initialization_state::CONSTRUCTOR);
	ASSERT(component->init_state == initialization_state::CONSTRUCTOR);
	component->entity_owner = this;
	
	auto try_find = [&]() -> bool {
		for (auto& c : all_components)
			if (c == component)
				return true;
		return false;
	};
	ASSERT(!try_find());

	all_components.push_back(component);
}





#include "tracy/public/tracy/Tracy.hpp"

glm::mat4 Entity::get_ls_transform() const
{
	return compose_transform(position,rotation,scale);
}
void Entity::set_ls_transform(const glm::mat4& transform) {
	decompose_transform(transform, position, rotation, scale);
	check_for_transform_nans();
	post_change_transform_R();
}
void Entity::set_ls_transform(const glm::vec3& v, const glm::quat& q, const glm::vec3& scale) {
	position = v;
	rotation = q;
	this->scale = scale;
	check_for_transform_nans();
	post_change_transform_R();
}
void Entity::set_ls_euler_rotation(glm::vec3 euler) {
	rotation = glm::quat(euler);
	post_change_transform_R();
}

void Entity::post_change_transform_R(bool ws_is_dirty, Component* skipthis)
{
	world_transform_is_dirty = ws_is_dirty;

	if (init_state != initialization_state::CALLED_START)
		return;

	for (auto c : all_components)
		c->on_changed_transform();

	// recurse to children
	for (int i = 0; i < children.size(); i++)
		children[i]->post_change_transform_R();
}

void Entity::set_ws_transform(const glm::vec3& v, const glm::quat& q, const glm::vec3& scale)
{
	if (!has_transform_parent()) {
		set_ls_transform(v, q, scale);
	}
	else {
		auto matrix = compose_transform(v, q, scale);
		set_ws_transform(matrix);
	}
}
void Entity::set_ls_position(glm::vec3 v)
{
	position = v;
	post_change_transform_R();
}
void Entity::set_ls_scale(glm::vec3 v)
{
	scale = v;
	post_change_transform_R();
}



void Entity::set_ws_transform(const glm::mat4& transform)
{
	// want local space
	if (has_transform_parent()) {
		const glm::mat4& parent_t = get_parent_transform();
		auto inv_world = glm::inverse(parent_t);
		glm::mat4 local = inv_world * transform;
		glm::vec3 newp;
		decompose_transform(local, newp, rotation, scale);
		if (newp.x != newp.x) {
			sys_print(Error,"ERROR");
		}
		position = newp;

		cached_world_transform = transform;
	}
	else {
		cached_world_transform = transform;
		decompose_transform(transform, position, rotation, scale);
	}
	check_for_transform_nans();
	post_change_transform_R( false /* cached_world_transform doesnt need updating, we already have it*/);
}

glm::mat4 Entity::get_parent_transform() const
{
	Entity* parent = get_parent();
	ASSERT(parent);

	if (!has_parent_bone() || !parent->get_cached_mesh_component()) {
		return parent->get_ws_transform();
	}
	else {
		MeshComponent* cached_mesh = parent->get_cached_mesh_component();
		return parent->get_ws_transform() * cached_mesh->get_ls_transform_of_bone(parent_bone.name);
	}
}
bool Entity::has_parent_bone() const { return !parent_bone.name.is_null(); }
// lazily evalutated
const glm::mat4& Entity::get_ws_transform() {
	if (world_transform_is_dirty) {
		if (has_transform_parent()) {
			cached_world_transform = get_parent_transform() * get_ls_transform();
		}
		else
			cached_world_transform = get_ls_transform();
		world_transform_is_dirty = false;
	}
	return cached_world_transform;
}
glm::vec3 Entity::get_ws_position() {
	if (!parent)
		return position;
	auto& ws = get_ws_transform();
	return ws[3];
}

glm::quat Entity::get_ws_rotation() {
	if (!parent)
		return rotation;
	auto& ws = get_ws_transform();
	return glm::quat_cast(ws);
}

glm::vec3 Entity::get_ws_scale() {
	if (!parent)
		return scale;
	// fixme
	return glm::vec3(1.f);
}

void Entity::set_ws_position(glm::vec3 v) { set_ws_transform(v, get_ws_rotation(), get_ws_scale()); }

void Entity::set_ws_position_rotation(glm::vec3 pos, glm::quat rot)
{
	set_ws_transform(pos, rot, get_ws_scale());	//fixme
}

void Entity::set_ls_position_rotation(glm::vec3 pos, glm::quat rot)
{
	set_ls_transform(pos, rot, get_ls_scale());
}

void Entity::set_is_top_level(bool b)
{
	is_top_level = b;
	world_transform_is_dirty = true;
}

void Entity::invalidate_transform(Component* skipthis)
{
	post_change_transform_R(true,skipthis);
}



class EntityBoneParentStringSerialize : public IPropertySerializer
{
	// Inherited via IPropertySerializer
	virtual std::string serialize(DictWriter& out, const PropertyInfo& info, const void* inst, ClassBase* user) override
	{
		const StringName* ptr_prop = (const StringName*)info.get_ptr(inst);
		return ptr_prop->get_c_str();
	}
	virtual void unserialize(DictParser& in, const PropertyInfo& info, void* inst, StringView token, ClassBase* user,IAssetLoadingInterface*) override
	{
		const StringName* ptr_prop = (const StringName*)info.get_ptr(inst);
		std::string to_str(token.str_start, token.str_len);
		Entity* e = (Entity*)inst;
		e->set_parent_bone(StringName(to_str.c_str()));
	}
};
ADDTOFACTORYMACRO_NAME(EntityBoneParentStringSerialize, IPropertySerializer, "EntityBoneParentString");

#ifdef EDITOR_BUILD


#endif
class EntityTagSerialize : public IPropertySerializer
{
	// Inherited via IPropertySerializer
	virtual std::string serialize(DictWriter& out, const PropertyInfo& info, const void* inst, ClassBase* user) override
	{
		const StringName* ptr_prop = (const StringName*)info.get_ptr(inst);
		return ptr_prop->get_c_str();
	}
	virtual void unserialize(DictParser& in, const PropertyInfo& info, void* inst, StringView token, ClassBase* user, IAssetLoadingInterface*) override
	{
		const StringName* ptr_prop = (const StringName*)info.get_ptr(inst);
		std::string to_str(token.str_start, token.str_len);
		Entity* e = (Entity*)inst;
		e->set_tag(StringName(to_str.c_str()));
	}
};
ADDTOFACTORYMACRO_NAME(EntityTagSerialize, IPropertySerializer, "EntityTagString");

Component* Entity::get_component(const ClassTypeInfo* ti) const {
	if (!ti)
		return nullptr;
	for (int i = 0; i < all_components.size(); i++)
		if (all_components[i]->get_type().is_a(*ti))
			return all_components[i];
	return nullptr;
}