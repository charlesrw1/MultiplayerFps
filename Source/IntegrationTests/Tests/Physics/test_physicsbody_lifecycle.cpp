// Source/IntegrationTests/Tests/Physics/test_physicsbody_lifecycle.cpp
// Lifecycle/state-machine tests for PhysicsBody. Exercises the funnel in
// apply_actor_config(): static<->dynamic rebuild, kinematic<->dynamic flag toggles,
// enable/disable, configure-before-start, and joint survival across actor rebuild.
// Body type is a single BodyType enum -- there is no illegal state to canonicalize.
#include "IntegrationTests/TestContext.h"
#include "IntegrationTests/TestRegistry.h"
#include "GameEnginePublic.h"
#include "Level.h"
#include "Game/Entity.h"
#include "Game/Components/PhysicsComponents.h"

namespace {

// Spawn a fresh entity with a BoxComponent attached. Caller controls whether to
// configure before the engine ticks (which calls start()).
PhysicsBody* spawn_box(glm::vec3 pos = {0, 5, 0}) {
    auto* ent = eng->get_level()->spawn_entity();
    ent->set_ws_position(pos);
    return ent->create_component<BoxComponent>();
}

} // namespace

// 1) Round-trip Static -> Kinematic -> Dynamic -> Kinematic -> Static. After every
//    transition assert that the underlying actor type matches the configured type.
//    Covers both paths: static<->dynamic rebuild and the in-place kinematic flag toggle.
static TestTask test_static_dynamic_kinematic_round_trip(TestContext& t) {
    eng->load_level("");
    auto* body = spawn_box();
    co_await t.wait_ticks(1); // start() has now run; default is static

    t.check(body->get_is_actor_static(), "default body is static");
    t.check(!body->get_is_actor_kinematic(), "static is not kinematic");

    // Static -> Kinematic (rebuild to RigidDynamic, kinematic flag set)
    body->set_body_type(BodyType::Kinematic);
    t.check(!body->get_is_actor_static(), "Kinematic: actor not static");
    t.check(body->get_is_actor_kinematic(), "Kinematic flag set on actor");

    // Kinematic -> Dynamic (in-place flag clear, no rebuild)
    body->set_body_type(BodyType::Dynamic);
    t.check(!body->get_is_actor_static(), "still a dynamic actor");
    t.check(!body->get_is_actor_kinematic(), "Dynamic: kinematic flag cleared");

    // Dynamic -> Kinematic (in-place flag set)
    body->set_body_type(BodyType::Kinematic);
    t.check(body->get_is_actor_kinematic(), "Kinematic flag set again");

    // Kinematic -> Static (full rebuild to RigidStatic)
    body->set_body_type(BodyType::Static);
    t.check(body->get_is_actor_static(), "Static: actor static again");
    co_return;
}
GAME_TEST("physics/body/static_dynamic_kinematic_round_trip", 10.f, test_static_dynamic_kinematic_round_trip);


// 2) Verify on_actor_type_change reads the CURRENT body_type each time it runs.
//    create_component() calls start() synchronously in non-editor mode, so we use
//    stop()+start() to simulate the deserialization path where fields are set
//    before the actor exists.
static TestTask test_start_picks_up_configured_fields(TestContext& t) {
    eng->load_level("");
    auto* body = spawn_box();
    co_await t.wait_ticks(1);
    body->stop();
    t.require(body->get_physx_actor() == nullptr, "actor cleared after stop");

    // Configure while no actor exists. apply_actor_config() must early-return,
    // and the next start() must pick body_type up via on_actor_type_change().
    body->set_body_type(BodyType::Dynamic);
    t.check(body->get_physx_actor() == nullptr, "no actor materialized from setters while stopped");

    body->start();
    t.check(!body->get_is_actor_static(), "start() applied Dynamic (not static)");
    t.check(!body->get_is_actor_kinematic(), "start() applied Dynamic (not kinematic)");
    co_return;
}
GAME_TEST("physics/body/start_picks_up_configured_fields", 10.f, test_start_picks_up_configured_fields);


