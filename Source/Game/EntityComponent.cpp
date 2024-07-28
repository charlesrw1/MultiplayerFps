#include "EntityComponent.h"
#include "glm/gtx/euler_angles.hpp"

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
