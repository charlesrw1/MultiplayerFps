# Render Module

OpenGL-based deferred renderer: batched draw calls, material instancing, GPU culling, shadows, SSAO, volumetric fog, environment probes, and decals.

## Key Files

- `DrawPublic.h` — `RenderScenePublic`: public interface for registering render objects
- `RenderScene.h/cpp` — `Render_Pass`, `draw_call_key`, `Pass_Object`: pass management and batching
- `RenderObj.h` — `Render_Object`: per-renderable state
- `Model.h/cpp` — `Model`, `Submesh`, `ModelVertex`: mesh asset
- `Shader.h/cpp` — `Shader`: GLSL compilation and uniform binding
- `Texture.h/cpp` — `Texture`: texture asset
- `MaterialLocal.h/cpp`, `MaterialPublic.h` — Material system, blend states, parameter values
- `DrawLocal.h/cpp` — Internal draw call submission
- `Render_Light.h`, `Render_Sun.h` — Light types
- `Render_Decal.h` — Decal rendering
- `Render_Volumes.h` — Volume effects
- `EnvProbe.h/cpp` — Environment probe capture and apply
- `Frustum.h/cpp` — Frustum culling
- `GpuAllocator.h/cpp` — GPU buffer allocation
- `GpuCullingTest.h/cpp` — GPU-driven culling
- `Meshlet.h/cpp` — Meshlet-based rendering
- `Ssao.cpp`, `Shadowmap.cpp`, `Volumetricfog.cpp` — Screen-space effects
- `RenderWindow.h/cpp` — Window and context management
- `OpenGlDevice.cpp` — OpenGL device implementation
- `IGraphsDevice.h` — Graphics device interface
- `RGiManager.h/cpp` — Global illumination management
- `RenderExtra.h/cpp` — Extra render utilities

## Key Classes

### `RenderScenePublic` (DrawPublic.h)
Public interface for registering renderables. Returns `handle<>` tokens.
- `register_obj(Render_Object&)` → `handle<Render_Object>` — Add a mesh renderable
- `update_obj(handle, Render_Object&)` — Update transform/state
- `remove_obj(handle)` — Unregister
- Same pattern for lights, decals, skylights, fog, particles, meshbuilder objects

### `Render_Pass` (RenderScene.h)
Manages one render pass (OPAQUE, TRANSPARENT, DEPTH).
- `add_object(Pass_Object)` — Submit a renderable to this pass
- `make_batches()` — Sort by `draw_call_key`, group into `Mesh_Batch` / `Multidraw_Batch`
- `create_sort_key_from_obj(obj)` — Build 64-bit sort key from object state

### `draw_call_key` (RenderScene.h)
Packed 64-bit sort key for a single draw call:
| Field | Bits | Purpose |
|-------|------|---------|
| distance | 14 | Coarse depth sort |
| mesh | 14 | Mesh asset ID |
| vao | 3 | Vertex array object |
| texture | 14 | Primary texture |
| backface | 1 | Culling mode |
| blending | 3 | Blend state |
| shader | 12 | Shader program |
| layer | 3 | Render layer |

### `Render_Object` (RenderObj.h)
Per-mesh-instance state submitted to `RenderScenePublic`.
- `model` — Asset pointer
- `animator_bone_offset` — Offset into shared bone matrix buffer
- `material_override` — Per-instance material
- Flags: `shadow_caster`, `viewmodel_layer`, `outline`, `color_overlay`, `dither`, `lightmapped`
- `transform` — World-space matrix
- `lightmap_uv_offset/scale` — Lightmap atlas coordinates
- `dist_cull_dist` — Distance at which to cull
- `owner` — Component pointer for editor picking

### `Model` (Model.h)
Static mesh asset: submeshes, skeleton, bounds, GPU allocations.
- `get_part(index)` → `Submesh&`
- `get_material(index)` → material index
- `bone_for_name(name)` → bone index
- `load_asset(path)` — Load from disk
- `ModelVertex` — 40 bytes: position, UV, normal, tangent, color/bone weights+indices

### `Submesh` (Model.h)
- `base_vertex`, `element_offset`, `element_count`, `vertex_count`
- Material index (transparency flag packed in high bit)

### `Shader` (Shader.h)
GLSL shader program wrapper.
- `compile(vert_src, frag_src, ...)` — Compile and link
- `compute_compile(src)` — Compute shader variant
- `use()` — Bind program
- `set_float/vec3/mat4/...()` — Uniform setters

### `Texture` (Texture.h)
Texture asset with mip/filtering/format options.
- `load(path, ...)` — Load from disk (DDS or stb_image formats)
- `get_size()` → ivec2
- `get_internal_render_handle()` → GLuint

### Material System (MaterialLocal.h)
- `MaterialParameterValue` — Variant: bool, float, vec4, color32, Texture*
- `MatParamType` — Empty, FloatVec, Float, Vector, Bool, Texture2D
- `BlendState` — OPAQUE, BLEND, ADD, MULT, SCREEN, PREMULT_BLEND
- `LightingMode` — Lit, Unlit
- `MaterialUsage` — Default, Postprocess, Terrain, Decal, UI, Particle
- MAX_INSTANCE_PARAMETERS = 8, MATERIAL_SIZE = 64 bytes, MAX_MATERIALS = 1024

## Key Concepts

- **Sort-key batching** — All visible objects get a 64-bit key; single `std::sort` produces optimal state-change minimization before GPU submission
- **Multi-draw indirect** — Batched draws emitted as `glMultiDrawElementsIndirect` commands to minimize CPU-GPU overhead
- **Material instancing** — Shared 64-byte parameter buffer per material; per-instance overrides for up to 8 parameters
- **GPU culling** — `GpuCullingTest` performs frustum/occlusion culling on the GPU; results feed back to CPU batch lists
- **LOD system** — `Model` stores per-submesh LODs; renderer picks LOD based on screen-space size
- **Meshlets** — `Meshlet.h` supports meshlet-based fine-grained culling
- **Deferred passes** — OPAQUE fills G-buffer; lighting computed in screen-space passes; TRANSPARENT sorted back-to-front
- **Shadow maps** — `Shadowmap.cpp` cascaded shadow maps for `Render_Sun`
- **SSAO** — `Ssao.cpp` screen-space ambient occlusion post-process
- **Volumetric fog** — `Volumetricfog.cpp` ray-marched fog volume
- **Environment probes** — `EnvProbe` captures local cubemaps for specular IBL
- **GI manager** — `RenderGiManager` handles global illumination probes/lightmaps
