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
- `"myMaterials": ["my_mat.mi"]` — override materials (one entry per submesh, in submesh order)

The asset pipeline compiles `.mis` → `.cmdl` automatically. Reference the asset in code as the `.cmdl` path relative to `Data/`.

## Step 2: Spawn a MeshComponent at runtime

Include the headers:
```cpp
#include "Game/Components/MeshComponent.h"
#include "Render/Model.h"
```

Spawn a static prop at a world position:
```cpp
Entity* e = GameplayStatic::spawn_entity();
e->set_ws_position(pos);
e->create_component<MeshComponent>()->set_model(
    Model::load("path/to/your_model.cmdl"));
```

To orient the entity (yaw to match a forward direction vector):
```cpp
const float yaw = std::atan2(forward.x, forward.z);
e->set_ws_position_rotation(pos, glm::angleAxis(yaw, glm::vec3(0.f, 1.f, 0.f)));
```

To parent it to another entity (e.g. attach to a bike):
```cpp
e->parent_to(parent_entity);
e->set_ls_position_rotation(local_pos, local_rot);
```

## Asset path convention

`Data/` is the asset root (`g_project_base = "Data"` in vars.txt). Paths in `Model::load()` are relative to it:

| File on disk | Model::load() path |
|---|---|
| `Data/props/race_props/finish_line.cmdl` | `"props/race_props/finish_line.cmdl"` |
| `Data/props/road_bike/road_bike.cmdl` | `"props/road_bike/road_bike.cmdl"` |

Note: `Data/` is gitignored — `.mis` files live there alongside the source `.glb` and are not checked in.
