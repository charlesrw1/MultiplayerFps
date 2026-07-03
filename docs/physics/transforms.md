# Physics transforms & triggers

PhysX wrapper. Bodies are `Component`s: `PhysicsBody` base +
`CapsuleComponent`/`BoxComponent`/`SphereComponent`/`MeshColliderComponent`.
Source: `Source/Game/Components/PhysicsComponents.{h,cpp}`, scene/filter in
`Source/Physics/Physics2Local.cpp`.

## Transform ownership model (the #1 source of confusion)

A body has ONE of two relationships with its Entity's transform, chosen by
`get_body_type()`:

- **Static / Kinematic → the Entity drives the body.** Move the Entity
  (`Entity::set_ws_transform` / `set_ws_position`) and the body follows.
  `on_changed_transform` pushes it into PhysX: Static snaps, Kinematic sweeps.
- **Dynamic (simulating) → PhysX drives the Entity.** Each sim step the solver
  writes the pose back into the Entity (`fetch_new_transform`). An external
  Entity move is **ignored** by the body. To read a Dynamic body use
  `entity->get_ws_transform()`; to reposition it call `teleport_to()`.

Rule of thumb: never push Entity transforms into a Dynamic body every frame —
it fights the solver and does nothing useful.

## Transform API — one method, one meaning

| Method | What it does | Body types | Velocity |
|---|---|---|---|
| `teleport_to(mat4)` | instant `setGlobalPose` | any | **preserved** |
| `move_to(mat4)` | swept `setKinematicTarget` (generates contacts) | Kinematic only (asserts) | n/a |
| `get_physics_pose()` | PhysX world pose, **no scale** | any | — |
| `set_linear_velocity/set_angular_velocity` | set velocity | Dynamic | sets |
| `apply_impulse/apply_force(worldPos, v)` | force/impulse at a world point | Dynamic | adds |

Neither `teleport_to` nor `move_to` mutates velocity. (The old `set_transform`
silently zeroed a Dynamic body's velocity — that was the ragdoll "crawl instead
of fall" bug. It is now a deprecated shim; do not use it.)

## Body type

`enum BodyType { Static, Kinematic, Dynamic }` via `get/set_body_type()` — the
sole body-type API (C++ and Lua). Internally: `Static` = RigidStatic;
`Kinematic` = RigidDynamic+eKINEMATIC (code-driven); `Dynamic` = RigidDynamic
(simulated). Stored as one `BodyType body_type` field, so illegal combos are
unrepresentable. `set_is_enable(bool)` is orthogonal — it gates whether the
actor ticks at all. Lua uses the `BODYTYPE_STATIC/KINEMATIC/DYNAMIC` constants.

The old bool API (`set_is_static/set_is_simulating/set_is_kinematic`) has been
removed; use `set_body_type`.

## Scale (known limitation)

Collision shapes bake `entity->get_ls_scale()` **once** at shape-creation time.
PhysX poses are rigid (position+rotation only), so `teleport_to`/`move_to` strip
scale. Rescaling an Entity at runtime does NOT resize its collision shape. Set
the scale before the body starts, or rebuild the shapes.

## Unity / Unreal → CsRemake cheat-sheet

| Unity | Unreal | CsRemake |
|---|---|---|
| `Rigidbody.position =` | `SetWorldLocation(…, TeleportPhysics)` | `teleport_to()` |
| `Rigidbody.MovePosition()` | (kinematic sweep) | `move_to()` |
| `Rigidbody.isKinematic` | Mobility Movable/Stationary | `BodyType::Kinematic` |
| static collider (no Rigidbody) | Mobility Static | `BodyType::Static` |
| simulated Rigidbody | `SetSimulatePhysics(true)` | `BodyType::Dynamic` |
| `Rigidbody.linearVelocity` | `GetPhysicsLinearVelocity` | `get/set_linear_velocity` |
| `AddForceAtPosition` | `AddForceAtLocation` | `apply_force(worldPos, f)` |
| `Collider.isTrigger` | overlap volume | `set_is_trigger(true)` |
| `Physics.Raycast` | `LineTraceSingle` | `g_physics.trace_ray` |
| layer collision matrix | collision channels | `PL` + `set_physics_layer` |

Not yet implemented (agents will reach for these — they are gaps, not hidden
API): gravity toggle, linear/angular damping, `set_mass`, torque, center-of-mass
force helpers, sleep/wake, axis-lock constraints, CCD.

## Triggers — rules & sharp edges

Set `set_is_trigger(true)`. Overlap enter/leave is delivered via
`Component::on_trigger(Entity* other, bool entered)`.

- **Kinematic pairs fire.** PhysX 4 always processes kinematic-static and
  kinematic-kinematic pairs (the old `PxSceneFlag::eENABLE_KINEMATIC_*_PAIRS`
  flags were removed in 4.0), so a Static trigger volume DOES fire against a
  Kinematic controller-driven character. Only the filter shader gates it.
- **Triggers respect the layer matrix.** A trigger's `physics_layer` filters
  overlaps via `get_collision_mask_for_physics_layer` (same as contacts). Only
  `PhysicsObject` is restricted today; every other layer overlaps all.
- **Trigger vs trigger never reports** (PhysX rule). At least one side must be a
  non-trigger shape.
- **Triggers are NOT scene-query shapes.** Raycasts / `sphere_is_overlapped` /
  `sweep_*` do NOT hit trigger volumes — they only emit `on_trigger` events.
- **Dispatch target.** `on_trigger` is called on the trigger owner's *other*
  components, NOT on the trigger `PhysicsBody` itself. Put the handler on a
  sibling component of the trigger entity (unlike Unity's `OnTriggerEnter`,
  which fires on the trigger's own script).
