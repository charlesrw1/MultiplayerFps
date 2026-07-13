#include "PhysicsComponents.h"
#include "GameEnginePublic.h"
#include "Physics/Physics2.h"
#include "Physics/Physics2Local.h"
#include "MeshbuilderComponent.h"
#include "Game/Entity.h"

#include <physx/foundation/PxTransform.h>

// Implementations for constraint/joint components:
// PhysicsJointComponent, HingeJointComponent, BallSocketJointComponent, AdvancedJointComponent

using namespace physx;

extern ConfigVar ed_physics_shapes_depth_tested;
extern std::string print_vector(glm::vec3 v);

PhysicsJointComponent::PhysicsJointComponent() {
	set_call_init_in_editor(true);
	set_ticking(false);
}
PhysicsJointComponent::~PhysicsJointComponent() {}

void PhysicsJointComponent::refresh_joint() {
	ASSERT(get_owner());
	free_joint();

	// init joint
	auto self_physics = get_owner_physics();
	if (!self_physics) {
		sys_print(Warning, "Joint component has no physics component\n");
		return;
	}
	PhysicsBody* other = nullptr;
	if (get_target()) {
		other = get_target()->get_component<PhysicsBody>();
		if (!other) {
			sys_print(Warning, "Joint component target has no physics component\n");
		}
	}
	init_joint(self_physics, other /* can be nullptr */);

	if (!get_joint()) {
		sys_print(Error, "couldnt find joint\n");
	}
}

void PhysicsJointComponent::start() {
	ASSERT(get_owner());
	if (eng->is_editor_level()) {
		editor_meshbuilder = get_owner()->create_component<MeshBuilderComponent>();
		editor_meshbuilder->dont_serialize_or_edit = true;
		editor_meshbuilder->use_background_color = true;
		editor_meshbuilder->use_transform = false;
		editor_meshbuilder->depth_tested = ed_physics_shapes_depth_tested.get_bool();
	}

	if (!eng->is_editor_level()) {
		refresh_joint();
	}
}

void PhysicsJointComponent::set_target(Entity* e) {
	ASSERT(e);
	if (e != target.get()) {
		target = e->get_self_ptr();
		refresh_joint();
	}
}

void PhysicsJointComponent::stop() {
	if (editor_meshbuilder)
		editor_meshbuilder->destroy();
}

PhysicsBody* PhysicsJointComponent::get_owner_physics() {
	ASSERT(get_owner());
	return get_owner()->get_component<PhysicsBody>();
}

static glm::mat4 get_transform_joint(JointAnchor anchor, int axis) {
	ASSERT(axis >= 0 && axis <= 2);
	auto local_t = glm::translate(glm::mat4(1), anchor.p) * glm::mat4_cast(anchor.q);
	glm::mat4 m = glm::mat4(1);
	if (axis == 1) {
		m[0] = glm::vec4(0, 1, 0, 0);
		m[1] = glm::vec4(-1, 0, 0, 0);
	} else if (axis == 2) {
		m[0] = glm::vec4(0, 0, 1, 0);
		m[2] = glm::vec4(-1, 0, 0, 0);
	}

	return local_t * m;
}

void PhysicsJointComponent::draw_meshbuilder() {
	ASSERT(editor_meshbuilder);
	glm::mat4 world = get_ws_transform();
	auto local_t = glm::translate(glm::mat4(1), anchor.p);
	world = world * local_t;
	editor_meshbuilder->mb.AddSphere(world[3], 0.05, 10, 10, COLOR_RED);

	if (!get_target())
		return;
	auto other_phys = get_target()->get_component<PhysicsBody>();
	if (!other_phys)
		return;
	world = get_target()->get_ws_transform();
}

template <typename T>
static T* make_joint_shared(const glm::mat4& ws_transform, JointAnchor anchor, JointAnchor target_anchor,
							int local_joint_axis,
							T* (*create_func)(PxPhysics&, PxRigidActor*, const PxTransform&, PxRigidActor*,
											  const PxTransform&),
							PhysicsBody* a, PhysicsBody* b) {
	ASSERT(a);
	T* joint = nullptr;
	auto my_local = get_transform_joint(anchor, local_joint_axis);
	// target_anchor is deliberately NOT folded into my_local -- my_local stays this actor's own
	// fixed attached frame (used as-is below for actor a), while target_anchor only biases the
	// world/other-actor side (see PhysicsComponents.h's set_target_anchor comment for why that
	// split matters: baking a bias into my_local instead gets conjugated away).
	auto target_local = glm::translate(glm::mat4(1), target_anchor.p) * glm::mat4_cast(target_anchor.q);
	auto my_world = ws_transform * my_local * target_local;
	if (b) {
		auto& other_world = b->get_ws_transform();
		auto other_local = glm::inverse(other_world) * my_world;
		joint = create_func(*physics_local_impl->physics_factory, a->get_physx_actor(), glm_to_physx(my_local),
							b->get_physx_actor(), glm_to_physx(other_local));
	} else {
		joint = create_func(*physics_local_impl->physics_factory, a->get_physx_actor(), glm_to_physx(my_local), nullptr,
							glm_to_physx(my_world));
	}
	if (joint)
		joint->setConstraintFlag(PxConstraintFlag::eVISUALIZATION, true);

	return joint;
}

void HingeJointComponent::init_joint(PhysicsBody* a, PhysicsBody* b) {
	ASSERT(!joint);
	ASSERT(get_owner() == a->get_owner());
	joint = make_joint_shared(get_ws_transform(), anchor, target_anchor, local_joint_axis, PxRevoluteJointCreate, a, b);
}
physx::PxJoint* HingeJointComponent::get_joint() const {
	return joint;
}
void HingeJointComponent::free_joint() {
	if (joint) {
		joint->release();
		joint = nullptr;
	}
}

