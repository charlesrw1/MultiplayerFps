#include "Entity.h"

#include "EntityTypes.h"
#include "Framework/Factory.h"
#include "Physics/Physics2.h"

#include "Animation/Runtime/Animation.h"
#include "Render/Material.h"

#include "Framework/ArrayReflection.h"
#include "Game/Schema.h"


#include "glm/gtx/euler_angles.hpp"

#include "GameEnginePublic.h"
#include "DrawPublic.h"

CLASS_IMPL(Entity);

CLASS_IMPL(Door);
CLASS_IMPL(Grenade);
CLASS_IMPL(NPC);

CLASS_IMPL(EntityComponent);

CLASS_IMPL(EmptyComponent);
CLASS_IMPL(MeshComponent);
CLASS_IMPL(BoxComponent);
CLASS_IMPL(CapsuleComponent);



// database to map an integer to any type of object, for example models or entities, automatically resolved and editable in the editor

const PropertyInfoList* Entity::get_props() {
	START_PROPS(Entity)
		REG_ENTITY_COMPONENT_PTR(root_component, PROP_SERIALIZE),
		REG_ENTITY_PTR(self_id,PROP_SERIALIZE)
	END_PROPS(Entity)
}

Entity::Entity()
{
}
const EntityComponent* Entity::get_default_component_for_string_name(const std::string& name) const
{
	auto default_obj = get_type().default_class_object;
	const ClassTypeInfo* ti = &get_type();
	InlineVec<const PropertyInfoList*, 10> allprops;
	for (; ti; ti = ti->super_typeinfo) {
		if (ti->props)
			allprops.push_back(ti->props);
	}
	for (int i = 0; i < allprops.size(); i++) {
		auto props = allprops[i];
		for (int j = 0; j < props->count; j++) {
			auto& p = props->list[j];
			if (strcmp(props->list[j].custom_type_str, "EntityComponent") == 0)
			{
				EntityComponent* ec = (EntityComponent*)props->list[j].get_ptr(this);
				if (ec->eSelfNameString == name)
					return ec;
			}
		}
	}
	return nullptr;
}
void Entity::register_components()
{
	// by the time you get here, root must be something
	// either defined in a C++ class, or the editor must have added an instance component to it
	assert(root_component.get());

	auto level = eng->get_level();
	//const bool is_editor_level = level->is_editor_level();

	// find all EntityComponent INLINED fields (part of classtype)

	// now run register functions
	for (int i = 0; i < all_components.size(); i++) {
		// insert logic for editor only components here
		auto& c = all_components[i];
		assert(c->attached_parent.get() || root_component.get() == c.get());
		c->on_init();
	}

}

void Entity::destroy()
{
	for (int i = 0; i < all_components.size(); i++)
		all_components[i]->on_deinit();
	all_components.clear();	// deletes all
}


void Entity::update_entity_and_components() {
	// call the entity tick function
	update();

	// tick components, update renderables, animations etc.
	for (int i = 0; i < all_components.size(); i++)
		all_components[i]->on_tick();
}
void EntityComponent::remove_this(EntityComponent* child_component)
{
#ifdef _DEBUG
	bool found = false;
	for (int i = 0; i < children.size(); i++) {
		if (children[i] == child_component) {
			if (found)
				assert(!"component was added twice");
			children.erase(children.begin() + i);
			i--;
			found = true;
		}
	}
#else
	for (int i = 0; i < children.size(); i++) {
		if (children[i] == child_component) {
			children.erase(children.begin() + i);
			return;
		}
	}
#endif
	assert(!"component couldn't be found to remove in remove_this");
}
void EntityComponent::attach_to_parent(EntityComponent* parent_component, StringName point)
{
	if (attached_parent.get()) {
		remove_this(attached_parent.get());
		attached_parent = nullptr;
	}
	parent_component->children.push_back(this);
	attached_parent = parent_component;
	attached_bone_name = point;

}
void EntityComponent::unlink_and_destroy() 
{
	if (attached_parent.get())
		attached_parent->remove_this(this);
	for (int i = 0; i < children.size(); i++)
		children[i]->destroy_children_no_unlink();
	on_deinit();
}
void EntityComponent::destroy_children_no_unlink()
{
	for (int i = 0; i < children.size(); i++)
		children[i]->destroy_children_no_unlink();
	on_deinit();
	entity_owner->remove_this_component(this);
}

