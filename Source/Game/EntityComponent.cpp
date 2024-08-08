#include "EntityComponent.h"
#include "glm/gtx/euler_angles.hpp"
#include "Entity.h"

CLASS_IMPL(EntityComponent);
CLASS_IMPL(EmptyComponent);

static void decompose_transform(const glm::mat4& transform, glm::vec3& p, glm::quat& q, glm::vec3& s)
{
	s = glm::vec3(glm::length(transform[0]), glm::length(transform[1]), glm::length(transform[2]));
	q = glm::normalize(glm::quat_cast(transform));
	p = transform[3];
}

glm::mat4 EntityComponent::get_ls_transform() const
{
	glm::mat4 model;
	model = glm::translate(glm::mat4(1), position);
	model = model * glm::mat4_cast(rotation);
	model = glm::scale(model, glm::vec3(scale));

	return model;
}
void EntityComponent::set_ls_transform(const glm::mat4& transform) {
	decompose_transform(transform, position, rotation, scale);
	post_change_transform_R();
}
void EntityComponent::set_ls_transform(const glm::vec3& v, const glm::quat& q, const glm::vec3& scale) {
	position = v;
	rotation = q;
	this->scale = scale;
	post_change_transform_R();
}
void EntityComponent::set_ls_euler_rotation(const glm::vec3& euler) {
	rotation = glm::quat(euler);
	post_change_transform_R();
}

void EntityComponent::post_change_transform_R(bool ws_is_dirty)
{
	world_transform_is_dirty = ws_is_dirty;
	on_changed_transform();	// call down to derived
	// recurse to children
	for (int i = 0; i < children.size(); i++)
		children[i]->post_change_transform_R();
}


void EntityComponent::set_ws_transform(const glm::mat4& transform)
{
	// want local space
	if (attached_parent.get()) {
		auto inv_world = glm::inverse(attached_parent->get_ws_transform());
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

// lazily evalutated
const glm::mat4& EntityComponent::get_ws_transform() {
	if (world_transform_is_dirty) {
		if (attached_parent.get())
			cached_world_transform = attached_parent->get_ws_transform() * get_ls_transform();
		else
			cached_world_transform = get_ls_transform();
		world_transform_is_dirty = false;
	}
	return cached_world_transform;
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
	assert(found && "component couldn't be found to remove in remove_this");
	return;
#else
	for (int i = 0; i < children.size(); i++) {
		if (children[i] == child_component) {
			children.erase(children.begin() + i);
			return;
		}
	}
	assert("component couldn't be found to remove in remove_this");
#endif
}

void EntityComponent::post_unserialize_created_component(Entity* parent)
{
	parent->add_component_from_loading(this);	// add the component to the list (doesnt initalize it yet)
	if (attached_parent.get())	// set the parent if it got serialized, might redo this to make it clearer
		attached_parent->children.push_back(this);
}

void EntityComponent::attach_to_parent(EntityComponent* parent_component, StringName point)
{
	ASSERT(parent_component);

	// prevents circular parent creations
	// checks the node we are parenting to's tree to see if THIS is one of the parent nodes
	EntityComponent* cur_node = parent_component;
	while (cur_node) {

		if (cur_node->get_parent_component() == this) {
			ASSERT(attached_parent.get());
			remove_this(cur_node);
			cur_node->attached_parent = {};
			cur_node->attach_to_parent(attached_parent.get());
			break;
		}
		cur_node = cur_node->attached_parent.get();
	}

	if (attached_parent.get()) {
		attached_parent->remove_this(this);
		attached_parent = nullptr;
	}
	parent_component->children.push_back(this);
	attached_parent = parent_component;
	//attached_bone_name = point;

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
