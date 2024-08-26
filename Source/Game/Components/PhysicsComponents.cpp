#include "PhysicsComponents.h"
#include "GameEnginePublic.h"
#include "Physics/Physics2.h"

CLASS_IMPL(BoxComponent);
CLASS_IMPL(CapsuleComponent);

void CapsuleComponent::on_init() {
	actor = g_physics->allocate_physics_actor(this);
	auto preset = (physics_preset.ptr) ? (PhysicsFilterPresetBase*)physics_preset.ptr->default_class_object : nullptr;
	actor->init_physics_shape(preset, get_ws_transform(), simulate_physics, send_overlap, send_hit, is_static, is_trigger, false);
	actor->add_vertical_capsule_to_actor(glm::vec3(0.f), height, radius);
}
void CapsuleComponent::on_deinit() {
	g_physics->free_physics_actor(actor);
}
void CapsuleComponent::on_changed_transform() {
	assert(actor);
	actor->set_transform(get_ws_transform());
}