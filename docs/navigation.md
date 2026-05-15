# Navigation

Recast/Detour-backed navmesh system. Static bake from level geometry, runtime A* queries, single-agent path following.

Source lives in `Source/Navigation/`. Recast/Detour vendored via vcpkg (`recastnavigation:x64-windows`). Headers under `recastnavigation/`.

## Authoring (editor)

1. Place one or more `NavMeshVolumeComponent` entities. The volume's world AABB is the owner entity's world transform applied to a unit cube spanning `[-0.5, +0.5]` — gizmo-scale the entity to size the volume (same convention as `GiVolumeComponent`). In the editor each volume renders as a `cube1m` mesh with a transparent zone material. Only triangles inside any volume are baked.
2. Drop exactly **one** `NavMeshSettingsComponent` somewhere in the level. Every Recast knob (cell size, agent radius/height/slope, region thresholds, etc.) is a REFLECT'd field on this component — edit it via the standard property panel.
3. Set `nav_static = true` on the `MeshComponent`s that should contribute geometry. Default is true for new meshes.
4. Run `bake_nav` in the console. The baker collects geometry, runs Recast → Detour, hands the resulting `dtNavMesh` to `RuntimeNavManager::inst`.
5. Run `save_baked_nav` (or rely on `auto_save_on_bake`). Writes `<map>.navmesh` next to the `.tmap`.

Console commands are registered in `EngineMain_Commands.cpp` next to `bake_probes` / `save_baked_gi`.

If the level has no `NavMeshSettingsComponent` or no `NavMeshVolumeComponent`, bake fails with a clear `Warning` — no silent defaults.

## Runtime

`Level::start()` calls `LevelNavUtil::on_scene_load_nav(source_name)` immediately after the GI sidecar load. If `<map>.navmesh` is absent, the manager simply holds no mesh and `find_path` returns false.

`RuntimeNavManager::inst` is the only public surface. All callers go through it; Detour types (`dtNavMesh`, `dtNavMeshQuery`) stay behind the interface so a v2 swap to `dtTileCache` for dynamic obstacles is additive.

```cpp
std::vector<glm::vec3> corners;
if (RuntimeNavManager::inst->find_path(start, end, corners)) { ... }
```

`find_path` accepts an optional `NavQueryFilter*` (reserved for v2 cost overrides; pass nullptr in v1). `find_path_async` exists as a sync-wrapped stub for the same forward-compat reason.

## Agent component

`NavAgentComponent` ticks every frame. Call `request_path_to(dest)` to plan; the component walks the corner string at `move_speed`, snaps to each corner within `arrive_radius`, and reports completion via `has_arrived()`. No replanning on environment changes — caller re-requests as needed.

REFLECT'd properties: `move_speed`, `arrive_radius`, `debug_draw_path` (per-agent overlay override).

## Sidecar format

`<map>.navmesh` (`FileSys::open_read_game`). v1 layout:

```
int32 magic   = 'NMSH' (0x4E4D5348)
int32 version = 1
int32 tile_count
repeat tile_count:
  int32 tile_byte_size
  bytes tile_data      (raw dtCreateNavMeshData output, owned by Detour after load)
```

Loaded via `dtNavMesh::addTile(..., DT_TILE_FREE_DATA, ...)` so Detour owns the memory.

v2 will append a `dtTileCacheLayer` section after the tiles to support dynamic obstacles. The version header gates the read; v1 readers stop at the tile array and ignore trailing bytes.

## Debug draw

All `nav.debug.*` cvars default off. Cvars live in `NavMeshDebugDraw.cpp`; the tick runs from `Level::update_level` so overlays are one-frame and cost nothing when off (except `nav.debug.agents` per-agent path scan, which is required for the per-agent override to fire).