#include "Animation/Runtime/Animation.h"
#include "Animation/AnimationTreePublic.h"


glm::mat4 EntityComponent::get_local_transform()
{
	mat4 model;
	model = glm::translate(mat4(1), position);
	model = model * glm::mat4_cast(rotation);
	model = glm::scale(model, vec3(1.f));

	return model;
}

MeshComponent::~MeshComponent()
{
	assert(!animator && !draw_handle.is_valid());
}
MeshComponent::MeshComponent() {}
void MeshComponent::set_model(const char* model_path)
{
	model = mods.find_or_load(model_path);
}

const PropertyInfoList* MeshComponent::get_props() {
#ifndef RUNTIME
	MAKE_VECTORCALLBACK_ATOM(AssetPtr<Material>, eMaterialOverride);
#endif // !RUNTIME
	MAKE_VECTORCALLBACK_ATOM(AssetPtr<Material>, MaterialOverride_compilied)

		auto t = &Model::StaticType.classname;
		const char* str = Model::StaticType.classname;
	START_PROPS(MeshComponent)
		REG_ASSET_PTR(model, PROP_DEFAULT),
		REG_ASSET_PTR(animator_tree, PROP_DEFAULT),

#ifndef RUNTIME
		REG_STDVECTOR(eMaterialOverride, PROP_DEFAULT | PROP_EDITOR_ONLY),
		REG_BOOL(eAnimateInEditor, PROP_DEFAULT | PROP_EDITOR_ONLY, "0"),
#endif // !RUNTIME

		REG_BOOL(simulate_physics, PROP_DEFAULT, "0"),
	END_PROPS(MeshCompponent)
}

void MeshComponent::editor_on_change_property(const PropertyInfo& property_)
{
	bool remake = false;
	void* prop_ptr = (void*)property_.get_ptr(this);
	if (prop_ptr == &model) {
		remake = true;
	}
	else if (prop_ptr == &visible)
		remake = true;
	else if (prop_ptr == &eMaterialOverride)
		remake = true;
}

void MeshComponent::on_init()
{
	draw_handle = idraw->register_obj();
	if (model.get()) {
		if (model->get_skel() && animator_tree.get() && animator_tree->get_graph_is_valid()) {
			assert(animator_tree->get_script());
			assert(animator_tree->get_script()->get_native_class());
			assert(animator_tree->get_script()->get_native_class()->allocate);

			ClassBase* c = animator_tree->get_script()->get_native_class()->allocate();
			assert(c->is_a<AnimatorInstance>());
			animator.reset(c->cast_to<AnimatorInstance>());

			bool good = animator->initialize_animator(model.get(), animator_tree.get(), get_owner());
			if (!good) {
				sys_print("!!! couldnt initialize animator\n");
				animator.reset(nullptr);	// free animator
				animator_tree = nullptr;	// free tree reference
			}
		}

		Render_Object obj;
		obj.model = model.get();
		obj.visible = visible;
		obj.transform = get_local_transform();

		idraw->update_obj(draw_handle, obj);
	}
}
void MeshComponent::on_tick()
{

}
void MeshComponent::on_deinit()
{
	idraw->remove_obj(draw_handle);
	animator.reset();
}

Entity::~Entity()
{


}

glm::mat4 Entity::get_world_transform()
{
	mat4 model;
	model = glm::translate(mat4(1), position);
	model = model * glm::eulerAngleXYZ(rotation.x, rotation.y, rotation.z);
	model = glm::scale(model, vec3(1.f));

	return model;
}