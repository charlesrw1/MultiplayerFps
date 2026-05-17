# Graphics Device Abstraction

This is the canonical migration doc. The earlier strategy doc at
`~/.claude/plans/i-want-to-put-idempotent-fairy.md` is historical — Phase 2c
decisions there are superseded by **Pipeline model** below.

Phase 3 implementation plan (detailed): `~/.claude/plans/review-gfx-abstraction-md-and-sdl3gpu-wh-cuddly-bunny.md`.

Per-sub-phase status entries for Phase 1 and Phase 2 (as-landed implementation
notes — split out to keep this file readable): [[gfx_abstraction_changelog]].

sdl3 gpu api (for reference): https://github.com/libsdl-org/SDL/blob/main/include/SDL3/SDL_gpu.h

Migration of all rendering behind `IGraphicsDevice` (Source/Render/IGraphicsDevice.h) so an SDL3 GPU backend can sit alongside the current OpenGL backend.

Global accessor: `gfx()` (free function). The OpenGL singleton is built by `gfx_init_opengl()` during `Renderer::init` and torn down by `gfx_shutdown()`.

## Phase plan

1. **Wrap.** Every `gl*` call moves behind `IGraphicsDevice`. API stays GL-shaped — by-name uniforms, apply-at-draw pipeline state, immediate draws. Goal: zero `gl*` calls outside `Source/Render/Opengl*`. No behavior changes.
2. **Redesign.** With everything funneled through the interface, reshape the API on the OpenGL backend only — synthetic UBOs, combined texture+sampler binding, pipeline-state consolidation, command encoder, frame lifecycle, transfer ring buffers.
3. **Port.** Add the SDL3 GPU backend against the redesigned interface. GLSL → SPIR-V via glslang/shaderc at runtime (dev) or prebaked blobs (release).

The Phase 1 boundary is gated by a **static `gl*` scanner** (`Scripts/check_no_gl_leaks.py` + the `LegacyGlCalls.NoneOutsideBackend` unit test) landed in sub-phase 1.9. The originally-planned null/passthrough runtime backend was superseded — see the 1.5 row below.

## Pipeline model

Locked-in decisions that shape every later phase.

**Struct-in, no pipeline objects exposed.** `gfx().set_pipeline(const RenderPipelineState&)`
stays as the entry point. **No** `IGraphicsRasterPipeline*` / `IGraphicsComputePipeline*`
interface types — pipeline handles never cross the abstraction boundary.

- OpenGL backend: keeps the existing invalid-bits state cache in
  `OpenglRenderDevice` (DrawLocal_Device.h:148+). `set_pipeline` diffs against
  cached state, emits `glUseProgram` / `glBindVertexArray` / etc. only on change.
- SDL3 backend: hashes the struct contents, looks up an internal
  `unordered_map<hash, SDL_GPUGraphicsPipeline*>`, creates + caches on miss,
  binds the cached pipeline. The map is private to the backend translation unit.
- Hot-reload: `Program_Manager::recompile_all` invalidates SDL3 cache entries
  referencing the recompiled program. OpenGL backend cache is unaffected.

**Statemachine setters fold onto `RenderPipelineState` in Phase 2c.** SDL3 GPU
treats these as pipeline-create fields, not runtime state. Move them to the
struct so the SDL3 backend can read them at hash time:

| Current immediate setter                          | Becomes field on `RenderPipelineState`                            | Caller                                       |
| ------------------------------------------------- | ----------------------------------------------------------------- | -------------------------------------------- |
| `set_color_write_mask(att, r,g,b,a)`              | `ColorWriteMask color_write_masks[MAX_COLOR_ATTACHMENTS]` (default write-all) | DecalBatcher.cpp:131-136,176 (material-driven variants — ≤16 unique masks) |
| `set_polygon_offset(enabled, factor, units)`      | `polygon_offset_{enabled, factor, units}`                         | shadow-bias passes                           |
| `set_line_width(float)`                           | `line_width` (default 1.f; SDL3 silently ignores)                 | debug meshbuilder                            |
| `set_polygon_fill_mode(GraphicsFillMode)`         | `fill_mode` (default `Fill`)                                      | wireframe debug pass                         |

After 2c, the struct is hashable by raw bytes (no padding hazards if laid out
carefully); SDL3 caching key is `XXH3_64bits(&state, sizeof(state))`.

Already on the struct today: blend, depth test/write/func, cull mode, program,
vao. Phase 2c additionally migrates `vao` to `IGraphicsVertexInput*` and
`program` to `IGraphicsShader*` (the latter depends on Sub-phase 1.7).

**Indirect-buffer binds move onto the draw call itself.** `bind_indirect_buffer`
and `bind_parameter_buffer` are deleted in 2c. The buffer pointer becomes a
parameter:

```cpp
multi_draw_elements_indirect(mode, idx_type, IGraphicsBuffer* indirect,
                             int byte_offset, int draw_count, int stride);
multi_draw_elements_indirect_count(mode, idx_type,
                                   IGraphicsBuffer* indirect, int indirect_off,
                                   IGraphicsBuffer* count,    int count_off,
                                   int max_draw_count, int stride);
```

Matches `SDL_DrawGPUIndexedPrimitivesIndirect(cmdbuf, buffer, offset, draw_count)`.

OpenGL backend caching contract: tracks the last-bound `IGraphicsBuffer*` for
`GL_DRAW_INDIRECT_BUFFER` and `GL_PARAMETER_BUFFER` internally (same invalid-bits
style as `OpenglRenderDevice`). The indirect-draw entry points only issue
`glBindBuffer(...)` when the pointer differs — back-to-back MDI on the same
indirect buffer emits zero rebinds. SDL3 backend doesn't need this (buffer goes
straight into the draw call). **Lifetime invariant:** when an `IGraphicsBuffer`
is `release()`d, the backend clears it from cached indirect/parameter slots so
a later draw can't re-bind a dangling handle. Same rule applies to any future
cached buffer slots if they get the same treatment.

**Setters that stay immediate** (genuinely dynamic state on both backends):
`set_scissor` / `disable_scissor`.

## Phase 0 status

- `IGraphsDevice.h` → `IGraphicsDevice.h` (typo fix).
- `IGraphicsDevice::inst` (public static) → `gfx()` free function + `gfx_init_opengl()/gfx_shutdown()/gfx_is_initialized()`.
- Insult comment, `ThingerBobber`, and `FUUUUUUUCK` factory param removed. The OpenGL impl now reaches `draw.get_device().set_depth_write_enabled(true)` directly inside `set_render_pass` to keep `OpenglRenderDevice`'s cached state in sync after `glClear`. Transitional coupling; cleanly removed when `OpenglRenderDevice` folds into the OpenGL backend in Phase 2.
- `bindlesstexhandle` typedef + `supports_bindless` extension flag removed (zero production callers).

