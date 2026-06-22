# Render Module

OpenGL deferred renderer: sort-key batched draw calls, material instancing, GPU culling, shadows, SSAO, volumetric fog, environment probes, decals, GI.

## Concepts

- **Sort-key batching** — every visible object gets a 64-bit `draw_call_key`; one `std::sort` minimises state changes before GPU submission. Built per-pass in `Render_Pass::make_batches`, which groups results into `Mesh_Batch` / `Multidraw_Batch`.
- **Multi-draw indirect** — batched draws emitted via `glMultiDrawElementsIndirect`.
- **Deferred passes** — OPAQUE fills G-buffer, lighting in screen-space, TRANSPARENT sorted back-to-front; DEPTH pass is its own `Render_Pass`.
- **GPU culling** — `GpuCullingTest` does frustum/occlusion on GPU; results feed back into CPU batch lists.
- **Meshlets** — fine-grained culling unit, see `Meshlet.h`.
- **LOD** — per-submesh LODs on `Model`, picked by screen-space size.
- **Material instancing** — shared 64-byte parameter buffer per material; per-instance overrides for up to `MAX_INSTANCE_PARAMETERS` (8) parameters.
- **Env probes / GI** — `EnvProbe` captures local cubemaps for specular IBL; `RenderGiManager` owns probe/lightmap GI.
- **Public API** — `RenderScenePublic` (DrawPublic.h) is the only entry point; register/update/remove returns opaque `handle<>` tokens for objects, lights, decals, skylights, fog, particles, meshbuilder objects.

## Invariants / magic numbers

- `MAX_INSTANCE_PARAMETERS = 8`
- `MATERIAL_SIZE = 64` bytes
- `MAX_MATERIALS = 1024`
- `ModelVertex = 40` bytes (pos, uv, normal, tangent, color/boneW+I)
- `Submesh` material index packs the transparency flag in its high bit.
- Material enums: `BlendState` {OPAQUE, BLEND, ADD, MULT, SCREEN, PREMULT_BLEND}; `LightingMode` {Lit, Unlit, Clearcoat, Iridescence, Sheen, Subsurface, Translucent, Anisotropic, Hair}; `MaterialUsage` {Default, Postprocess, Terrain, Decal, UI, Particle}.

### `draw_call_key` 64-bit layout

| Field    | Bits | Purpose                       |
|----------|------|-------------------------------|
| distance | 14   | Coarse depth sort             |
| mesh     | 14   | Mesh asset ID                 |
| vao      | 3    | Vertex array object           |
| texture  | 14   | Primary texture               |
| backface | 1    | Culling mode                  |
| blending | 3    | Blend state                   |
| shader   | 12   | Shader program                |
| layer    | 3    | Render layer                  |

## Gotchas

- `animator_bone_offset` on `Render_Object` indexes a shared bone-matrix buffer — owned by the caller, not the render scene.
- Texture loads accept DDS or stb_image formats via `Texture::load`; mismatched format flags will silently produce wrong sampling.
- `dist_cull_dist` on `Render_Object` is per-instance; 0 means engine default, not "never cull".
- Editor picking relies on `Render_Object::owner` being a valid component pointer; leave null in headless/tooling paths.