// 3) Disable then re-enable: verify the entity's top-level flag is restored on
//    re-enable for a body that drives its own transform (Dynamic).
//    Pre-fix, set_is_enable(true) never restored top-level (asymmetric branch).
static TestTask test_disable_enable_top_level(TestContext& t) {
    eng->load_level("");
    auto* body = spawn_box();
    body->set_body_type(BodyType::Dynamic);
    co_await t.wait_ticks(1);

    t.check(body->get_is_enabled(), "starts enabled");
    t.check(body->get_owner()->get_is_top_level(), "Dynamic body owns its transform");

    body->set_is_enable(false);
    t.check(!body->get_owner()->get_is_top_level(), "disabled body is not top-level");

    body->set_is_enable(true);
    t.check(body->get_owner()->get_is_top_level(), "re-enabled Dynamic restores top-level");
    co_return;
}
GAME_TEST("physics/body/disable_enable_top_level", 10.f, test_disable_enable_top_level);


// 4) Positive test: enable_with_initial_transforms must not trip its ASSERTs
//    on a properly-configured body (Dynamic + initialized).
static TestTask test_enable_with_initial_transforms_smoke(TestContext& t) {
    eng->load_level("");
    auto* body = spawn_box();
    body->set_body_type(BodyType::Dynamic);
    co_await t.wait_ticks(1);
    t.require(body->get_physx_actor() != nullptr, "actor initialized before enable_with_initial_transforms");

    glm::mat4 t0(1.f);
    glm::mat4 t1 = glm::translate(glm::mat4(1.f), glm::vec3(0.1f, 0, 0));
    body->enable_with_initial_transforms(t0, t1, 1.f / 60.f);
    t.check(body->get_is_enabled(), "still enabled");
    t.check(!body->get_is_actor_static() && !body->get_is_actor_kinematic(),
            "still Dynamic after enable_with_initial_transforms");
    co_return;
}
GAME_TEST("physics/body/enable_with_initial_transforms_smoke", 10.f, test_enable_with_initial_transforms_smoke);


// 5) Joint survives an actor rebuild on one of its anchored bodies. Pre-fix the
//    HingeJoint would point at the released old PxRigidActor — undefined behaviour
//    on the next sim step. After the funnel, on_actor_type_change refreshes joints
//    on the entity.
static TestTask test_joint_survives_actor_rebuild(TestContext& t) {
    eng->load_level("");

    // Anchor body (the joint will be hosted here; static initially)
    auto* a_ent = eng->get_level()->spawn_entity();
    a_ent->set_ws_position({0, 5, 0});
    auto* a_body = a_ent->create_component<BoxComponent>();

    // Target body (always Dynamic)
    auto* b_ent = eng->get_level()->spawn_entity();
    b_ent->set_ws_position({1, 5, 0});
    auto* b_body = b_ent->create_component<BoxComponent>();
    b_body->set_body_type(BodyType::Dynamic);

    // Hinge on a_ent, targeting b_ent
    auto* joint = a_ent->create_component<HingeJointComponent>();
    joint->set_target(b_ent);

    co_await t.wait_ticks(2); // start() + joint init

    // Rebuild a_body's actor (Static -> Dynamic). With the fix, on_actor_type_change
    // walks owner's joints and refreshes them against the new actor pointer. Without
    // the fix this would dangle the joint and the next tick would access freed memory.
    a_body->set_body_type(BodyType::Dynamic);

    // Run a few sim steps. Pre-fix this would crash or corrupt under ASan.
    co_await t.wait_ticks(5);

    // Sanity: both bodies still have valid actors.
    t.check(a_body->get_physx_actor() != nullptr, "rebuilt actor is valid");
    t.check(b_body->get_physx_actor() != nullptr, "target actor still valid");
    co_return;
}
GAME_TEST("physics/body/joint_survives_actor_rebuild", 15.f, test_joint_survives_actor_rebuild);


// 6) Canonical enum API: set_body_type drives every transition and the actor type
//    tracks it.
static TestTask test_body_type_enum_round_trip(TestContext& t) {
    eng->load_level("");
    auto* body = spawn_box();
    co_await t.wait_ticks(1);
    t.check(body->get_body_type() == BodyType::Static, "default body_type is Static");

    body->set_body_type(BodyType::Dynamic);
    t.check(body->get_body_type() == BodyType::Dynamic, "set Dynamic");
    t.check(!body->get_is_actor_static() && !body->get_is_actor_kinematic(),
            "Dynamic actor: not static, not kinematic");

    body->set_body_type(BodyType::Kinematic);
    t.check(body->get_body_type() == BodyType::Kinematic, "set Kinematic");
    t.check(body->get_is_actor_kinematic(), "Kinematic flag set on actor");

    body->set_body_type(BodyType::Static);
    t.check(body->get_body_type() == BodyType::Static, "set Static");
    t.check(body->get_is_actor_static(), "static actor type");
    co_return;
}
GAME_TEST("physics/body/body_type_enum_round_trip", 10.f, test_body_type_enum_round_trip);