| Cvar                    | Effect                                                                  |
| ----------------------- | ----------------------------------------------------------------------- |
| `nav.debug.navmesh`     | Translucent green filled polys (Particle_Object + `navmesh_fill.mm`) plus green edge wireframe (Debug::add_line) |
| `nav.debug.tile_grid`   | Tile-bound wireframe boxes (orange)                                     |
| `nav.debug.agents`      | Blue corner-string polyline + yellow arrive-radius sphere per agent     |
| `nav.debug.volumes`     | Cyan wireframe AABB for every `NavMeshVolumeComponent` in the level     |

Per-agent override: `NavAgentComponent::debug_draw_path = true` shows just that agent's path even with the global cvar off.

## Source-comment anchors

These exist so source-code `@docs` refs into navigation validate. Content for each lives in the sections above.

### runtime-nav-manager
See [Runtime](#runtime). `RuntimeNavManager::inst` owns the loaded `dtNavMesh` + a `dtNavMeshQuery`. Detour types stay behind the singleton so v2 can swap to `dtTileCache` without touching call sites.

### level-nav-util
See [Authoring (editor)](#authoring-editor) + [Runtime](#runtime). Static façade for sidecar load/save + editor bake entry points. Mirror of `GameSceneGiUtil`.

### volume-component
See [Authoring (editor)](#authoring-editor). `NavMeshVolumeComponent` is a pure region tag — only meshes whose world AABB intersects at least one volume are baked. Editor-only authoring.

### settings-component
See [Authoring (editor)](#authoring-editor). `NavMeshSettingsComponent` is the single source of truth for bake parameters. Baker errors if zero or more-than-one exist in the level.

### agent-component
See [Agent component](#agent-component). Ticks once per frame; walks corner string; no replanning.

### baker
See [Authoring (editor)](#authoring-editor). Editor-only TU (`#ifdef EDITOR_BUILD`). Collects nav_static meshes intersecting volumes, runs Recast → Detour, hands the result to `RuntimeNavManager::inst`.

### debug-draw
See [Debug draw](#debug-draw). Cvar-driven one-frame overlays via `Debug::add_*`.

## Files

| Purpose                  | File                                                  |
| ------------------------ | ----------------------------------------------------- |
| Runtime singleton        | `Source/Navigation/RuntimeNavManager.{h,cpp}`         |
| Scene load/save/bake     | `Source/Navigation/LevelNavUtil.{h,cpp}`              |
| Volume tag               | `Source/Navigation/NavMeshVolumeComponent.{h,cpp}`    |
| Bake parameters          | `Source/Navigation/NavMeshSettingsComponent.{h,cpp}`  |
| Agent follower           | `Source/Navigation/NavAgentComponent.{h,cpp}`         |
| Editor bake pipeline     | `Source/Navigation/NavMeshBaker.{h,cpp}` (EDITOR_BUILD only) |
| Per-frame overlays       | `Source/Navigation/NavMeshDebugDraw.{h,cpp}`          |
| `nav_static` flag        | `Source/Game/Components/MeshComponent.h`              |
| Sidecar load hook        | `Source/Level.cpp` (`Level::start`)                   |
| Console commands         | `Source/EngineMain_Commands.cpp`                      |
| Singleton init           | `Source/EngineMain_Init.cpp`                          |

## Out of scope (v1)

- Off-mesh / nav links
- Dynamic obstacles (`dtTileCache`)
- Crowd avoidance (`dtCrowd`)
- Multiple agent profiles (each profile would need its own bake)
- Async path queries on a worker pool — `find_path_async` is a sync stub

## Tests

- `Source/IntegrationTests/Tests/Navigation/test_nav.cpp`
  - `nav/no_mesh_query` — sanity: queries fail cleanly with no mesh loaded.
  - `nav/runtime_path` — Recast-builds a flat plane, spawns a `NavAgentComponent`, ticks until it reaches the far corner.

The integration test fixture re-implements the Recast pipeline inline so it doesn't depend on a baked level asset. The triangles are CCW-wound viewed from +Y so `rcMarkWalkableTriangles` keeps them — flip the winding if reusing this fixture for a -Y-up case.

## Sign / coord conventions

Same as the rest of the engine: +Y up, +Z forward, +X right. Recast itself uses +Y up internally, so no axis swap is needed at the Recast boundary.
