# AssetCompile Module

Handles offline asset import and compilation: GLTF/GLB model import, skeleton extraction, animation baking, and GLTF export.

## Key Files

- `ModelAsset2.h` — `ModelImportSettings`, `AnimImportSettings` — import configuration types
- `ModelCompilier.cpp`, `ModelCompilierLocal.h` — Main model compilation pipeline
- `SkeletonAsset.h/cpp` — `SkeletonMirror`, `SkeletonMask`, bone mirror/mask structs
- `Compiliers.h` — Compiler interface declarations
- `GltfExport.h`, `write_gltf.cpp` — GLTF/GLB export
- `Someutils.h` — Shared compilation utilities

## Key Classes

### `ModelImportSettings` (ModelAsset2.h)
Reflection-based import configuration for a GLB/GLTF file.
- LOD screen-space size thresholds per LOD level
- Per-submesh material assignment overrides
- Skeleton sharing: share skeleton with another model asset
- `bone_keep_list` — Prune unused bones from the skeleton
- `mirror_map` — Bone mirror pairs for `SkeletonMirror`
- `retarget_map` — Retarget animations to another skeleton
- Lightmap UV generation settings
- Per-clip `AnimImportSettings` list

### `AnimImportSettings` (ModelAsset2.h)
Per-animation-clip import options.
- `clip_name`, `source_file` — Identifies the source clip
- `crop_start`, `crop_end` — Frame range cropping
- `is_additive` — Convert to additive by subtracting a reference clip
- `remove_linear_velocity` — Bake root motion velocity into the clip
- `has_root_motion` — Enable root motion extraction
- `fps` — Override clip frame rate

### `SkeletonMirror` / `SkeletonMask` (SkeletonAsset.h)
- `SkeletonMirror` — Maps left/right bone pairs for mirroring animations
- `SkeletonMask` — Per-bone float weight array for partial-body blending
- `BoneMirror` — Pair of bone names forming a mirror relationship
- `BoneMaskValue` — (bone name, weight) pair

## Key Concepts

- **Compilation pipeline** — `ModelCompilier` reads GLTF via cgltf, extracts meshes/skeletons/animations, applies import settings, and writes engine-format assets
- **LOD generation** — Multiple LODs can be generated or imported from the GLTF; screen-space thresholds set in `ModelImportSettings`
- **Bone pruning** — `bone_keep_list` removes bones not needed at runtime, reducing skinning cost
- **Additive animation** — Clips can be converted to additive by subtracting a reference pose at compile time
- **Root motion baking** — Linear velocity removal strips locomotion from root bone and stores it separately for runtime use
- **GLTF export** — `write_gltf.cpp` allows exporting engine meshes back to GLTF format (e.g., for tooling round-trips)
- **Lightmap UVs** — Can generate or pack lightmap UV channels during import