void BallSocketJointComponent::init_joint(PhysicsBody* a, PhysicsBody* b) {
	ASSERT(!joint);
	ASSERT(get_owner() == a->get_owner());
	joint = make_joint_shared(get_ws_transform(), anchor, target_anchor, local_joint_axis, PxSphericalJointCreate, a, b);
}
physx::PxJoint* BallSocketJointComponent::get_joint() const {
	return joint;
}
void BallSocketJointComponent::free_joint() {
	if (joint) {
		joint->release();
		joint = nullptr;
	}
}

void PhysicsJointComponent::on_changed_transform() {
	ASSERT(get_owner());
	if (editor_meshbuilder) {
		editor_meshbuilder->mb.Begin();
		draw_meshbuilder();
		editor_meshbuilder->mb.End();
	}
}
#ifdef EDITOR_BUILD
void PhysicsJointComponent::editor_on_change_property() {
	if (editor_meshbuilder) {
		editor_meshbuilder->mb.Begin();
		draw_meshbuilder();
		editor_meshbuilder->mb.End();
	}
}
#endif

PxJoint* AdvancedJointComponent::get_joint() const {
	return joint;
}
void AdvancedJointComponent::init_joint(PhysicsBody* a, PhysicsBody* b) {
	ASSERT(!joint);
	ASSERT(get_owner() == a->get_owner());
	joint = make_joint_shared(get_ws_transform(), anchor, target_anchor, local_joint_axis, PxD6JointCreate, a, b);
	auto get_jm_enum = [&](JointMotion jm) {
		if (jm == JM::Free)
			return PxD6Motion::eFREE;
		else if (jm == JM::Limited)
			return PxD6Motion::eLIMITED;
		else
			return PxD6Motion::eLOCKED;
	};
	if (!joint)
		return;
	joint->setMotion(PxD6Axis::eX, get_jm_enum(x_motion));
	joint->setMotion(PxD6Axis::eY, get_jm_enum(x_motion));
	joint->setMotion(PxD6Axis::eZ, get_jm_enum(x_motion));
	joint->setMotion(PxD6Axis::eTWIST, get_jm_enum(ang_x_motion));
	// ang_y_motion/ang_y_limit <-> eSWING2, ang_z_motion/ang_z_limit <-> eSWING1 (swapped from the
	// naive same-letter mapping): PxD6Axis::eSWING1 is the actual physical rotation axis about Z,
	// and eSWING2 about Y (verified physically -- torquing a real PxD6Joint with ang_y Locked/ang_z
	// Limited actually rotates around Z, not Y).
	joint->setMotion(PxD6Axis::eSWING1, get_jm_enum(ang_z_motion));
	joint->setMotion(PxD6Axis::eSWING2, get_jm_enum(ang_y_motion));
	joint->setTwistLimit(PxJointAngularLimitPair(twist_limit_min, twist_limit_max, PxSpring(twist_stiff, twist_damp)));
	if (ang_y_limit <= 0)
		ang_y_limit = 0.00001;
	if (ang_z_limit <= 0)
		ang_z_limit = 0.00001;
	joint->setSwingLimit(PxJointLimitCone(ang_z_limit, ang_y_limit, PxSpring(cone_stiff, cone_damp)));
}
void AdvancedJointComponent::free_joint() {
	if (joint) {
		joint->release();
		joint = nullptr;
	}
}
void AdvancedJointComponent::draw_meshbuilder() {
	ASSERT(editor_meshbuilder);
	auto myworld = get_ws_transform();
	auto mylocal = get_transform_joint(anchor, local_joint_axis);
	myworld = myworld * mylocal;

	// draw x angle
	auto& origin = myworld[3];
	float length = 0.4;
	if (ang_x_motion == JM::Limited) {
		glm::vec3 min = glm::vec3(0, sin(twist_limit_min), cos(twist_limit_min)) * length;
		glm::vec3 min_vec = myworld * glm::vec4(min, 1.0);
		editor_meshbuilder->mb.PushLine(origin, min_vec, COLOR_RED);
		glm::vec3 max = glm::vec3(0, sin(twist_limit_max), cos(twist_limit_max)) * length;
		glm::vec3 max_vec = myworld * glm::vec4(max, 1.0);
		editor_meshbuilder->mb.PushLine(origin, max_vec, COLOR_RED);
	}
	if (ang_y_motion == JM::Limited) {
		glm::vec3 min = glm::vec3(sin(-ang_y_limit), 0, cos(-ang_y_limit)) * length;
		glm::vec3 min_vec = myworld * glm::vec4(min, 1.0);
		editor_meshbuilder->mb.PushLine(origin, min_vec, COLOR_GREEN);
		glm::vec3 max = glm::vec3(sin(ang_y_limit), 0, cos(ang_y_limit)) * length;
		glm::vec3 max_vec = myworld * glm::vec4(max, 1.0);
		editor_meshbuilder->mb.PushLine(origin, max_vec, COLOR_GREEN);
	}
	if (ang_z_motion == JM::Limited) {
		glm::vec3 min = glm::vec3(cos(-ang_z_limit), sin(-ang_z_limit), 0) * length;
		glm::vec3 min_vec = myworld * glm::vec4(min, 1.0);
		editor_meshbuilder->mb.PushLine(origin, min_vec, COLOR_BLUE);
		glm::vec3 max = glm::vec3(cos(ang_z_limit), sin(ang_z_limit), 0) * length;
		glm::vec3 max_vec = myworld * glm::vec4(max, 1.0);
		editor_meshbuilder->mb.PushLine(origin, max_vec, COLOR_BLUE);
	}
}

#ifdef EDITOR_BUILD
// FIXME!

#endif
