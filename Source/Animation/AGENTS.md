# Animation Module

Skeletal animation runtime: playback, blending, root motion, ragdoll, and curve-driven effects.

## Concepts

- **Direct animation slots** — Named slots for manual one-shot clip playback with start/end callbacks, bypassing the graph.
- **Blackboard variables** — Float/bool/int/vec3 variables drive graph transitions.
- **Root motion** — Bone delta extracted and applied to entity transform. `has_linear_velocity_removed` bakes velocity into the clip.
- **Sync markers** — Clips in a sync group align phase for blended locomotion.
- **Retargeting** — `SkeletonMirror` plus retarget maps let clips play on differently-rigged skeletons; bone mirroring supports symmetry.
- **Ragdoll blending** — Partial ragdoll via per-bone blend weights on `AnimatorObject`.
- **Curve values** — Per-clip float curves sampled per-frame to drive effects (e.g. footstep intensity).
- **Cached pose roots** — Named reusable pose caches registered via `agBuilder`.
- **Pose clips** — Static single-frame poses (no duration); flagged on `AnimationSeq`.

## Invariants

- `MAX_BONES = 256`.
- `Pose` per-bone layout: `quat rotation`, `vec3 position`, `float scale`.
- Global bone matrices are double-buffered (read prior frame while writing current).
