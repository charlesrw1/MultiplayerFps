# Model Importing

Importer accepts `.glb` only; produces `.cmdl` (compiled) + `.mis` (settings sidecar).

## Console import

`IMPORT_MODEL <glb_path>` — path relative to `g_project_base` (default `Data/`). Creates `.cmdl` + `.mis`.

For overrides (materials, pruned skeleton bones, additional anim sources), use the **Model Editor** tool.

## Authoring `.mis` directly

Place `.mis` next to the `.glb` in `Data/`. Filename (sans extension) = asset name.

Minimal template (no skeleton, no collision):

```json
!json
{
"__classname": "ModelImportSettings",
"srcGlbFile": "your_model.glb",
"myMaterials": [],
"lodScreenSpaceSizes": [],
"generate_auto_lods": false,
"meshAsCollision": false
}
```

Common options:

- `"meshAsCollision": true` — mesh becomes physics collider.
- `"generate_auto_lods": true` — auto LOD generation.
- `"prune_disconnected_islands_min_lod": 1` — auto-LOD level (1-4) at which meshopt may drop disconnected mesh islands during simplification; `0` disables island pruning entirely (default `1`, i.e. always allowed).
- `"lodScreenSpaceSizes": [0.05, 0.01]` — screen-space cutoffs for LOD0/LOD1.
- `"myMaterials": ["my_mat.mi"]` — per-submesh override (in submesh order). Path relative to `Data/`.

Pipeline compiles `.mis` → `.cmdl` automatically. Reference assets by `.cmdl` path relative to `Data/`.

## Asset path convention

`Data/` is the asset root (`g_project_base = "Data"`, set in the per-project `.ini` under `Projects/`). `Model::load()` paths are relative:

| File on disk | `Model::load()` path |
|---|---|
| `Data/props/race_props/finish_line.cmdl` | `"props/race_props/finish_line.cmdl"` |
| `Data/props/road_bike/road_bike.cmdl` | `"props/road_bike/road_bike.cmdl"` |

`Data/` is gitignored — `.mis` lives there alongside `.glb`, not committed.

## Model Physics

In the DCC tool, prefix meshes with `CVX_` to make them convex collision meshes attached to the asset. In the map editor, expose them by adding a `MeshColliderComponent` to an object that already has a `MeshComponent`.

## Model Animations

(TODO — animation pipeline notes pending.)
