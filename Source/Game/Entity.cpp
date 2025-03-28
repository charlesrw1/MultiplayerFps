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

#include "EntityPtrArrayMacro.h"
#include "EntityPtrMacro.h"

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
			if (subclasses.get_type()->allocate) {
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


	virtual bool assets_are_filepaths() const { return false; }
};
REGISTER_ASSETMETADATA_MACRO(EntityTypeMetadata);
#endif

Entity* EntityPtr::get() const {
	if (handle == 0) return nullptr;
	auto e = eng->get_object(handle);
	return e ? e->cast_to<Entity>() : nullptr;
}
EntityPtr::EntityPtr(const Entity* e)
{
	if (e) {
		handle = e->get_instance_id();
	}
	else
		handle = 0;
}

PropertyInfo GetAtomValueWrapper<EntityPtr>::get() {
	return make_entity_ptr_property("", 0, PROP_DEFAULT);
}

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

void Entity::activate()
{
	if (is_activated()) {
		sys_print(Warning, "activate called on already activated entity\n");
		return;
	}
	ASSERT(start_disabled);
	{
		for (auto comp : get_components())
			comp->activate_internal_step1();
		for (auto c : get_children())
			Entity::set_active_R(c,true, true);
		for (auto comp : get_components())
			comp->activate_internal_step2();
		for (auto c : get_children())
			Entity::set_active_R(c, true, false);
	}
	ASSERT(is_activated());
}

void Entity::initialize_internal()
{
	ASSERT(init_state == initialization_state::HAS_ID);
	if(!start_disabled || eng->is_editor_level())
		init_state = initialization_state::CALLED_START;
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


void Entity::remove_this_component_internal(EntityComponent* component_to_remove)
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

EntityComponent* Entity::create_component_type(const ClassTypeInfo* info)
{
	ASSERT(init_state != initialization_state::CONSTRUCTOR);

	if (!info->is_a(EntityComponent::StaticType)||!info->allocate) {
		sys_print(Error, "create_and_attach_component_type not subclass of entity component or isnt createable\n");
		return nullptr;
	}
	EntityComponent* ec = (EntityComponent*)info->allocate();
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


void Entity::add_component_from_unserialization(EntityComponent* component)
{
	ASSERT(component);
	ASSERT(init_state == initialization_state::CONSTRUCTOR);
	ASSERT(component->init_state == initialization_state::CONSTRUCTOR);
	component->entity_owner = this;
	all_components.push_back(component);
}


static void decompose_transform(const glm::mat4& transform, glm::vec3& p, glm::quat& q, glm::vec3& s)
{
	p = transform[3];
	s = glm::vec3(glm::length(transform[0]), glm::length(transform[1]), glm::length(transform[2]));
	q = glm::quat_cast(glm::mat3(
		transform[0] / s.x,
		transform[1] / s.y,
		transform[2] / s.z
	));
}

static glm::mat4 compose_transform(const glm::vec3& v, const glm::quat& q, const glm::vec3& s)
{
	glm::mat4 model;
	model = glm::translate(glm::mat4(1), v);
	model = model * glm::mat4_cast(q);
	model = glm::scale(model, glm::vec3(s));
	return model;
}

#include "tracy/public/tracy/Tracy.hpp"

glm::mat4 Entity::get_ls_transform() const
{
	return compose_transform(position,rotation,scale);
}
void Entity::set_ls_transform(const glm::mat4& transform) {
	decompose_transform(transform, position, rotation, scale);
	post_change_transform_R();
}
void Entity::set_ls_transform(const glm::vec3& v, const glm::quat& q, const glm::vec3& scale) {
	position = v;
	rotation = q;
	this->scale = scale;
	post_change_transform_R();
}
void Entity::set_ls_euler_rotation(glm::vec3 euler) {
	rotation = glm::quat(euler);
	post_change_transform_R();
}

void Entity::post_change_transform_R(bool ws_is_dirty, EntityComponent* skipthis)
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
		auto inv_world = glm::inverse(get_parent_transform());
		glm::mat4 local = inv_world * transform;
		decompose_transform(local, position, rotation, scale);
		cached_world_transform = transform;
	}
	else {
		cached_world_transform = transform;
		decompose_transform(transform, position, rotation, scale);
	}
	post_change_transform_R( false /* cached_world_transform doesnt need updating, we already have it*/);
}

glm::mat4 Entity::get_parent_transform() const
{
	ASSERT(get_parent());
	if (!has_parent_bone() || !get_parent()->get_cached_mesh_component()) {
		return get_parent()->get_ws_transform();
	}
	else {
		return
			get_parent()->get_ws_transform()
			* get_parent()->get_cached_mesh_component()->get_ls_transform_of_bone(parent_bone.name);

	}
}
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

void Entity::set_is_top_level(bool b)
{
	is_top_level = b;
	world_transform_is_dirty = true;
}

void Entity::invalidate_transform(EntityComponent* skipthis)
{
	post_change_transform_R(true,skipthis);
}

