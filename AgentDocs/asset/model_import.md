# Model Import

How to import a new GLB model and use it in code.

## Step 1: Create the .mis file

Place a `.mis` file next to the `.glb` in `Data/`. The filename (minus extension) becomes the asset name used in code.

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
- `"meshAsCollision": true` — use the mesh as a physics collider
- `"generate_auto_lods": true` — auto-generate LOD levels
- `"lodScreenSpaceSizes": [0.05, 0.01]` — screen-space thresholds for LOD0/LOD1 cutoffs
- `"myMaterials": ["my_mat.mi"]` — override materials (one entry per submesh, in submesh order). path is relative to Data/.

The asset pipeline compiles `.mis` → `.cmdl` automatically. Reference the asset in code as the `.cmdl` path relative to `Data/`.

## Asset path convention

`Data/` is the asset root (`g_project_base = "Data"` in vars.txt). Paths in `Model::load()` are relative to it:

| File on disk | Model::load() path |
|---|---|
| `Data/props/race_props/finish_line.cmdl` | `"props/race_props/finish_line.cmdl"` |
| `Data/props/road_bike/road_bike.cmdl` | `"props/road_bike/road_bike.cmdl"` |

Note: `Data/` is gitignored — `.mis` files live there alongside the source `.glb` and are not checked in.
