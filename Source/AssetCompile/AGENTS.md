# AssetCompile Module

Offline asset import/compilation: GLTF/GLB model import, skeleton extraction, animation baking, GLTF export. Pipeline reads GLTF via cgltf, applies `ModelImportSettings`, writes engine-format assets.

## Concepts

- **LOD generation** — LODs imported from GLTF or generated; screen-space size thresholds drive selection at runtime.
- **Bone pruning** — `bone_keep_list` strips bones not needed at runtime to cut skinning cost. Skeletons can be shared across models (one model references another's skeleton asset).
- **Additive animation** — A clip is converted to additive by subtracting a reference pose at compile time (not runtime).
- **Root motion baking** — `remove_linear_velocity` strips locomotion from the root bone and stores it as a separate track; runtime re-applies it. Distinct from `has_root_motion` which just enables extraction.
- **Mirror / mask** — `SkeletonMirror` is a list of left/right bone-name pairs; `SkeletonMask` is a per-bone float weight array for partial-body blending. Both authored in `ModelImportSettings` (`mirror_map`) and consumed by the anim system.
- **Retargeting** — `retarget_map` rebinds clips to a different skeleton at compile time.
- **Lightmap UVs** — Generated or packed during import, not at runtime.
- **GLTF export** — `write_gltf.cpp` round-trips engine meshes back to GLTF for tooling.

## Gotchas

- Import settings are reflection-driven (ClassBase) — adding fields requires regenerating MEGA.cpp.
- Frame cropping (`crop_start`/`crop_end`) and `fps` override happen before additive conversion and root-motion extraction; order matters if you add new passes.
- Additive reference pose comes from another clip in the same asset — circular references are not checked.