#ifdef EDITOR_BUILD
class EntityBoneParentStringEditor : public IPropertyEditor
{
public:
	// Inherited via IPropertyEditor
	virtual bool internal_update() override
	{
		Entity* self = (Entity*)instance;
		if (!has_init) {
			Entity* parent = self->get_parent();
			if (parent) {
				MeshComponent* mc = parent->get_component<MeshComponent>();
				if (mc && mc->get_model() && mc->get_model()->get_skel()) {
					const Model* mod = mc->get_model();
					auto skel = mod->get_skel();
					auto& allbones = skel->get_all_bones();
					for (auto& b : allbones) {
						options.push_back(b.strname);
					}
				}
			}
			has_init = true;
		}

		if (options.empty()) {
			ImGui::Text("No options (add a MeshComponent with a skeleton)");
			return false;
		}

		bool has_update = false;

		BoneParentStruct* my_struct = (BoneParentStruct*)prop->get_ptr(instance);

		const char* preview = (!my_struct->string.empty()) ? my_struct->string.c_str() : "<empty>";
		if (ImGui::BeginCombo("##combocalsstype", preview)) {
			for (auto& option : options) {

				if (ImGui::Selectable(option.c_str(),
					my_struct->string == option
				)) {
					self->set_parent_bone(option);
					has_update = true;
				}

			}
			ImGui::EndCombo();
		}

		return has_update;
	}
	bool has_init = false;
	std::vector<std::string> options;
};

ADDTOFACTORYMACRO_NAME(EntityBoneParentStringEditor, IPropertyEditor, "EntityBoneParentString");
#endif

class EntityBoneParentStringSerialize : public IPropertySerializer
{
	// Inherited via IPropertySerializer
	virtual std::string serialize(DictWriter& out, const PropertyInfo& info, const void* inst, ClassBase* user) override
	{
		const BoneParentStruct* ptr_prop = (const BoneParentStruct*)info.get_ptr(inst);
		return ptr_prop->string;
	}
	virtual void unserialize(DictParser& in, const PropertyInfo& info, void* inst, StringView token, ClassBase* user) override
	{
		const BoneParentStruct* ptr_prop = (const BoneParentStruct*)info.get_ptr(inst);
		std::string to_str(token.str_start, token.str_len);
		Entity* e = (Entity*)inst;
		e->set_parent_bone(to_str);
	}
};
ADDTOFACTORYMACRO_NAME(EntityBoneParentStringSerialize, IPropertySerializer, "EntityBoneParentString");

class GameTagManager
{
public:
	static GameTagManager& get() {
		static GameTagManager inst;
		return inst;
	}
	void add_tag(const std::string& tag) {
		registered_tags.insert(tag);
	}
	std::unordered_set<std::string> registered_tags;
};

DECLARE_ENGINE_CMD(REG_GAME_TAG)
{
	if (args.size() != 2) {
		sys_print(Error, "REG_GAME_TAG <tag>");
		return;
	}
	GameTagManager::get().add_tag(args.at(1));
}
#ifdef EDITOR_BUILD
class EntityTagEditor : public IPropertyEditor
{
public:
	// Inherited via IPropertyEditor
	virtual bool internal_update() override
	{
		Entity* self = (Entity*)instance;
		if (!has_init) {

			auto& all_tags = GameTagManager::get().registered_tags;
			for (auto& t : all_tags)
				options.push_back(t);

			has_init = true;
		}

		if (options.empty()) {
			ImGui::Text("No options, add tag in init.txt with REG_GAME_TAG <tag>");
			return false;
		}

		bool has_update = false;

		TagStruct* my_struct = (TagStruct*)prop->get_ptr(instance);

		const char* preview = (!my_struct->string.empty()) ? my_struct->string.c_str() : "<untagged>";
		if (ImGui::BeginCombo("##combocalsstype", preview)) {
			for (auto& option : options) {

				if (ImGui::Selectable(option.c_str(),
					my_struct->string == option
				)) {
					self->set_tag(option);
					has_update = true;
				}

			}
			ImGui::EndCombo();
		}

		return has_update;
	}
	bool has_init = false;
	std::vector<std::string> options;
};

ADDTOFACTORYMACRO_NAME(EntityTagEditor, IPropertyEditor, "EntityTagString");
#endif
class EntityTagSerialize : public IPropertySerializer
{
	// Inherited via IPropertySerializer
	virtual std::string serialize(DictWriter& out, const PropertyInfo& info, const void* inst, ClassBase* user) override
	{
		const TagStruct* ptr_prop = (const TagStruct*)info.get_ptr(inst);
		return ptr_prop->string;
	}
	virtual void unserialize(DictParser& in, const PropertyInfo& info, void* inst, StringView token, ClassBase* user) override
	{
		const TagStruct* ptr_prop = (const TagStruct*)info.get_ptr(inst);
		std::string to_str(token.str_start, token.str_len);
		Entity* e = (Entity*)inst;
		e->set_tag(to_str);
	}
};
ADDTOFACTORYMACRO_NAME(EntityTagSerialize, IPropertySerializer, "EntityTagString");

EntityComponent* Entity::get_component_typeinfo(const ClassTypeInfo* ti) const {
	if (!ti)
		return nullptr;
	for (int i = 0; i < all_components.size(); i++)
		if (all_components[i]->get_type().is_a(*ti))
			return all_components[i];
	return nullptr;
}