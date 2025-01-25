#include "Entity.h"

#include "Framework/Factory.h"
#include "Framework/ArrayReflection.h"
#include "glm/gtx/euler_angles.hpp"

#include "Assets/AssetRegistry.h"
#include "Level.h"
#include "GameEnginePublic.h"

#include "Framework/PropertyEd.h"

CLASS_IMPL(Entity);


// create native entities as a fake "Asset" for drag+drop and double click open to create instance abilities
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

PropertyInfo GetAtomValueWrapper<EntityPtr<Entity>>::get() {
	return make_entity_ptr_property<Entity>("", 0, PROP_DEFAULT, nullptr);
}

// database to map an integer to any type of object, for example models or entities, automatically resolved and editable in the editor

const PropertyInfoList* Entity::get_props() {
	START_PROPS(Entity)
		REG_VEC3(position, PROP_DEFAULT),
		REG_QUAT(rotation, PROP_DEFAULT),
		REG_VEC3(scale, PROP_DEFAULT),
		REG_STDSTRING(editor_name, PROP_DEFAULT),	// edit+serialize
		REG_STRUCT_CUSTOM_TYPE(parent_bone, PROP_DEFAULT, "EntityBoneParentString"),
		REG_BOOL(start_disabled, PROP_DEFAULT,"0")
	END_PROPS(Entity)
}

Entity::Entity()
{

}

void Entity::set_active(bool make_active)
{
	if (make_active == is_activated()) return;
	if (make_active) {
		activate_internal_step1();
		for (auto c : get_all_components())
			c->activate_internal_step1();
		activate_internal_step2();
		for (auto c : get_all_components())
			c->activate_internal_step2();
	}
	else {
		deactivate_internal();
		for (auto c : get_all_components())
			c->deactivate_internal();
	}
	ASSERT(make_active == is_activated());
}

void Entity::initialize_internal_step1()
{
	if(!start_disabled || eng->is_editor_level())
		activate_internal_step1();
}
void Entity::initialize_internal_step2()
{
	if(!start_disabled || eng->is_editor_level())
		activate_internal_step2();
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
	if(init_state == initialization_state::CALLED_START)
		deactivate_internal();

	if (get_entity_parent())
		get_entity_parent()->remove_this(this);
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

void Entity::parent_to_entity(Entity* other)
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

			if (cur_node->get_entity_parent() == this) {
				remove_this(cur_node);
				cur_node->parent_to_entity(parent);	// parent circular child entity to my parent
				break;
			}
			cur_node = cur_node->get_entity_parent();
		}
	}

	if (get_entity_parent())
			get_entity_parent()->remove_this(this);
	
	if (!other)
		return;

	ASSERT(parent == nullptr);

	// check if 'other' has 'this' as a parent (circular) 
	auto check_circular = [&]() -> bool {
		Entity* check = other;
		int loop_count = 0;
		while (check) {
			if (check == this)
				return false;
			check = check->get_entity_parent();
			loop_count++;
			ASSERT(loop_count < 100);
		};
		return true;
	};
	ASSERT(check_circular());


	other->children.push_back(this);
	parent = other;
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

Entity::~Entity()
{
	ASSERT(init_state != initialization_state::CALLED_START&&init_state!=initialization_state::CALLED_PRE_START);
	ASSERT(all_components.empty());
}

EntityComponent* Entity::create_and_attach_component_type(const ClassTypeInfo* info)
{
	ASSERT(init_state != initialization_state::CONSTRUCTOR);

	if (!info->is_a(EntityComponent::StaticType)) {
		sys_print(Error, "create_and_attach_component_type not subclass of entity component\n");
		return nullptr;
	}
	EntityComponent* ec = (EntityComponent*)info->allocate();
	ec->entity_owner = this;
	all_components.push_back(ec);
	eng->get_level()->add_and_init_created_runtime_component(ec);
	ASSERT(ec->init_state == initialization_state::CALLED_START);
	return ec;
}

Entity* Entity::create_and_attach_entity(const ClassTypeInfo* info)
{
	ASSERT(init_state != initialization_state::CONSTRUCTOR);

	Entity* e = eng->get_level()->spawn_entity_from_classtype(*info);
	ASSERT(e);
	e->parent_to_entity(this);

	return e;
}


void Entity::add_component_from_unserialization(EntityComponent* component)
{
	ASSERT(init_state == initialization_state::CONSTRUCTOR);
	ASSERT(component->init_state == initialization_state::CONSTRUCTOR);
	component->entity_owner = this;
	all_components.push_back(component);
}

static void decompose_transform(const glm::mat4& transform, glm::vec3& p, glm::quat& q, glm::vec3& s)
{
	s = glm::vec3(glm::length(transform[0]), glm::length(transform[1]), glm::length(transform[2]));
	q = (glm::quat_cast(transform));
	p = transform[3];
}

static glm::mat4 compose_transform(const glm::vec3& v, const glm::quat& q, const glm::vec3& s)
{
	glm::mat4 model;
	model = glm::translate(glm::mat4(1), v);
	model = model * glm::mat4_cast(q);
	model = glm::scale(model, glm::vec3(s));
	return model;
}



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
void Entity::set_ls_euler_rotation(const glm::vec3& euler) {
	rotation = glm::quat(euler);
	post_change_transform_R();
}

void Entity::post_change_transform_R(bool ws_is_dirty, EntityComponent* skipthis)
{
	world_transform_is_dirty = ws_is_dirty;

	if (init_state != initialization_state::CALLED_START)
		return;

	for (auto c : all_components)
		if(c!=skipthis)
			c->on_changed_transform();

	// recurse to children
	for (int i = 0; i < children.size(); i++)
		children[i]->post_change_transform_R();
}

void Entity::set_ws_transform(const glm::vec3& v, const glm::quat& q, const glm::vec3& scale)
{
	if (!get_entity_parent()) {
		set_ls_transform(v, q, scale);
		return;
	}

	auto matrix = compose_transform(v, q, scale);
	set_ws_transform(matrix);
	post_change_transform_R();
}


void Entity::set_ws_transform(const glm::mat4& transform)
{
	// want local space
	if (get_entity_parent()) {
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
#include "Game/Components/MeshComponent.h"
#include "Animation/SkeletonData.h"
glm::mat4 Entity::get_parent_transform() const
{
	ASSERT(get_entity_parent());
	if (!has_parent_bone() || !get_entity_parent()->get_cached_mesh_component()) {
		return get_entity_parent()->get_ws_transform();
	}
	else {
		return
			get_entity_parent()->get_ws_transform()
			* get_entity_parent()->get_cached_mesh_component()->get_ls_transform_of_bone(parent_bone.name);

	}
}
// lazily evalutated
const glm::mat4& Entity::get_ws_transform() {
	if (world_transform_is_dirty) {
		if (get_entity_parent()) {
			cached_world_transform = get_parent_transform() * get_ls_transform();
		}
		else
			cached_world_transform = get_ls_transform();
		world_transform_is_dirty = false;
	}
	return cached_world_transform;
}

void Entity::invalidate_transform(EntityComponent* skipthis)
{
	post_change_transform_R(true,skipthis);
}


class EntityBoneParentStringEditor : public IPropertyEditor
{
public:
	// Inherited via IPropertyEditor
	virtual bool internal_update() override
	{
		Entity* self = (Entity*)instance;
		if (!has_init) {
			Entity* parent = self->get_entity_parent();
			if (parent) {
				MeshComponent* mc = parent->get_first_component<MeshComponent>();
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