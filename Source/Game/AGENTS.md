# Game Module

Entity-component system, game logic, player controllers, physics integration, and game mode implementations.

## Layout

- `Components/` — per-aspect components (mesh, lights, camera, physics, ragdoll, particles, decals, billboards, anim, nav, spawner, meshbuilder, prefab-asset, etc).
- `Entities/` — concrete entity types (Player, Bike, CharacterController, anim drivers).
- `GameModes/` — mode states (main menu, etc).
- `TP/`, `CarGame/`, `TopDownShooter/` — per-game implementations with their own HUD/player code.

Glob the dirs for the current file list; do not maintain it here.

## Concepts

- **Entity / Component** — `Entity` is the world object with a transform hierarchy and a list of `Component`s. Both inherit `BaseUpdater` (virtual `update()` driven off the level's tick list). Components are scriptable from Lua.
- **Component lifecycle** — `start()` fires once after the component is fully constructed and the entity is in the level. `update()` fires each tick if ticking is enabled. `stop()` fires on destruction. `on_sync_render_data()` runs each frame before rendering — push matrices/state to `RenderScenePublic` here, not in `update()`.
- **Transform hierarchy** — world transforms are lazily computed and cached. `set_ls_*` dirties the subtree; world transform recomputes on the next `get_ws_*`. Always go through these accessors so caching stays correct.
- **Bone parenting** — set `bone_parent` (an `EntityBoneParentString`) to attach an entity to a specific bone on its parent's skeleton; resolved in `on_changed_transform`.
- **Tag queries** — `EntityTagString` on entities supports fast level-wide lookups; prefer tags over walking the hierarchy.
- **Prefabs** — `.tprefab` files loaded via `PrefabFile`; `PrefabAssetComponent` spawns one as runtime children. `PrefabAssetMetadata` registers them with the asset browser.
- **Serialization** — all reflected `Entity`/`Component` properties round-trip through the Framework `Serializer`; reflection drives both level files and editor UI.
- **Physics** — `PhysicsComponents` registers rigidbodies/colliders/constraints with the physics engine. `RagdollComponent` blends physics-driven poses with animation via `AnimatorObject`.

## Gotchas

- Don't push render state from `update()` — use `on_sync_render_data()`, otherwise you get a one-frame lag or race with the render thread.
- After mutating local transform, do not read `get_ws_*` expecting the old value; it recomputes.
- `destroy()` is deferred (queued); the object is still alive for the rest of the tick.
