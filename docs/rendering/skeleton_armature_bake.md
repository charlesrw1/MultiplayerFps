# Skeleton Armature Root Bake

glTF files from Blender/UE often have a parent node above the armature with scale and/or rotation (e.g. cm-to-m conversion). The model compiler detects this and bakes it at compile time so the runtime skeleton, animations, and skinning all operate in meters.

## What `armature_root` is

`armature_root = get_node_transform(root_joint->parent)`. It captures the immediate parent of the first skin joint. For a correctly-exported file this is identity (no-op).

## What the bake does

Three operations, all in `ModelCompile_Skeleton.cpp::apply_armature_root_to_skeleton`:

1. **Normalize bind pose 3x3** — glTF exporters bake the armature parent's inverse into `invBindMatrices`, embedding a scale factor in the 3x3. The runtime builds `globalBoneMat` from `mat4_cast(quaternion)` which is always scale 1. Normalizing the posematrix columns strips the embedded scale so `palette = globalBone * invBind` has no scale blowup at bind pose.

2. **Bake local transforms** — root bone: full `armature_root * position`, rotation pre-multiplied by `arm_quat`. Child bones: position `*= arm_scale` (uniform). This makes the skeleton hierarchy evaluate in meters.

3. **Bake animation keyframes** — same root/child split applied to position and rotation keyframes in the self-contained animation set.

Vertex positions are left untouched — the glTF spec requires exporters to pre-scale skinned mesh vertices into bind-pose space, so they arrive already in the target unit. Every major exporter (Blender, UE, Maya) follows this. If a non-conforming exporter does not pre-scale vertices, the skinned mesh will be in the wrong unit — apply `armature_root` to vertex positions in that case.

Non-skinned meshes (static props) are unaffected; `globaltransform` from the node hierarchy handles their parent transforms already.

## Constraints

- Only uniform scale is supported. Non-uniform armature root scale emits a compile error and skips the bake.
- Only the immediate parent node is captured. Stacked scale nodes (e.g. two 0.01 parents) produce wrong results — fix the export instead.
- The `apply_armature_transform` field on `ModelDefData` (default `true`) can disable the bake per-model.
- Retarget path in `ModelCompile_Animation.cpp` assumes both skeletons are baked. Will break if a source skeleton was not baked. Needs retarget rewrite (TODO).

## Skinning equation

```
palette[i] = globalBoneMat[i] * invBindPose[i]
GPU:  finalPos = palette * vertex
```

After bake, all three terms have scale 1 and consistent translations. `palette` is pure rotation + translation. The skinned result matches the unskinned `MeshComponent` path.

## Debug

The compiler writes `<model>_skel_dump.txt` alongside the `.cmdl` with bone transforms, vertex samples, and globaltransform for diagnosing mismatches.
