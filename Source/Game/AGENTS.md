# Game Module

Entity-component system, game logic, player controllers, physics integration, and game mode implementations.

## Key Files

### Core ECS
- `Entity.h/cpp` — `Entity`: game object with hierarchical transforms and components
- `EntityComponent.h/cpp` — `Component`: base class for all components
- `EntityPtr.h` — Weak/safe pointer to entities
- `BaseUpdater.h/cpp` — Base class providing `update()` scheduling
- `Game.cpp` — Main game loop integration
- `LevelAssets.h/cpp` — Level asset loading and management

### Components (`Components/`)
- `MeshComponent` — Attaches a `Model` + `Material` to an entity; registers with `RenderScenePublic`
- `LightComponents` — Directional, point, spot light registration
- `CameraComponent` — Active camera; drives view/projection matrices
- `PhysicsComponents` — Rigidbody, colliders (box/capsule/mesh), constraints
- `RagdollComponent` — Physics-driven ragdoll blended with animation
- `ParticleComponent` — Particle effect attachment
- `DecalComponent` — World-space decal registration
- `BillboardComponent` — Camera-facing sprite
- `GameAnimationMgr` — Manages `AnimatorObject` for an entity
- `ParticleMgr` — Particle system manager
- `NavComps` — Navigation mesh components
- `ArrowComponent` — Debug direction arrow
- `SpawnerComponent` — Runtime entity spawning
- `MeshbuilderComponent` — Debug/procedural mesh drawing via `MeshBuilder`
- `GraphCurveComponent` — In-world curve graph visualization

### Entities (`Entities/`)
- `Player` — Player character entity
- `BikeEntity` — Bike/vehicle entity
- `CharacterController` — Movement, jumping, collision response
- `PlayerAnimDriver` — Animation state machine for player
- `BikeSystem` — Bike-specific physics and handling

### Game Modes (`GameModes/`)
- `MainMenuMode` — Main menu state

### Game Implementations
- `TP/TPGame` — Third-person game mode
- `CarGame/CarGame`, `CarGame/CarGui` — Racing/driving mode with HUD
- `TopDownShooter/TopDownShooterGame`, `TopDownPlayer`, `TDChest` — Top-down shooter mode

## Key Classes

### `Entity` (Entity.h)
Root game object. Inherits `BaseUpdater`.
- `create_component<T>(...)` — Allocate and attach a component of type T
- `create_child_entity(...)` — Create a child entity in the hierarchy
- `get_component<T>()` — Find first component of type T
- `get_ls_transform()` / `get_ws_transform()` — Local/world-space transforms
- `set_ls_position(pos)` / `set_ws_position_rotation(pos, rot)` — Transform setters
- `parent_to(entity, bone)` — Parent this entity to another (optionally to a bone)
- `transform_look_at(target)` — Orient toward world-space point
- `destroy()` — Queue entity for removal
- `invalidate_transform()` — Mark world transform dirty for recompute
- Fields: `tag` (`EntityTagString`), `bone_parent` (`EntityBoneParentString`), `children`, `components`

### `Component` (EntityComponent.h)
Base for all entity components. Inherits `BaseUpdater`. Scriptable from Lua.
- Virtual callbacks: `start()`, `update()`, `stop()`, `on_changed_transform()`, `on_sync_render_data()`
- Editor callbacks: `get_editor_outliner_icon()`, `create_editor_ui()`, `editor_compile()`
- `set_ticking(bool)` — Enable/disable `update()` calls
- `sync_render_data()` — Push state to render scene (called each frame before render)
- `destroy()` — Remove from owner entity
- `get_owner()` → `Entity*`

### `BaseUpdater` (BaseUpdater.h)
Provides virtual `update()` and integration with the level's tick list.
- Subclassed by both `Entity` and `Component`

## Key Concepts

- **Component lifecycle** — `start()` fires once after component is fully constructed and the entity is in the level. `stop()` fires on destruction. `update()` fires each tick if ticking is enabled.
- **Transform hierarchy** — World transforms are lazily computed and cached. Calling `set_ls_*` dirtifies the subtree; world transform is recomputed on next `get_ws_*` call.
- **Bone parenting** — Set `bone_parent` to attach an entity to a specific bone of the parent entity's skeleton; resolved in `on_changed_transform`.
- **Tag queries** — `EntityTagString` tags allow efficient queries across the level's entity list.
- **Render sync** — `on_sync_render_data()` is called each frame by the engine before rendering; components push updated matrices/state to `RenderScenePublic` here.
- **Serialization** — All reflected `Component` and `Entity` properties are serialized to/from level files via the Framework `Serializer` system.
- **Physics integration** — `PhysicsComponents` registers with the physics engine; `RagdollComponent` blends physics-driven poses with animation via `AnimatorObject`.
