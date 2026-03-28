# Animation Module

Handles skeletal animation runtime: playback, blending, root motion, ragdoll, and curve-driven effects.

## Key Files

- `Runtime/Animation.h/cpp` — Core runtime: `AnimatorObject`, `agBuilder`
- `Runtime/AnimationTreeLocal.cpp/h` — Animation tree evaluation
- `Runtime/RuntimeNodesNew.h/cpp`, `RuntimeNodesNew2.h/cpp` — Graph node implementations
- `Runtime/SyncTime.h`, `Easing.h`, `Percentage.h` — Utilities for sync/easing
- `AnimationTypes.h/cpp` — `AnimationSeq`, `Pose` data structures
- `AnimationSeqAsset.h/cpp` — Asset loading for animation clips
- `AnimationUtil.h/cpp` — Utility functions
- `SkeletonData.h/cpp` — `MSkeleton` skeleton hierarchy and clip storage
- `Event.h` — Animation event definitions

## Key Classes

### `AnimatorObject` (Animation.h)
Core animation runtime. Manages playback, slot-based blending, bone matrices, and root motion.
- `update()` — Advances time, evaluates animation graph, outputs bone matrices
- `play_animation(slot, clip, ...)` — Plays a clip on a named direct slot
- `get_global_bonemats()` — Returns double-buffered world-space bone matrices
- `set_float_variable(name, val)` / `get_curve_value(name)` — Blackboard and curve access
- Supports ragdoll blending, bone masking, additive layers

### `MSkeleton` (SkeletonData.h)
Skeletal hierarchy: bone transforms, inverse bind poses, and clip library.
- `get_bone_index(name)` — Lookup bone by name
- `find_clip(name)` — Retrieve `AnimationSeq` by name
- `get_remap(target)` — Retargeting map to another skeleton
- Supports bone mirroring for animation symmetry

### `AnimationSeq` (AnimationTypes.h)
Single animation clip: compressed keyframe channels, events, curves, sync markers.
- `get_keyframe(bone, time)` — Sample a bone channel at a time
- `get_events_for_keyframe(frame)` — Query animation events
- `is_pose_clip()` — True if clip is a static pose (no duration)
- Fields: `duration`, `fps`, `is_additive`, `has_root_motion`

### `Pose` (AnimationTypes.h)
Per-frame bone pose buffer. MAX_BONES = 256. Each bone: `quat rotation`, `vec3 position`, `float scale`.

### `agBuilder` (Animation.h)
Factory for building animation graph nodes.
- `alloc<T>(...)` — Allocate a typed graph node
- `set_root(node)` — Set the graph output node
- `add_cached_pose_root(name, node)` — Register a reusable pose cache

## Key Concepts

- **Direct animation slots** — Named slots for manual clip playback with start/end callbacks; used for one-shot animations without graph nodes
- **Blackboard variables** — Float, bool, int, vec3 variables driving graph transitions
- **Root motion** — Extracts bone delta movement and applies to entity transform; `has_linear_velocity_removed` bakes in velocity
- **Sync markers** — Synchronize multiple clips in a sync group for blended locomotion
- **Retargeting** — `SkeletonMirror` and retarget maps let animations play on differently-rigged skeletons
- **Ragdoll blending** — `AnimatorObject` supports partial ragdoll with per-bone blend weights
- **Curve values** — Custom float curves on clips queryable per-frame for driving effects (e.g., footstep intensity)
