#include "PhysicsComponents.h"
#include "GameEnginePublic.h"
#include "Physics/Physics2.h"
#include "MeshbuilderComponent.h"
#include "MeshComponent.h"
#include "Render/Model.h"

// Implementations for concrete collision shape components:
// CapsuleComponent, BoxComponent, SphereComponent, MeshColliderComponent

static Color32 mb_color = Color32(86, 150, 252);

void CapsuleComponent::add_actor_shapes() {
	ASSERT(radius > 0.f);
	add_vertical_capsule_to_actor(glm::vec3(0, height_offset, 0), height, radius);
}

void CapsuleComponent::add_editor_shapes() {
	ASSERT(get_editor_meshbuilder());
	auto mb = get_editor_meshbuilder();
	mb->mb.AddLineCapsule(glm::vec3(0, height_offset, 0), radius, height * 0.5, mb_color);
}

void BoxComponent::add_editor_shapes() {
	ASSERT(get_editor_meshbuilder());
	auto mb = get_editor_meshbuilder();
	mb->mb.PushLineBox(glm::vec3(-0.5f), glm::vec3(0.5), mb_color);
}

void SphereComponent::add_editor_shapes() {
	ASSERT(get_editor_meshbuilder());
	auto mb = get_editor_meshbuilder();
	mb->mb.AddLineSphere(glm::vec3(0.f), radius, mb_color);
}

void BoxComponent::add_actor_shapes() {
	ASSERT(get_owner());
	add_box_shape_to_actor(glm::mat4(1.f), get_owner()->get_ls_scale() * 0.5f);
}

void SphereComponent::add_actor_shapes() {
	ASSERT(get_owner());
	auto scale = get_owner()->get_ls_scale();
	auto scale_sz = glm::max(scale.x, glm::max(scale.y, scale.z));
	add_sphere_shape_to_actor(glm::vec3(0.f), radius * scale_sz);
}

void MeshColliderComponent::add_actor_shapes() {
	ASSERT(get_owner());
	auto mesh = get_owner()->get_component<MeshComponent>();
	if (!mesh || !mesh->get_model())
		sys_print(Error, "MeshColliderComponent couldnt find MeshComponent");
	else
		add_model_shape_to_actor(mesh->get_model());
}