## `get_internal_handle()` spike

Used to pull a raw GL handle out of an `IGraphicsTexture` / `IGraphicsBuffer` / `IGraphicsVertexInput` so a direct `gl*` call can consume it. Every such caller is either wrap-routable (a new abstraction method covers it) or needs a redesign-pass extension.

Counts come from `rg "get_internal_handle\(\)"` at start of Phase 0. Verify before editing.

| Site / category                                                                 | Calls | Phase 1 routing                                                                   |
| ------------------------------------------------------------------------------- | ----: | --------------------------------------------------------------------------------- |
| `glBindBufferBase(GL_SHADER_STORAGE_BUFFER, …)` — Lighting, Volumetricfog, DecalBatcher, BatchScene, RenderPass, GpuCullingTest, RT/*, RenderGiManager | ~35   | `gfx().bind_storage_buffer(slot, IGraphicsBuffer*, offset, size)` (Phase 1) |
| `glBindBufferBase(GL_UNIFORM_BUFFER, …)` — GpuCullingTest, RT                  |   ~3  | `gfx().bind_uniform_buffer(slot, IGraphicsBuffer*, offset, size)`                 |
| `glBindBuffer(GL_DRAW_INDIRECT_BUFFER, …)` — BatchScene, DecalBatcher           |   ~2  | `gfx().bind_indirect_buffer(IGraphicsBuffer*)` (or fold into draw call)           |
| `glBindBuffer(GL_PARAMETER_BUFFER, …)` — BatchScene, RenderPass                 |   ~2  | `gfx().bind_parameter_buffer(IGraphicsBuffer*)` (for MDEI w/ count)               |
| `glBindBuffer(GL_SHADER_STORAGE_BUFFER, …)` non-base — RT_Probe, RenderGiManager |  ~3  | `gfx().map_buffer_range / write_buffer` — Phase 2e ring buffer                    |
| `glBindImageTexture(…)` — GpuCullingTest, RenderExtra_SSR, RT_Probe             |   ~5  | `gfx().bind_image_for_compute(slot, IGraphicsTexture*, mip, layer, access)`       |
| `glBindTextureUnit(…)` — Ssao                                                   |   ~4  | `gfx().bind_texture_ptr(slot, IGraphicsTexture*)` (existing on OpenglRenderDevice; lift to gfx()) |
| `glNamedFramebufferTexture(…)` — Ssao, dead-code in SceneDraw/RenderPass        |   ~3  | Already covered by `RenderPassState`; finish migrating Ssao.                      |
| `glNamedBufferData(…)` mid-frame on dynamic buffer — DrawLocal_Scene            |   1   | Phase 1: `IGraphicsBuffer::upload`. Phase 2e: route via ring buffer.              |
| `glGetTextureImage(…)` — SceneDraw_Debug (depth/editor-id readback), Screenshot test, GiScene cubemap save | ~5 | `gfx().download_texture(IGraphicsTexture*, mip, …)` — new API (debug/test path only) |
| `glCopyImageSubData(…)` — TextureUpload (mip copy), RenderGiManager (cube copy) |   2   | `gfx().copy_texture(src, dest, region)` (Phase 1)                                 |
| `glGenerateTextureMipmap(…)` — TextureUpload                                    |   1   | `gfx().generate_mipmaps(IGraphicsTexture*)`                                       |
| `SaveCubeArrayToDDS / save_float_texture_as_dds` — GiScene baking               |   3   | Bake path is offline-ish; route through `gfx().download_texture` + DDS encoder.   |
| `IGraphicsTexture* tex; tex_id(tex)` — OpenGlDevice impl-internal               |   1   | Stays inside backend. Not a leak.                                                 |
| `state.vao = vao_ptr->get_internal_handle()` — RenderPass, BatchScene, EnvProbe, RT_Shade | ~4 | Phase 2c: `RenderPipelineState::vao` becomes `IGraphicsVertexInput*`; backend resolves to GL handle / SDL3 layout at bind. No exposed pipeline object — see [Pipeline model](#pipeline-model). |
| `RenderDump.cpp` integration test — pulls handle for `glReadPixels`            |   1   | Test-side. Move to `gfx().download_texture` once available.                       |
| Commented-out callers                                                           |   ~9  | Delete during wrap.                                                               |

**Risk.** All identified sites are routable through Phase 1 wrap additions. None require a redesign-pass extension. Plan estimate of "17 known" was low — actual count is ~75 raw calls across ~25 files, but they cluster into the ~12 API additions listed above.

The static-scan gate (Phase 1.9) asserts that every `gl*` call lives inside `Source/Render/Opengl*` or sits on an explicit accept-list. `get_internal_handle()` escapes are not yet enforced — a future runtime null/passthrough backend could cover that.

## Phase 1 sub-phases

Total `gl*` site count is ~800 across ~25 files. Phase 1 lands as discrete sub-phases — each builds, tests, and is committed independently so regressions can be bisected to one subsystem.

| Sub-phase | Scope | gl-call count | API additions | Status |
| --------- | ----- | -------------:| ------------- | ------ |
| **1.1** | `DrawLocal_Misc.cpp`, `DrawLocal_Debug.cpp` | ~14 | scissor, draw_elements_base_vertex, bind_uniform_buffer_base_raw, wait_for_gpu_idle, download_texture; move `CheckGlErrorInternal_` to backend | done |
| **1.2a** | `Ssao.cpp` | ~100 | draw_arrays, bind_texture, bind_uniform_buffer_base; migrate raw FBOs to `gfx().set_render_pass`; migrate raw textures + UBO to `IGraphicsTexture` / `IGraphicsBuffer`; fix `rgba16_snorm` input type in backend | done |
| 1.2b | `EnvProbe.cpp` | ~106 | cubemap mip clamping, temp render target, `glReadPixels`, BRDFIntegration (defer shader-coupled lines to 1.7) | done |
| **1.3a** | `Volumetricfog.cpp` | ~33 | dispatch_compute, memory_barrier (flag enum), bind_image_for_compute, bind_storage_buffer_base (+ `_raw`); t3D path in backend; add WRAP_R to LinearClamped sampler | done |
| **1.3b** | `RenderExtra_SSR.cpp`, `GpuCullingTest.cpp` | ~52 | `IGraphicsSampler` + bind_sampler, glClearNamedBufferData wrapper | done |
| 1.3c | `RT/RaytraceTest_Probe.cpp`, `RT/RaytraceTest_Shade.cpp` | ~45 | buffer map/unmap for readback (probe relocation, invalid-count) | not started |
| **1.4a** | `DrawLocal_Lighting.cpp` | ~10 | set_line_width, bind_indirect_buffer (nullptr unbinds) | done |
| **1.4b** | `DrawLocal_BatchScene.cpp` | ~10 | bind_parameter_buffer, multi_draw_elements_indirect_count, set_polygon_offset | done |
| **1.4c** | `DrawLocal_RenderPass.cpp` | ~50 | bind_storage_buffer_range_raw, bind_indirect_buffer_raw, multi_draw_elements_indirect, draw_elements, draw_elements_instanced_base_vertex_base_instance, set_clear_color, upload_buffer_raw, sub_upload_buffer_raw | done |
| **1.4d** | API drift correction: per-attachment clear color on `ColorTargetInfo` (mirrors `SDL_GPUColorTargetInfo`); deletes `set_clear_color` + `RenderPassState::{wants_color_clear, use_gray_clear, set_clear_both}` | — | `ColorTargetInfo::{wants_clear, clear_color}` | done |
| **1.4e** | API drift correction: migrate remaining raw `bufferhandle` fields (`ubo.current_frame`, `ShadowMapManager::frame_view`, `CascadeShadowMapSystem::ubo.{info,frame_view[4]}`, `RenderScene::gpu_skinned_mats_buffer`, `Render_Lists::{gpu_command_list, gldrawid_to_submesh_material, glinstance_to_instance}`, `GpuCullingTest::mdi_buf`, `Render_Level_Params::provied_constant_buffer`) → `IGraphicsBuffer*`; delete entire `_raw` API surface; add `bind_storage_buffer_range(IGraphicsBuffer*, …)` | — | `bind_storage_buffer_range`; delete `bind_{uniform,storage}_buffer_base_raw`, `bind_storage_buffer_range_raw`, `bind_indirect_buffer_raw`, `{upload,sub_upload}_buffer_raw` | done |
| **1.4f** | API drift correction: move texture-scoped ops onto `IGraphicsTexture` | — | `IGraphicsTexture::{set_mip_range, download}`; delete `IGraphicsDevice::{set_mip_range, download_texture, download_texture_2d}` | done |
| **1.4g** | API drift correction: implicit render/compute pass separation (SDL3 GPU prep) | — | `IGraphicsDevice::begin_compute_pass()`; backend tracks `PassMode`; `set_render_pass()` flips to Render; assert in `dispatch_compute()` | done |
| **1.5a** | `DecalBatcher.cpp` | ~12 | per-attachment color masks as immediate setters (baked in 2c) | done |
| **1.6** | `DrawLocal_SceneDrawInternal.cpp` (orchestration) | ~14 | set_polygon_fill_mode; migrate `render_bloom_chain` to `IGraphicsTexture*` | done |
| **1.7a** | `IGraphicsShader` interface + factory scaffolding | — | `IGraphicsShader`; `create_shader_{vert_frag,vert_frag_geo,compute,single_file,single_file_tess}`; `OpenGlShaderImpl.cpp`; route compute + shared-tess paths in `Program_Manager::recompile_do` through factory | done |
| **1.7b** | Move `Shader.cpp` compile/link bodies into `OpenGlShaderImpl.cpp` | ~50 | source-loader + `glCreateShader/Compile/Attach/Link/Delete` move into backend; `Shader::compile*` stays in `Shader.h` but bodies live in backend TU | done |
| **1.7c** | Uniform setters onto `IGraphicsShader` | ~12 | `IGraphicsShader::{use,set_int,set_float,set_mat4,…,set_block_binding}` (DSA `glProgramUniform*`); `Shader::set_*` forward through `gfx_handle`; binary-cache paths populate `gfx_shader` via `opengl_wrap_program_handle` | done |
| **1.7d** | Program-binary cache + delete `Shader` struct | ~20 | move `glProgramBinary`/`glGetProgramBinary` cache paths in `recompile_shared`/`recompile_normal` into backend; delete `Shader` struct; `RenderPipelineState::program` becomes `IGraphicsShader*` (lifted from Phase 2c) | done |
| **1.8** | Window/swapchain + ImGui wrap | ~10 (`SDL_GL_*`, `gladLoad*`, swap, vsync, imgui_render) | `gfx_opengl_pre_window_setup`; `gfx_init_opengl(SDL_Window*)`; `set_vsync`, `present`, `imgui_init`, `imgui_shutdown`, `imgui_new_frame`, `imgui_render_draw_data`, `imgui_process_event` | done |
| **1.9** | static-scan no-legacy-`gl*` gate + migrate every remaining non-backend caller (MeshBuilder, GiScene, DdsExport, Screenshot, RenderDump, RenderExtra, LightCookieAtlas, SceneDraw, Scene, Editor, EnvProbe::BRDFIntegration, Meshlet, TextureUpload, RenderGiManager, DrawLocal_Init) + delete dead `BufferTest/` + delete dead `Meshlet.cpp` body + accept-list Profilier/GpuTimer (OpenGL-only — SDL3 GPU has no timestamp/debug-group API) and `DrawLocal_Device.*` (folded into backend in Phase 2) | — | `copy_texture`, `IGraphicsTexture::generate_mipmaps`, `gfx_opengl_dump_capabilities`, `gfx_opengl_enable_debug_output`; rename `glCheckError` → `gfx_check_gl_error` | done |
| ~~1.5 (post-1.x)~~ | ~~Null/passthrough leak-detector backend~~ | — | ~~gate that proves the wrap is complete~~ | **superseded** by the 1.9 static-scan gate (`Scripts/check_no_gl_leaks.py` + `LegacyGlCalls.NoneOutsideBackend`). A runtime null backend would still be useful for catching `get_internal_handle()` escapes inside future non-OpenGL backends, but is no longer load-bearing for Phase 1 completion. |

Migration rule per sub-phase: any GL call still needed by a non-backend caller becomes an `IGraphicsDevice` method (with a `_raw` suffix when the parameter is still a `bufferhandle`/`vertexarrayhandle`; those raw escape hatches disappear in Phase 2 once the corresponding resources route through `IGraphicsBuffer*` / `IGraphicsVertexInput*`).

## Phase 2 — Redesign (planned)

With every call funneled through the interface, the API itself reshapes —
each redesign is one API change + one backend change, not 45 call-site edits.
Lands after the Phase 1.9 static-scan gate is clean (it is).

### 2a — Per-pass UBO migration (manual)

**Strategy change from the original plan.** The earlier proposal was a
load-time GLSL preprocessor that rewrote every plain `uniform` into a
synthetic `_AutoUniforms` block at compile, keeping `set_float(name, ...)`
working. That approach was _rejected_: it keeps a name-based setter layer
forever, requires a permanent GLSL parser-rewriter in the load path, and
the final SDL3 code looks identical to manual migration anyway (CPU
struct → UBO upload → bind). We migrate each shader explicitly instead.

**Why this works for SDL3:** SPIR-V (Vulkan / DX12 via DXC) rejects
top-level scalar/vector uniforms. The two replacements are UBOs (any size,
slot-bound) and push constants (~128B, slot-bound, inline in cmd buffer).
For OpenGL we model both with UBOs; the SDL3 backend port can promote the
per-pass slot to a real push-constant block if that's cheaper.

**Pattern (locked by the BloomDownsample / BloomUpsample pilot):**

Single source of truth for every per-pass struct lives in
`Shaders/ShaderBufferShared.txt` — same convention as the existing
`Shaders/SharedGpuTypes.txt` (`#ifdef __cplusplus` wrapper that pulls in
`using namespace glm`, `typedef uint32_t uint`, and `namespace gpu { … }`).
The file is `#include`'d directly from C++ headers AND from GLSL via the
shader-source include preprocessor — one struct declaration is canonical
for both languages.

In `Shaders/ShaderBufferShared.txt` (shared, single source of truth):
```glsl
struct BloomParams {
    vec2  srcResolution;
    int   mipLevel;
    float filterRadius;
};
```

In each shader (e.g. `BloomDownsampleF.txt`):
```glsl
#include "ShaderBufferShared.txt"
layout(binding = 0) uniform sampler2D srcTexture;       // textures get explicit binding too
layout(binding = 7, std140) uniform BloomParamsUbo { BloomParams params; };
// usage: params.srcResolution, params.mipLevel
```

In C++ (`DrawLocal_RenderPass.cpp`):
```cpp
gpu::BloomParams p{};
p.srcResolution = glm::vec2(src_x, src_y);
p.mipLevel = i;
ubo.bloom_params->upload(&p, sizeof(p));
gfx().bind_uniform_buffer_base(BLOOM_PARAMS_UBO_BINDING, ubo.bloom_params);
```

**Why nested-struct (`BloomParamsUbo { BloomParams params; }`) over inline
fields:** matches the established `Ubo_View_Constant_Buffer { Ubo_View_Constants_Struct g; }`
pattern; one struct definition shared between GLSL and C++ via
`#include`; renames update everywhere from a single edit; std140 padding
correctness is enforced in one place. The cost — an extra `params.`
prefix at every access site — is the same indirection the View UBO and
Cull UBO already use, so it stays consistent across the codebase.

**Block-layout keyword:** `std140` for UBO blocks, `std430` for SSBO
blocks. `std430` on a `uniform` block is non-portable (NVIDIA accepts it,
AMD/Intel reject; SPIR-V requires `VK_KHR_uniform_buffer_standard_layout`).
For the param structs in scope (scalars / vec2 / vec3-with-padding / vec4
/ mat4 — no arrays of scalars or vec3), std140 and std430 produce
identical layouts, so there's no payload reason to deviate.

**UBO binding-slot convention** (active shaders surveyed before pilot):
- `0` — `Ubo_View_Constant_Buffer` (shared view/frame constants, everywhere)
- `4` — Vfog system
- `5` — Cull system
- `7` — **per-pass params slot** (the push-constant analog). Conflicts
  with SSBO/sampler bindings at the same number are fine — separate slot
  pools in OpenGL.
- `8` — DDGI globals

**Endgame:** when every `IGraphicsShader::set_*(name, ...)` call-site is
gone, delete the name-based setter methods from `IGraphicsShader` and
`OpenGLShaderImpl`. That's the SDL3-ready milestone.

### 2a UBO group plan

Original per-shader UBO design (one struct per shader) was rejected as too
fragmented — most candidate shaders run **once per frame** and share related
field sets. Consolidate into per-group UBOs instead. **Group sharing rule:**

- Shader runs **once per frame** → put it in a group; the group's single
  UBO is uploaded once near the start of the pipeline stage and read by
  every shader in the group.
- Shader runs **N times per frame in a loop** (per-mip, per-light) →
  *don't* share with unrelated shaders; iterate uploads on a dedicated
  buffer (the bloom-pilot pattern). Inside a shader-group, if all
  iterating shaders run sequentially in the same loop body, they can
  share — re-upload between iterations.
- Shader runs **per-draw** (material/meshbuilder/UI) → never share; each
  draw owns its params, dispatched by the material system. May migrate to
  push constants later.

All groups share **UBO binding slot 7**. The buffer bound to that slot
swaps per group. Group-shared struct definitions live in a single shader
include (`Shaders/<group>_params.glsl`); the same `#include` is used by
each shader in the group so the std140 layout is identical at link time.

| Group | Members | Cadence | UBO storage | Struct name | Fields ≈ |
| --- | --- | --- | --- | --- | --- |
| **A. Lit + compositor** | `SunLightAccumulationF`, `LightAccumulationFullScreen`, `SampleCubemapsF`, `ShadowmapSampling` (include), `AmbientLightingF`, `CombineF` | once each | `ubo.lit_compositor_params` | `LitCompositorParams` | 3 + 3 + 1 + 7 + 2 + 7 ≈ 23 (dedupe, e.g. `specular_ao_intensity`) |
| **B. Bloom** (done) | `BloomDownsampleF`, `BloomUpsampleF` | N (per-mip) | `ubo.bloom_params` (single 16-byte buffer; each shader reads only its subset) | `BloomParams` | 4 (2 + 1 + 1, fits one std140 block) |
| **C. Temporal upsample** | `TaaResolveF`, `temporal_upsample_ddgi`, `temporal_upsample_ssr` | once each | `ubo.temporal_params` | `TemporalParams` (union; shared `doc_*`/`amt`/`lastViewProj`/`*_flicker`/`use_reproject`/`dilate_velocity`; +TAA jitters; +SSR weight) | 11 ∪ 10 ∪ 11 ≈ 14 unique |
| **D. SSR pipeline** | `ssr_f`, `blur_ssr`, `ssr_downsample`, `ssr_upsample`, `ssr_apply_upsampled`, `reflectionShared` | once each (downsample/upsample iterate per-mip — re-upload in loop) | `ubo.ssr_params` | `SsrParams` | 17 + 2 + 3 + 4 + 1 + 2 ≈ 25 |
| **E. DDGI runtime** | `ddgiShadeF`, `bilateral_upsample_ddgi`, `ddgi_apply_upsampled`, `ddgiShadeDebugF` | once each | `ubo.ddgi_params` | `DdgiRuntimeParams` | 5 + 3 + 2 + 4 = 14 |
| **F. Cull pipeline** | `CullCompute`, `compact_mdi`, `cpu_vis_to_mdi`, `zero_instances_mdi`, `debugCull`, `DepthPyramidC` | 1–3 cull passes (re-upload per pass); DepthPyramid iterates per-mip | `ubo.cull_params` | `CullParams` | 5 + 2 + 1 + 1 + 3 + 4 = 16 |
| **G. Volumetric fog** | `VfogScatteringC`, `VfogRaymarchC` | once each | `ubo.volfog_pass_params` (separate from existing `buf.fog_uniforms` at binding 4) | `VolfogPassParams` | 4 + n |
| **H. SSAO blur** | `BilateralBlurF`, `hbao/linearizedepth` | once each | `ubo.ssao_blur_params` | `SsaoBlurParams` | 3 + 1 = 4 |
| **J. Probe / GI baking** | `compute_irrad_pos_C`, `get_best_cubemap_C`, `probeLumCalc_C`, `avgProbeCalc_C`, `trace_C`, `gather_C`, `PerlinGenF` | once each (editor / bake time) | `ubo.bake_params` | `BakeParams` | 4 + 4 + 1 + n + 5 + 1 + 6 ≈ 22 |
| **K. Raytrace debug** | `Raytracing/RaytracedShadows`, `Raytracing/RaytracedReflections`, `Raytracing/RaytraceSphere` | once each (debug / one-off) | `ubo.rt_debug_params` | `RtDebugParams` | 4 + 4 + 8 = 16 |
| **PER-DRAW** (own UBO each) | `MbSimpleV/F`, `MbTexturedV/F`, `MeshSimpleV`, `SimpleMeshV`, `MeshDebugProbeF`, `fullscreen_quad_textureF`, `Helpers/EqrtCubemap*`, `Helpers/PrefilterSpecular*`, `LightAccumulationV/F`, all `MASTER/*` material shaders | per-draw / per-instance / per-light | `ubo.<shader>_params` each (or push-const later) | `<Shader>Params` | varies |

Notes:
- A group's UBO size is fixed at the union of its fields' std140 layout.
  Adding a field grows the buffer and recompiles consumers. Empty groups
  start with the buffer allocated but unbound.
- Helper includes (`ShadowmapSampling.txt`, `reflectionShared.txt`)
  contribute their uniforms to whichever group includes them; they don't
  get their own UBO.
- `MASTER/*` material shaders are codegen-output; their `indirect_material_offset`
  + per-master tweakables migrate when the master-material pipeline lands.
- Naming convention: struct = `<Group>Params` (PascalCase); UBO member =
  `ubo.<group>_params` (snake_case); shader-side block name = `<Group>Params`
  (the struct name; std140 block-name identity is required for sharing
  across stages in OpenGL).

**Sweep order:** H → C → E → F → A → D → G → J → K. SSAO blur (H) first —
smallest pure-once-per-frame case (4 fields), exercises the shared-include
pattern without iteration. A (lit+compositor) follows once the include
pattern is locked. B (Bloom) already landed as the pilot; it stays
separate because both shaders iterate per-mip — folding them into A's
once-per-frame union would force a whole-struct re-upload on every mip
step, clobbering the lit/compositor fields. SSR (D) and Bake (J) are the
largest and saved for last.

### 2a follow-up — `#define` macro-shadow gotcha

The early A/E commits used `#define <name> params.<name>` aliases inside
shaders to keep the diff small. That backfired on H: `linearizedepth.txt`
has a function parameter literally named `clipInfo` (shadowing the
file-scope uniform), and `#define clipInfo params.clipInfo` mangled the
function signature to `vec4 params.clipInfo` — silent compile failure,
garbage depth-linearize output, solid red SSAO + a 4x4 grid pattern
across the screen.

Pattern locked going forward: **use `params.X` directly in shader
bodies. No `#define` aliasing for UBO fields.** Existing A/E `#define`s
are tolerated (no name conflicts there) but new migrations should not
introduce them. Two HBAO-shader-side cleanups also fell out of this:
file-scope global initializers reading from the UBO got moved into
functions/main, because some drivers handle UBO-vs-default-block
differently for that pattern (`g_Float2Offset`, `g_Jitter`,
`uvOffset`/`invResolution`).

### 2c — Pipeline-state consolidation — done

Landed across four sequential commits (see [[gfx_abstraction_changelog]] → "Sub-phase 2c status").

- **OpenglRenderDevice folded into OpenGLDeviceImpl.** The GL state cache used
  to live on a separate class held by `Renderer`; SDL3 GPU made that
  architecturally awkward (two abstraction boundaries instead of one). Cache
  state, invalid-bits, blend/depth/cull/program/vao/texture setters all moved
  inside the `IGraphicsDevice` impl, exposed on the interface as
  `set_pipeline` / `set_depth_write_enabled` / `get_active_shader` /
  `reset_state_cache` / `set_viewport` / `clear_framebuffer`. The individual
  state-cache setters (`set_shader_ptr`, `set_blend_state`,
  `set_show_backfaces`, `set_vao_raw`, `bind_texture_unit_raw`) were demoted
  to private internals of the backend in a follow-up: every external caller
  now goes through `set_pipeline(RenderPipelineState)` or
  `bind_texture(IGraphicsTexture*)`. The matching `Renderer::set_shader`,
  `Renderer::set_blend_state`, `Renderer::set_show_backfaces`,
  `Renderer::bind_vao`, and the `Renderer::bind_texture(int,int)` raw-id
  forwarder went with them; `Renderer::bind_texture_ptr` survives as a thin
  forward to `gfx().bind_texture`. Renderer no longer holds a `device` member; per-frame
  stats bump `draw.stats` directly. `draw.get_device()` kept as a transitional
  shim that returns `gfx()`.
- **RenderPipelineState::vao** is now `IGraphicsVertexInput*` (was
  `vertexarrayhandle`). Backend resolves to GL handle inside set_pipeline.
- **Folded onto `RenderPipelineState`:** `polygon_offset_{enabled,factor,units}`
  and a per-attachment `color_write_masks[]` array (default write-all). Decal
  pass builds the mask array on each pipeline; the explicit
  "reset all masks to true" loop is gone (the next set_pipeline elsewhere
  installs the default). Shadow + decal passes thread the polygon offset
  factor through `setup_batch` / `setup_batch2` rather than calling an
  immediate setter; ✕`set_color_write_mask` and ✕`set_polygon_offset` deleted
  from the interface.
- **`set_line_width` and `set_polygon_fill_mode` stay immediate.** The
  wireframe debug pass straddles many draws across `gbuffer_pass` invocations,
  so folding these onto pipeline state would require either plumbing through
  every nested helper or a push/pop stack — out of scope for 2c. SDL3 GPU has
  no line-width equivalent (silently capped to 1) and no equivalent for
  polygon-mode line; the wireframe path will be re-expressed when that backend
  lands.
- **Indirect-buffer binds folded onto draw calls.** ✕`bind_indirect_buffer`
  and ✕`bind_parameter_buffer` deleted; `multi_draw_elements_indirect` and
  `_count` now take `IGraphicsBuffer*` (plus byte offsets) directly. OpenGL
  backend caches `current_indirect` / `current_parameter` and skips
  `glBindBuffer` if unchanged. Lifetime-aware: `OpenGLBufferImpl::~` calls
  `opengl_backend_buffer_released` to clear those slots, so a stale pointer
  can never re-bind a dangling handle. The client-side MDI fallback (used
  when `use_client_buffer_mdi` is set) is preserved via a `client_ptr`
  parameter on `multi_draw_elements_indirect`.
- **SDL3 hash-keyed pipeline cache** still deferred (the SDL3 backend itself
  is Phase 3). Pipeline state is now structurally ready: every field that
  SDL3 GPU bakes at pipeline-create time lives on the struct.

Open follow-ups: `line_width` / `fill_mode` fold (above); the
"polygon offset units does nothing?" comment near the historic
`DrawLocal_RenderPass.cpp` setter still untested — units now stored on the
struct verbatim and forwarded to `glPolygonOffset`.

### 2d — Command encoder + frame lifecycle — done

- `gfx().begin_frame()` / `gfx().acquire_swapchain_texture()` /
  `gfx().submit_and_present()` added to `IGraphicsDevice`. ✕`present()`
  deleted; ✕`get_swapchain_texture()` renamed to `acquire_swapchain_texture()`
  (the SDL3 GPU model is acquire-once-per-frame; the returned pointer is
  still referenced multiple times by blit + render-pass code).
- OpenGL backend: `begin_frame()` sets `in_frame=true` and clears the
  PassMode tracker; `submit_and_present()` asserts the pairing and wraps
  `SDL_GL_SwapWindow`. Init-time GPU work (BRDF LUT integration in
  `EnviornmentMapHelper::init`) runs before any `begin_frame()` — OpenGL is
  lenient about pass ops outside a frame; the SDL3 backend will need init
  GPU work wrapped in an explicit begin/submit pair.
- Caller migration in `EngineMain_Loop.cpp`: `gfx().begin_frame()` at the top
  of each iteration of the main loop (and of `lua_error_loop`); the
  `wait_for_swap` lambda now calls `submit_and_present()` instead of
  `present()`. `skip_swap` / `skip_rendering` still suppress the present;
  the leftover `in_frame=true` is harmless because `begin_frame()` is
  idempotent.
- Single-threaded recording. Revisit only if profiling shows recording is the
  bottleneck (it isn't today).

Render/compute pass split was already runtime-checked inside the backend's
PassMode tracker (Sub-phase 1.4g); the lifecycle methods now sit alongside it
so the encoder boundary is observable from the interface.

### 2e — Per-frame buffer update model - done/HANDLE THIS IN ABSTRACTION LAYER

Mid-frame `glNamedBufferSubData` on GPU-active buffers (DrawLocal_SceneDraw.cpp:138,
DrawLocal_RenderPass.cpp:477-485, Ssao.cpp:264, Render_Lists per-frame upload
in 1.4e) is GL-driver-papered-over today. SDL3 GPU needs explicit transfer
buffers / ring buffers.

Promote `BUFFER_USE_DYNAMIC` (`IGraphicsDevice.h:46`) into a real per-frame
ring. `IGraphicsBuffer::sub_upload(offset, size, data)` on a DYNAMIC buffer
routes through the ring on SDL3; OpenGL keeps `glNamedBufferSubData`.
Testable on OpenGL by simulating frame-in-flight and asserting no in-flight
stomping.

### 2f — Shader reflection struct

`IGraphicsShader::reflect()` → vertex inputs, UBO/SSBO bindings, sampler
bindings. OpenGL uses `glGetProgramInterface*`; SDL3 GPU later uses
SPIRV-Cross. New `test_shader_reflection.cpp` covering 5 representative
shaders (Master*Shader.txt) asserting `reflect()` matches direct GL queries.

### Phase 2 verify

- `test_pipeline_cache.cpp` — bike demo creates ≤200 unique pipelines; decal
  variants ≤16.
- Synthetic UBO round-trip: every setter type, std140 offsets match shader.
- Visual diff on recorded frame: no decal MRT corruption, no depth-test
  regression.
- Per-frame buffer ring: 3-frames-in-flight stress test, no overwrite of
  in-flight data.

## Phase 2 prerequisites (open) — must land before Phase 3

Full breakdown in the Phase 3 plan file. Summary:

| Sub-phase | Scope | Status |
| --- | --- | --- |
| **B1** | Push-constant API (`push_{vertex,fragment,compute}_constants(slot, data, size)`) on `IGraphicsDevice`; migrate the 21 remaining `set_*(name, …)` callsites (DecalBatcher, RenderPass, Misc, Lighting, Debug, EnvProbe, RT/RaytraceTest_Shade) to per-shader `<Shader>PushConsts` blocks. Delete `IGraphicsShader::set_{bool,int,uint,float,mat4,vec*,ivec*}` + `set_block_binding` + `use()`. MUST FIT UNDER 128 BYTES. OpenGL impl backs push API with a reserved internal UBO slot + `sub_upload`. | **done** (sub-phases A..H). Push API at `kGfxPushConstBindingBase = 12` (VS 12-15, FS 16-19, CS 20-23); GL impl uses lazy 128B UBO per (stage, slot) orphaned via `glInvalidateBufferData`. Per-shader `<Shader>{Vert,Frag,Compute}PushConsts` structs in `Shaders/ShaderBufferShared.txt`. |
| **B2** | `IGraphicsShader::reflect()` → `{num_samplers, num_storage_textures, num_storage_buffers, num_uniform_buffers}` per stage. OpenGL impl via `glGetProgramInterfaceiv`. Required by `SDL_GPUShaderCreateInfo`. | **done**. `IGraphicsShader::Reflection` (`{vertex, fragment, compute}: PerStageCounts`). GL impl walks `GL_UNIFORM` (sampler vs image filtered by `GL_TYPE`), `GL_UNIFORM_BLOCK`, `GL_SHADER_STORAGE_BLOCK` with `GL_REFERENCED_BY_{VERTEX,FRAGMENT,COMPUTE}_SHADER`. Covered by `test_shader_reflection.cpp` on MbTexturedV/F. |
| **B4** | `IGraphicsDevice::push_debug_group(const char*) / pop_debug_group()`. SDL3 has `SDL_PushGPUDebugGroup` but no timestamp queries — Profiler GPU-timing stubs on SDL3, debug groups stay live. Drops `Framework/Profilier.cpp` + `IntegrationTests/GpuTimer.{h,cpp}` off the static-scanner accept-list. | **done**. `push_debug_group`/`pop_debug_group` on `IGraphicsDevice` (GL: `glPush/PopDebugGroup`). `IGraphicsTimerQuery` opaque resource — `record_timestamp` / `is_available` / `read_timestamp_ns`; GL impl wraps `glQueryCounter(GL_TIMESTAMP)`. `Profilier.cpp` uses two timer queries per scope (elapsed = stop − start). `ScopedGpuTimer` rewritten on top of the same API. Static-scanner accept-list down to `External/` + `Render/OpenGl` only. |
| **B5** | SDL2 → SDL3 windowing + `imgui_impl_sdl2` → `imgui_impl_sdl3`. SDL2_mixer → SDL3_mixer (`MIX_Mixer`/`MIX_Track*` API, float32 cooked-mix callback for low-pass). Independent of B1–B4 — parallel work stream. Required before Phase 3. | **done**. vcpkg `sdl3` 3.4 + `sdl3-mixer` 3.2.2. `SDL_ENABLE_OLD_NAMES` project-wide so `SDL_CONTROLLER_*` / `SDL_GameController*` aliases at call-sites survive via SDL3's `SDL_oldnames.h`. `SDL_GetMouseState`/`SDL_GetRelativeMouseState` widened to `float*` boundary, `SDL_GetKeyboardState` returns `const bool*`. `SDL_GameController*` → `SDL_Gamepad*` in `InputSystem` with instance-id Device::index (was joyindex). `imgui_impl_sdl3` vendored from imgui v1.91.0 + patched for 1.89.4 core (drop `AddMouseSourceEvent`, F13–F24, `PlatformSetImeDataFn` rename). SDL3_mixer rewrite: voice-channels → pre-allocated `MIX_Track*` pool, pitch via `MIX_SetTrackFrequencyRatio` (drops manual `PlaybackSpeedEffectHandler`), low-pass via `MIX_SetTrackCookedCallback` on float32 PCM. 185 unit tests + integration suite (game + editor) green. |

**Push-constant model (B1):** slot-indexed block uploads, mirroring `SDL_PushGPU{Vertex,Fragment,Compute}UniformData(cmdbuf, slot, data, size)`. The interface is block-shaped (no name-based scalar setter on top). Per-pass group UBOs at slot 7 stay UBOs — push constants are for *per-draw* data only. 21 callsites all per-draw / per-instance, so the migration target is push-const not slot-7 UBO.

**Followup — drop `slot` param (B1/I, planned):** every one of the 21 callsites uses `slot=0`. Slot axis is YAGNI: per-pass vs per-draw frequency split is already done at the UBO-vs-push-const seam (group UBOs at binding 7 own per-pass), not within push-const. Plan:
- Delete `slot` from `push_{vertex,fragment,compute}_constants` — new signature `(const void* data, int size)`.
- Delete `kGfxMaxPushConstSlotsPerStage`. Binding formula collapses to `kGfxPushConstBindingBase + stage_idx`. Reserved UBO bindings shrink 12 → 3 (one per stage; e.g. 12=vertex, 13=fragment, 14=compute).
- SDL3 backend hardcodes `slot=0` when calling `SDL_PushGPU*UniformData`. Trivially reversible if a second slot is ever needed.
- Mechanical sweep across the 21 callsites (`gfx().push_vertex_constants(0, &pc, sizeof(pc))` → `gfx().push_vertex_constants(&pc, sizeof(pc))`) plus shader `layout(binding=N)` renumbering.

**SDL3 "push uniforms" are not Vulkan push constants.** Confirmed by reading `SDL_gpu_vulkan.c@main`: `vkCmdPushConstants` appears zero times; `pipelineLayoutCreateInfo.pPushConstantRanges = NULL` at both pipeline-layout sites. `VULKAN_INTERNAL_PushUniformData` (line ~7837) implements push-uniforms as a **ring-buffered VkBuffer (UBO) with dynamic offsets** — per-(stage,slot) cursor, `SDL_memcpy` at `writeOffset`, bump by std140-aligned `blockSize`, flag descriptor-set/dynamic-offset rebind. Acquires a fresh buffer from `uniformBufferPool` when full. This is structurally identical to our OpenGL backend (orphan + `glNamedBufferSubData` + `glBindBufferBase`), just using Vulkan's dynamic-offset idiom instead of GL's buffer-orphan idiom. Our design is not a hack — it mirrors SDL3 internals. Metal (`setVertexBytes`) and D3D12 (root constants) are the only backends that map to a true inline path.

## Phase 3 — SDL3 GPU backend port (planned)

Recommended order (full detail in plan file):

| Step | Scope |
| --- | --- |
| **P3.1** | **Toolchain spike.** glslang as DLL (`vcpkg install glslang spirv-cross`; `<VcpkgApplocalDeps>true</>` for auto-copy). Lift source-loader from `OpenGlShaderImpl.cpp` anon namespace to shared `Source/Render/ShaderSourceLoader.{h,cpp}`. New `Source/Render/SpirvCompile.{h,cpp}` (`glslang::InitializeProcess` → `TShader.parse` → `glslang::GlslangToSpv`). Test: 5 shaders (lit master, bloom, cull, vfog, raytrace) round-trip GLSL → SPIR-V → SPIRV-Cross reflection. No backend code. |
| **P3.2** | **Pipeline cache.** Inside SDL3 backend `set_pipeline`: `unordered_map<XXH3_64bits(&RenderPipelineState), SDL_GPUGraphicsPipeline*>`. Miss → `SDL_CreateGPUGraphicsPipeline` (rasterizer = fill_mode+cull+front_face+depth-bias from polygon_offset_*, depth-stencil, per-attachment blend, color/depth target formats from current pass, vertex input from `IGraphicsVertexInput`). Cache key includes target formats — `set_pipeline` must run inside a render pass (already true). Invalidate on `Program_Manager::recompile_all`. |
| **P3.3** | **Frame skeleton + command encoder.** `begin_frame` → `SDL_AcquireGPUCommandBuffer`. `acquire_swapchain_texture` → `SDL_WaitAndAcquireGPUSwapchainTexture` (lazy, first call). `submit_and_present` → `SDL_SubmitGPUCommandBuffer`. `set_render_pass` ends any active pass, translates `ColorTargetInfo[]` → `SDL_GPUColorTargetInfo[]` (load_op = CLEAR if wants_clear else LOAD, store_op = STORE), `SDL_BeginGPURenderPass`. `begin_compute_pass` defers `SDL_BeginGPUComputePass` to first dispatch so writable bindings are known. |
| **P3.4** | **Bind-table flushing.** Per-stage tables: `vert_samplers[N] = (tex, samp)`, `frag_samplers[N]`, `vert/frag/compute_uniform_buffers[N]`, storage buffers, storage textures. `bind_texture`/`bind_sampler`/`bind_*_buffer_*` write the table; draw call flushes via `SDL_BindGPU{Vertex,Fragment,Compute}{Samplers,UniformBuffers,StorageBuffers,StorageTextures}`. Vertex buffers from `IGraphicsVertexInput` layout. Per-draw push constants via `SDL_PushGPU*UniformData` (B1 already exposes the slot-indexed API). |
| **P3.5** | **Y-flip + winding.** SDL3 NDC is +Y down. Flip projection matrix row 1 inside SDL3 view-constant upload; swap front-face winding in `set_pipeline`; audit `gl_FragCoord.y` / screen-UV-from-NDC consumers (post-process stack — TAA resolve, bloom composite, SSR apply). Visual diff catches regressions via `bake_probes_test` + `demo_level_1_shots`. |
| **P3.6** | **HiZ filter-minmax replacement.** `GraphicsSamplerReduction::{Min, Max}` has no SDL3 equivalent. Compute min/max pre-pass building reduced mip chains. Consumers: `SSRSystem::hiz_max_sampler`, `GpuCullingTest::hiZSampler`. New `Data/Shaders/HiZReduce.glsl`. |

RT test paths (`Source/Render/RT/*`) port as-is — confirmed they use plain `bind_texture`, no bindless, no texture handles. They come along with the rest of the compute paths in P3.4.

### Backend layout

```
Source/Render/SDL3Gpu/SDL3GpuDevice.cpp        // gfx_init_sdl3gpu + IGraphicsDevice impl
Source/Render/SDL3Gpu/SDL3GpuShaderImpl.cpp    // IGraphicsShader impl
Source/Render/SDL3Gpu/SDL3GpuTextureImpl.cpp   // IGraphicsTexture impl
Source/Render/SDL3Gpu/SDL3GpuBufferImpl.cpp    // IGraphicsBuffer impl + transfer-buffer ring
Source/Render/SDL3Gpu/SDL3GpuLocal.h           // pipeline-hash cache, internal helpers
```

Mirror of the `OpenGl*` layout. Free functions `gfx_sdl3gpu_pre_window_setup()` / `gfx_init_sdl3gpu(SDL_Window*)`; shared `gfx_shutdown()`. Backend selected at engine init (cvar or compile flag — TBD).

### Phase 3 verify

- `test_pipeline_cache.cpp` — ≤200 unique pipelines in bike demo, ≤16 decal variants.
- `test_sdl3_parity.cpp` runs the same scene on both backends, captures
  screenshots via `t.capture_screenshot` (test_obs_smoke.cpp:60), asserts
  perceptual diff < threshold (SSIM ≥ 0.98 per pass) for each G-buffer MRT,
  shadow cascades, deferred lighting, decal blend, SSAO, SSR, TAA, bloom,
  post-composite.
- `test_dynamic_buffer_ring.cpp` — 3 frames in flight, no slot overwrite.
- `test_shader_reflection.cpp` — 5 representative shaders, reflection counts match `glGetProgramInterfaceiv`.
- Existing screenshot suite (`bake_probes_test`, `demo_level_1_shots`) within soft-fail bands on both backends.
- Static scanner stays green — every new `SDL_GPU*` call lives inside `Source/Render/SDL3Gpu/`.


## Out of scope

- WebGPU / Emscripten target. SDL3 GPU doesn't target WebGPU. If web becomes
  real later, that's a second backend against this same interface with stricter
  constraints (no SSBO baseline, limited compute). Worth pressure-testing the
  Phase 2 API against WebGPU's binding model on paper before locking it.
- DX12 / Metal / mobile Vulkan — implicitly covered by SDL3 GPU's backends.
- SDL2 → SDL3 windowing + ImGui platform layer migration — required before
  Phase 3 but a separate parallel work stream (B5).
- SDL3 GPU has no tess shaders. Adaptive view-dependent factors in
`MaterialLocal_Shader.cpp:81` (`Program_Manager::create_single_file(...,
is_tesselation=true)`) → per-frame compute pre-pass writing displaced
vertex/index buffers, then indirect draw with a regular vertex shader. Sized
to worst-case LOD. Treat as a sub-project; only required if any live master
shader uses `is_tesselation=true` at SDL3-port-time. Verify before shipping.

### QUESTIONS

- `| **B3** | Per-frame ring buffer impl for `BUFFER_USE_DYNAMIC` (2e — decision made, impl not done). `IGraphicsBuffer::sub_upload` on DYNAMIC routes through N×size ring, head advances on `begin_frame()`. OpenGL keeps `glNamedBufferSubData`; teeth on SDL3 transfer-buffer path. | **deferred to SDL3 backend** — OpenGL path (`glNamedBufferSubData`) is a no-op on GL, all teeth are on the SDL3 transfer-buffer side. Implement inside the SDL3 backend during Phase 3 rather than as a prereq. |`
keep this? or no? Opengl doesntneed to use ring buffers. SDL3 gpu internally uses ring buffers.

### Footnote — D3D12 enablement (out of scope, future P3.9)

SPIR-V is Vulkan-only. SDL3 GPU on Windows picks backend from the shader
formats supplied to `SDL_CreateGPUDevice`: `SDL_GPU_SHADERFORMAT_SPIRV` →
Vulkan; `SDL_GPU_SHADERFORMAT_DXIL` → D3D12 (signed DXIL); `SDL_GPU_SHADERFORMAT_MSL` → Metal.
Phase 3 ships SPIR-V only → SDL3 runs on its Vulkan backend on Windows.
D3D12 enablement later via `SDL_shadercross` (separate SDL helper lib that
translates SPIR-V → DXIL/MSL at runtime in dev, prebake DXIL for release).
Not a Phase 3 blocker.