// 7) Stop then start round-trip: the configured body_type must survive a stop/start
//    cycle (stop() must not reset it).
static TestTask test_stop_then_start_round_trip(TestContext& t) {
    eng->load_level("");
    auto* body = spawn_box();
    body->set_body_type(BodyType::Dynamic);
    co_await t.wait_ticks(1);
    t.require(!body->get_is_actor_static() && !body->get_is_actor_kinematic(),
              "configured to Dynamic");

    body->stop();
    t.check(body->get_physx_actor() == nullptr, "stop() releases the actor");
    t.check(body->get_body_type() == BodyType::Dynamic, "stop() preserves body_type");

    body->start();
    t.check(body->get_physx_actor() != nullptr, "start() re-creates actor");
    t.check(!body->get_is_actor_static() && !body->get_is_actor_kinematic(),
            "re-started body picks up retained Dynamic config");
    co_return;
}
GAME_TEST("physics/body/stop_then_start_round_trip", 10.f, test_stop_then_start_round_trip);


// 8) teleport_to() must NOT mutate velocity. The old set_transform() silently
//    zeroed a dynamic body's velocity every call, which made parented ragdolls
//    "crawl instead of fall" (see RagdollComponent.cpp). This is the core bug fix.
static TestTask test_teleport_preserves_velocity(TestContext& t) {
    eng->load_level("");
    auto* body = spawn_box();
    body->set_body_type(BodyType::Dynamic);
    co_await t.wait_ticks(1);
    t.require(body->get_physx_actor() != nullptr, "actor initialized");

    body->set_linear_velocity(glm::vec3(3, 0, 0));
    body->teleport_to(glm::translate(glm::mat4(1.f), glm::vec3(0, 10, 0)));

    glm::vec3 v = body->get_linear_velocity();
    t.check(glm::length(v - glm::vec3(3, 0, 0)) < 0.01f, "teleport_to preserved linear velocity");
    glm::vec3 p = glm::vec3(body->get_physics_pose()[3]);
    t.check(glm::length(p - glm::vec3(0, 10, 0)) < 0.01f, "teleport_to moved the actor");
    co_return;
}
GAME_TEST("physics/body/teleport_preserves_velocity", 10.f, test_teleport_preserves_velocity);


// 9) Ownership model: a Dynamic body is driven BY physics, so an external Entity
//    transform push must be IGNORED (physics owns the transform). Pre-fix,
//    on_changed_transform teleported the actor to the Entity AND zeroed velocity.
static TestTask test_dynamic_ignores_entity_push(TestContext& t) {
    eng->load_level("");
    auto* body = spawn_box({0, 5, 0});
    body->set_body_type(BodyType::Dynamic);
    co_await t.wait_ticks(1);

    glm::vec3 before = glm::vec3(body->get_physics_pose()[3]);
    body->get_owner()->set_ws_position(glm::vec3(100, 100, 100)); // shove the entity
    glm::vec3 after = glm::vec3(body->get_physics_pose()[3]);

    t.check(glm::length(after - before) < 0.5f,
            "Dynamic body ignored external Entity move (physics owns the transform)");
    co_return;
}
GAME_TEST("physics/body/dynamic_ignores_entity_push", 10.f, test_dynamic_ignores_entity_push);


// 10) Ownership model: a Kinematic body is driven BY the Entity. Moving the Entity
//     issues a swept kinematic target (move_to), and the actor reaches it after a
//     sim step.
static TestTask test_kinematic_follows_entity(TestContext& t) {
    eng->load_level("");
    auto* body = spawn_box({0, 5, 0});
    body->set_body_type(BodyType::Kinematic);
    co_await t.wait_ticks(1);
    t.require(body->get_is_actor_kinematic(), "body is kinematic");

    body->get_owner()->set_ws_position(glm::vec3(2, 5, 0));
    co_await t.wait_ticks(2); // sim advances the kinematic body to its target

    glm::vec3 p = glm::vec3(body->get_physics_pose()[3]);
    t.check(glm::length(p - glm::vec3(2, 5, 0)) < 0.1f, "kinematic actor reached the Entity target");
    co_return;
}
GAME_TEST("physics/body/kinematic_follows_entity", 10.f, test_kinematic_follows_entity);
