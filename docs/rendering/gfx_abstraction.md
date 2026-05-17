# Graphics Device Abstraction

This is the canonical migration doc. The earlier strategy doc at
`~/.claude/plans/i-want-to-put-idempotent-fairy.md` is historical — Phase 2c
decisions there are superseded by **Pipeline model** below.

Phase 3 implementation plan (detailed): `~/.claude/plans/review-gfx-abstraction-md-and-sdl3gpu-wh-cuddly-bunny.md`.

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

## Sub-phase 2c status

Pipeline-state consolidation. Landed across four commits:

1. **Fold OpenglRenderDevice into the IGraphicsDevice backend.** Single
   abstraction boundary; the cache, the resource factory, and the draw entry
   points all live in `OpenGLDeviceImpl`. `DrawLocal_Device.{h,cpp}` is now
   zero-`gl*` and dropped from the leak scanner's accept-list. Default GL
   state (cubemap-seamless, depth-test, cull-face, reverse-Z clip,
   clear-depth=0) set inside the impl constructor — no
   `Renderer::InitGlState` step needed.
2. **`RenderPipelineState::vao` → `IGraphicsVertexInput*`.** Backend resolves
   to GL handle inside `set_pipeline`. `Renderer::get_empty_vao()` returns the
   pointer.
3. **Fold per-pipeline immediate setters.** `polygon_offset_*` and
   `color_write_masks[]` move onto `RenderPipelineState`; immediate
   `set_color_write_mask` and `set_polygon_offset` deleted. `setup_batch` /
   `setup_batch2` take a `poly_offset_factor` parameter so the shadow pass
   sets it per-pipeline rather than wrapping the loop in immediate state.
4. **Indirect buffer onto draw call.** `multi_draw_elements_indirect{,_count}`
   take `IGraphicsBuffer*` plus byte offsets. ✕`bind_indirect_buffer` and
   ✕`bind_parameter_buffer` deleted. OpenGL backend caches last-bound buffer
   per slot and clears it on buffer release.

185 unit tests + integration suite green after each step.

## Sub-phase 1.9 status

The Phase 1 boundary gate. After 1.8 the doc claimed only 5 files leaked raw
`gl*`; the actual count was 147 calls across 17 non-backend files (after
stripping comments). Scope was expanded to migrate every active leak rather
than accept-list around them.

- Static scanner: `Scripts/check_no_gl_leaks.py` walks `Source/**/*.{h,cpp,c}`
  (excluding `External/` and the accept-list paths), strips block + line
  comments, regex-matches `\bgl[A-Z][a-zA-Z0-9_]*\s*\(`. Exits 0 on clean,
  exits 1 with `file:line:match` on any leak. Wrapped by
  `Source/UnitTests/legacy_gl_calls_test.cpp` (`LegacyGlCalls.NoneOutsideBackend`)
  so it lands in the standard unit-test count (185 total now). The original
  plan's runtime `r.gpu.no_legacy_calls` cvar idea was dropped — would have
  required intercepting every glad function pointer for one-time gate value.
- Accept-list (declared in the scanner):
  - `Source/Framework/Profilier.cpp`, `Source/IntegrationTests/GpuTimer.{h,cpp}`
    — **OpenGL-only.** GPU timer queries (`glQueryCounter` /
    `glGetQueryObjectui64v`) and debug groups (`glPushDebugGroup` /
    `glPopDebugGroup`) have no SDL3 GPU equivalent today. These TUs become
    no-ops or `#ifdef GFX_BACKEND_OPENGL`-gated when the SDL3 backend lands.
  - `Source/Render/DrawLocal_Device.{h,cpp}` — transitional invalid-bits
    state cache, folded into backend in Phase 2.
- Caller migrations (call-site only, no behavioural change unless noted):
  - `Framework/MeshBuilder.cpp` — VAO/VBO/EBO → `IGraphicsVertexInput` +
    `IGraphicsBuffer`. Per-update `upload()` re-specs storage (matches prior
    `STREAM_DRAW`). `dd.draw()` now issues `gfx().draw_elements` and assumes
    caller has set `state.vao = dd.vao->get_internal_handle()` in the
    pipeline — 4 callers updated (Lighting ×2, Debug, EnvProbe).
  - `Game/Components/LightComponents_GiScene.cpp` + `Render/DdsExport.cpp` —
    `glBindTexture` + `glGetTextureImage/SubImage` pairs collapsed onto
    `IGraphicsTexture::download`. `DdsExport` signatures changed from
    `GLuint` to `IGraphicsTexture*`. Dead helpers `export_float_texture` /
    `ExportCubemapArrayHDR` deleted.
  - `IntegrationTests/Screenshot.cpp` + `RenderDump.cpp` — `glGetTextureImage`
    → `IGraphicsTexture::download`. `RenderDump` now per-format dispatches
    (rgb16f/rgba16f/r11f/rg16f/depth) and normalises into RGBA8 PNG.
  - `Render/EnvProbe.cpp` `BRDFIntegration::run` (54 raw calls) — full
    migration: `lut_id` (uint32) → `IGraphicsTexture* lut_tex` (rgb8,
    LinearClamped); `depth` (uint32) → `IGraphicsTexture* depth_tex`
    (depth24f, NearestClamped); raw FBO + framebuffer attach → `set_render_pass`
    with `wants_clear`/`clear_color`/`wants_depth_clear`; raw quad VAO/VBO
    dropped (the MeshBuilderDD `dd` is the actual VAO consumed); pipeline now
    routed via `RenderPipelineState`. `get_texture()` returns `IGraphicsTexture*`;
    3 consumers (SSR, RT_Shade, RenderPass) switched to `bind_texture_ptr`.
    Dead `drawdebug()` deleted.
  - `Render/Meshlet.cpp` — entire file body lived under `#if 0`; replaced
    with a one-line stub. The static scanner doesn't honour preprocessor
    branches.
  - `Render/TextureUpload.cpp` — `glGenerateTextureMipmap` →
    `IGraphicsTexture::generate_mipmaps` (new API on the interface). The
    dev-only `try_copy` benchmark (compressed-texture copy via raw
    `glCreateTextures`/`glCopyImageSubData`) deleted; reinstate when a
    BC-format texture create + a `copy_texture(src,dst)` helper land.
  - `Render/RenderGiManager.cpp` — `glCopyImageSubData` cube-face copy →
    `gfx().copy_texture` (new API on the device, see below). SSBO readback
    via `glBindBuffer` + `glMapBuffer` + `glUnmapBuffer` → `download_buffer`
    (existing API). Guards the readback against `num_cubemaps == 0` (the old
    code silently no-op'd via `glMapBuffer` returning the empty range; new
    `download_buffer` asserts `size > 0`).
  - `Render/RenderExtra.cpp` — `glEnable/glScissor/glDisable(GL_SCISSOR_TEST)`
    → `gfx().set_scissor` / `disable_scissor`.
  - `Render/RenderExtra_LightCookieAtlas.cpp` — final stray `glDrawArrays`.
  - `Render/DrawLocal_SceneDraw.cpp` — `glDrawArrays` (×3),
    `glBindBufferBase(SSBO)` → `bind_storage_buffer_base`, `glDepthMask` →
    `OpenglRenderDevice::set_depth_write_enabled` (transitional, folds in
    Phase 2). Dead `set_default_parameters` lambda deleted.
  - `Render/DrawLocal_Scene.cpp` — `glNamedBufferData` on
    `gpu_instance_buffer` → `upload()`.
  - `Render/DrawLocal_Editor.cpp` — thumbnail readback: `glReadBuffer` +
    `glReadPixels` → `IGraphicsTexture::download`.
  - `Render/DrawLocal_Init.cpp` — capability dump + GL debug-output enable
    moved out of `Renderer` into the OpenGL backend
    (`gfx_opengl_dump_capabilities` / `gfx_opengl_enable_debug_output`,
    free functions declared next to `gfx_init_opengl`). Empty VAO + dummy VB
    now allocated via `gfx().create_vertex_input` / `create_buffer`;
    `vao.default_` is `IGraphicsVertexInput*` (previously raw handle),
    `buf.default_vb` is `IGraphicsBuffer*`. Stale `glBindFramebuffer(0)` at
    the top of `InitFramebuffers` deleted.
- Hygiene: `glCheckError()` macro in `Framework/Util.h` renamed to
  `gfx_check_gl_error()` (6 files updated) so the scanner regex doesn't
  need a special case for a macro that's structurally not a `gl*` call.
- Deletions: `Source/Render/BufferTest/{glbuffer.h,glbuffer.cpp,gl_bufferlock.h}`
  — persistent-mapped buffer prototype superseded by Phase 2e ring buffer.
  Project entries removed from `CsRemake.vcxproj` + `.filters`.
- API additions during this sub-phase: `IGraphicsTexture::generate_mipmaps`;
  `IGraphicsDevice::copy_texture(src, src_mip, src_layer, dst, dst_mip,
  dst_layer, w, h)`; download-format table extended to cover `rgb8`,
  `rgba16f`, `rg16f`/`rg32f`, `r11f_g11f_b10f` so the texture-debug + DDS-bake
  paths can round-trip native formats.
- Smoke-test the gate: adding any `gl*` outside `Source/Render/OpenGl*` or
  the accept-list immediately fails the scanner with `file:line:match`.
  Verified during 1.9 by re-injecting `glClear(0)` into
  `DrawLocal_SceneDraw.cpp`.
- 185 unit tests (incl. new `LegacyGlCalls.NoneOutsideBackend`) + integration
  suite green; `bake_probes_test` + `demo_level_1_shots` within their
  soft-fail bands.

## Sub-phase 1.8 status

- API added on `IGraphicsDevice`: `set_vsync(bool)` (wraps `SDL_GL_SetSwapInterval`),
  `present()` (wraps `SDL_GL_SwapWindow`), and the ImGui platform/renderer
  lifecycle — `imgui_init`, `imgui_shutdown`, `imgui_new_frame`,
  `imgui_render_draw_data`, `imgui_process_event(const SDL_Event*)`. The
  backend hides the `imgui_impl_sdl2.h` + `imgui_impl_opengl3.h` includes from
  the rest of the codebase; `imgui_render_draw_data` binds the default
  framebuffer internally before issuing the GL3 draw (mirrors the SDL3 GPU
  model where the swapchain texture is acquired by the encoder pre-draw).
- Free-function init split: `gfx_opengl_pre_window_setup()` sets the
  `SDL_GL_SetAttribute` (major/minor/depth/double-buffer) — MUST run before
  `SDL_CreateWindow`. `gfx_init_opengl(SDL_Window*)` now takes the window,
  creates the `SDL_GLContext`, calls `gladLoadGLLoader`, sets the default
  swap interval, and constructs `OpenGLDeviceImpl`. The backend owns the GL
  context for the rest of the process; `gfx_shutdown()` destroys the device
  AND `SDL_GL_DeleteContext`s the context.
- `init_sdl_window` in `EngineMain_Init.cpp` collapses from manual SDL_GL_*
  setup + glad load + ImGui platform init down to: `SDL_Init` →
  `gfx_opengl_pre_window_setup()` → `SDL_CreateWindow(... SDL_WINDOW_OPENGL ...)`
  → `gfx_init_opengl(window)`. Renderer init no longer calls
  `gfx_init_opengl`; it asserts `gfx_is_initialized()` instead, because the
  device is live before the renderer comes up.
- `GameEngineLocal::gl_context` field deleted (was only ever read by ImGui
  init, which now lives in the backend). The `SDL_Window* window` member
  stays in `GameEngineLocal` — non-GL SDL2 ops (`SDL_GetWindowSize`,
  `SDL_SetWindowFullscreen`, `SDL_SetWindowTitle`, `SDL_WarpMouseInWindow`)
  still consume it directly.
- Caller migrations: `EngineMain_Loop.cpp::loop` — `SDL_GL_SwapWindow` →
  `gfx().present()`; `ImGui_ImplSDL2_ProcessEvent` →
  `gfx().imgui_process_event`; the explicit `glBindFramebuffer(0)` +
  `ImGui_ImplOpenGL3_RenderDrawData` pair → `gfx().imgui_render_draw_data()`
  (FBO 0 bind moved into the backend impl). `GUISystemLocal.cpp` —
  `ImGui_ImplSDL2_NewFrame` + `ImGui_ImplOpenGL3_NewFrame` →
  `gfx().imgui_new_frame()`. `DrawLocal_SceneDraw.cpp::sync_update` —
  the `enable_vsync` cvar branch collapses to `gfx().set_vsync(...)`.
- `glad/glad.h` include dropped from `EngineMain_Loop.cpp` and
  `EngineMain_Init.cpp`. ImGui platform/renderer impl headers no longer
  included anywhere outside `OpenGlDevice.cpp`.
- 184 unit tests + 80 integration tests green; `bake_probes_test` screenshot
  within its soft-fail band (max_delta=68, 8 pixel diff, 0.001%).

## Sub-phase 1.7d status

- Program-binary cache (load + timestamp check + `glProgramBinary` /
  `glGetProgramBinary` save) moved from `Program_Manager::recompile_shared` /
  `recompile_normal` (DrawLocal_Device.cpp) into anonymous-namespace helpers
  in `OpenGlShaderImpl.cpp` (`try_load_program_binary`, `save_program_binary`,
  `make_cache_filename`). Caching is now transparent inside the factories —
  `opengl_create_shader_vert_frag`, `opengl_create_shader_vert_frag_geo`, and
  `opengl_create_shader_single_file` do load → fallback-compile → save in one
  call. Compute and single-file-tess factories skip the cache (matches prior
  behavior — those paths never cached).
- `Program_Manager` collapsed: `recompile_shared` / `recompile_normal` /
  `compute_hash_for_program_def` / `release_prior_program` deleted. `recompile_do`
  is now a simple dispatch — release prior `gfx_shader`, then call the
  appropriate `gfx().create_shader_*`. All five compile branches funnel through
  the backend factory; no `gl*` calls remain in `DrawLocal_Device.cpp` along
  the shader path.
- `Shader.h` and `Shader.cpp` deleted. The legacy `Shader::compile*` static
  methods became free anonymous-namespace functions in `OpenGlShaderImpl.cpp`
  returning `uint32_t` program-id (0 on failure); the uniform setters + `use()`
  live entirely on `OpenGLShaderImpl` (Sub-phase 1.7c moved those). Removed
  `#include "Render/Shader.h"` from DrawLocal.h, EnvProbe.h, MaterialLocal.h,
  RenderExtra.h, DrawLocal_Device.h.
- `Program_Manager::program_def` now holds only `IGraphicsShader* gfx_shader`
  (no more `Shader shader_obj` mirror). `Program_Manager::get_obj(handle)`
  returns `IGraphicsShader*` instead of `Shader`. `Renderer::shader()` and
  `OpenglRenderDevice::shader()` return `IGraphicsShader*`; uniform-setter
  call sites switched from `.set_X` to `->set_X`.
- `RenderPipelineState::program` migrated from `program_handle` (int) to
  `IGraphicsShader*`. This was originally scheduled for Phase 2c — lifted into
  1.7d so the program field can be hashed straight from the struct in the
  Phase 2c SDL3 pipeline cache. All 30+ `state.program = X` callers (RenderPass,
  BatchScene, SceneDrawInternal, Lighting, RenderPass bloom, Misc, Debug,
  SSAO, SSR, EnvProbe, DecalBatcher, GpuCullingTest, RaytraceTest_Shade,
  LightCookieAtlas, etc.) wrap the handle via
  `draw.get_prog_man().get_obj(handle)`. Default value is `nullptr`.
- `OpenglRenderDevice::set_pipeline` calls a new `set_shader_ptr(IGraphicsShader*)`
  internal that diffs against the cached `active_program` (now also
  `IGraphicsShader*`). `set_shader(program_handle)` survives as a thin wrapper
  for the compute-dispatch call sites (`GpuCullingTest`, `Volumetricfog`,
  `RaytraceTest_Probe`, `RenderExtra_SSR::hiz_downsample`) — keeps the
  `program_handle` API surface intact for non-pipeline contexts.
- The transitional `opengl_wrap_program_handle` helper is gone — no
  out-of-backend callers wrap a raw GL program id anymore.
- The long-form `RenderPipelineState(bool, bool, ..., program_handle, ...)`
  constructor (unused) removed.
- 184 unit tests + integration suite (80 tests) green; `bake_probes_test`
  screenshot within its soft-fail band (max_delta=68, 3 pixel diff).

## Sub-phase 1.7c status

- API added on `IGraphicsShader`: `use()`, 10 uniform setters (`set_bool / set_int / set_uint / set_float / set_mat4 / set_vec2 / set_vec3 / set_vec4 / set_ivec2 / set_ivec3`), and `set_block_binding`. `OpenGLShaderImpl` implements them with the DSA variants (`glProgramUniform*` / `glUniformBlockBinding`) — no need to bind the program first, which means uniform sets are safe even if the active program got swapped out between `set_pipeline()` and the setter call.
- `Shader` struct grew an `IGraphicsShader* gfx_handle` field. All `Shader::set_*` / `use()` / `set_block_binding` bodies now forward to `gfx_handle->set_*`. Early-return on null `gfx_handle` (matches the legacy silent-no-op when `glGetUniformLocation` returned -1 on a zero program id during init).
- `Shader.cpp` contains zero `gl*` calls. The whole TU is forwarding wrappers; the data lives on `OpenGLShaderImpl`.
- `Program_Manager` lifecycle: drained `release_prior_program(def)` is now a private method called once at top of `recompile_do` (was an inline lambda only used in two branches). Binary-cache paths (`recompile_shared` + `recompile_normal`) wrap their freshly-minted GL program id into `def.gfx_shader` via `opengl_wrap_program_handle` after a successful cache load OR `Shader::compile*` fallback. Single-rooted ownership preserved: gfx_shader owns when set, otherwise the raw shader_obj.ID (now only the in-flight scratch slot before wrapping).
- `Program_Manager::get_obj(handle)` now populates `Shader::gfx_handle` on the returned-by-value copy from `def.gfx_shader`. Every Shader handed out via `device.shader()` has a live `gfx_handle`.
- Added `opengl_wrap_program_handle(uint32_t)` backend helper (declared in `OpenGlDeviceLocal.h`). Transitional — once 1.7d folds the binary-cache GL calls into the backend TU, the wrap helper becomes an internal detail.
- `DrawLocal_Device.cpp` now includes `OpenGlDeviceLocal.h` so it can reach `opengl_wrap_program_handle`. Acceptable transitional coupling (`DrawLocal_Device.cpp` is essentially backend code; the binary-cache machinery folds into `OpenGlShaderImpl.cpp` in 1.7d).
- `EnvProbe::BRDFIntegration::integrate_shader` migrated from `Shader` to `IGraphicsShader*`; `Shader::compile(&integrate_shader, …)` becomes `gfx().create_shader_vert_frag(…)`. Remaining raw GL in `BRDFIntegration::run` (FBO/texture/quad VBO setup) is unrelated and stays for a later misc cleanup.
- Dead code in `Ssao.cpp` (`make_program` helper — defined, never called) deleted.
- 184 unit tests + integration suite green.

## Sub-phase 1.7b status

- Source-loader (`SourceAndLinenums`, `read_and_add_recursive`, `get_definess_with_directive`, `count_characters`, `get_source`, `make_shader`) moved from `Shader.cpp` into `OpenGlShaderImpl.cpp` (anonymous namespace).
- All five `Shader::compile*` static method bodies moved from `Shader.cpp` into `OpenGlShaderImpl.cpp`. `Shader.h` is unchanged so external callers (Program_Manager's binary-cache fallback, EnvProbe BRDFIntegration, etc.) continue to compile. Linker resolves the statics in the backend TU.
- `Shader.cpp` shrinks from 517 lines to ~45 lines — only uniform setters + `use()` + `set_block_binding` remain (12 GL calls, migrate in 1.7c).
- `opengl_create_shader_*` factory bodies still delegate to `Shader::compile*` — same TU now, so it's just an internal call.
- 184 unit tests + integration suite green.

## Sub-phase 1.7a status

Scaffolding only — no `gl*` calls actually move into the backend yet. Sub-phase 1.7 was split into four parts because the work involves the interface, the source-loader, the uniform-setter surface (used by every render subsystem), and the program-binary cache (which lives in `DrawLocal_Device.cpp` and runs *alongside* `Shader::compile*` on the same `program_def`). Doing all four at once made the diff unbisectable.

- Interface: `IGraphicsShader { release(); get_internal_handle(); }`. Phase 2c migrates `RenderPipelineState::program` from `program_handle` (int) to `IGraphicsShader*`; for now it stays as an integer that indexes `Program_Manager::programs`.
- Factory: 5 methods on `IGraphicsDevice` — `create_shader_vert_frag`, `create_shader_vert_frag_geo`, `create_shader_compute`, `create_shader_single_file`, `create_shader_single_file_tess`. Returns `IGraphicsShader*` (heap), or `nullptr` on compile/link failure.
- `OpenGlShaderImpl.cpp` houses `OpenGLShaderImpl` and the 5 factory bodies. **For 1.7a, factory bodies delegate to the legacy `Shader::compile*` helpers** and wrap the resulting GL handle. The actual `glCreateShader` / `glCompileShader` / `glAttachShader` / `glLinkProgram` calls migrate in 1.7b.
- `Program_Manager::program_def` gains `IGraphicsShader* gfx_shader`. The compute and shared+tesselation paths in `recompile_do` now route through `gfx().create_shader_*`; `def.gfx_shader` owns the program, `def.shader_obj.ID` mirrors `gfx_shader->get_internal_handle()` so `Shader::use()` and the existing uniform setters keep working.
- Binary-cache paths (`recompile_shared` + `recompile_normal`) untouched — they still own the program via raw `def.shader_obj.ID`. `def.gfx_shader` stays `nullptr` there. Ownership stays single-rooted by branch (a given `program_def` always takes the same recompile branch on every recompile).
- Lifetime: `release_prior_program()` lambda in `recompile_do` drains the prior program before installing the new one — releases via `safe_release(gfx_shader)` if the previous compile produced an `IGraphicsShader`, else falls back to raw `glDeleteProgram(shader_obj.ID)`.
- 184 unit tests + integration suite (80 tests) green; bake_probes + demo_level_1 screenshots within their soft-fail bands.

## Sub-phase 1.6 status

- API added: `GraphicsFillMode { Fill, Line }` + `set_polygon_fill_mode(GraphicsFillMode)` over `glPolygonMode(GL_FRONT_AND_BACK, …)` — immediate setter; Phase 2c folds onto `RenderPipelineState::fill_mode` (see [Pipeline model](#pipeline-model)).
- `Renderer::render_bloom_chain` signature migrated from `texhandle` to `IGraphicsTexture*`. Internal `device.bind_texture(0, scene_color_handle)` becomes `bind_texture_ptr(0, scene_color)`.
- `DrawLocal_SceneDrawInternal.cpp`: wireframe pass `glPolygonMode`/`glLineWidth` pair → `gfx().set_polygon_fill_mode` + `gfx().set_line_width`. 3× `glDrawArrays(GL_TRIANGLES, 0, 3)` (TAA resolve, composite, post-process stack) → `gfx().draw_arrays(Triangles, 0, 3)`. `bind_texture(0, scene_color_handle->get_internal_handle())` → `bind_texture_ptr(0, scene_color_handle)`. Trailing defensive `glBindFramebuffer(GL_FRAMEBUFFER, 0)` deleted (FBO 0 is the default; renderer sets its own FBO before any subsequent draws — same rationale as the EnvProbe::init cleanup deleted in 1.2b). Dead-code comments referencing `glNamedFramebufferTexture` / `glBlitNamedFramebuffer` / `GL_COLOR_BUFFER_BIT` removed. `glad/glad.h` include dropped.
- `DrawLocal_SceneDrawInternal.cpp` contains zero direct `gl*` calls and zero `get_internal_handle()` calls. 184 unit tests + full integration suite green.

## Sub-phase 1.5a status

- API added: `set_color_write_mask(int attachment, bool r, bool g, bool b, bool a)` — per-attachment color mask (wraps `glColorMaski`). Immediate setter; Phase 2c folds the mask onto `RenderPipelineState::color_write_masks` (see [Pipeline model](#pipeline-model)) so decal materials carry their own per-attachment write mask as part of pipeline state.
- `DecalBatcher.cpp`: 6× `glColorMaski` in `apply_decal_color_masks` → `gfx().set_color_write_mask`; trailing 6× restore loop → same. 2× `glBindBufferBase(GL_SHADER_STORAGE_BUFFER, …)` on `IGraphicsBuffer*` (`draw.buf.decal_uniforms`, `indirection_buffer`) → `bind_storage_buffer_base`. `glBindBuffer(GL_DRAW_INDIRECT_BUFFER, multidraw_commands)` → `bind_indirect_buffer`; matching unbind → `bind_indirect_buffer(nullptr)`. `glMultiDrawElementsIndirect(GL_TRIANGLES, MODEL_INDEX_TYPE_GL, …)` → `multi_draw_elements_indirect(Triangles, MODEL_INDEX_TYPE, …)`. Dropped `extern const GLenum MODEL_INDEX_TYPE_GL`.
- `DecalBatcher.cpp` contains zero direct `gl*` calls. 184 unit tests + full integration suite (80 tests) green.

## Sub-phases 1.4d–1.4g status (API drift corrections)

Folded in from the now-removed `FIXES_TO_API.md`. Four shapes had leaked into the
interface during 1.1–1.4c that wouldn't survive contact with SDL3 GPU; corrected
before resuming the migration at 1.5a.

- **1.4d** — `ColorTargetInfo` now carries `wants_clear` + `clear_color` per
  attachment (mirrors `SDL_GPUColorTargetInfo.{load_op, clear_color}`); SDL3
  port hands the struct straight to the backend without state-cache fixup.
  Removed `RenderPassState::use_gray_clear`/`wants_color_clear` and the device
  `set_clear_color` setter. Multi-attachment grey-clear in the gbuffer pass now
  loops over attachments to set the gray on each.
- **1.4e** — Migrated every remaining raw `bufferhandle` field to
  `IGraphicsBuffer*` (UBOs sized at create + per-frame `sub_upload`; per-frame
  SSBOs use `upload` re-spec for now — Phase 2e ring buffer is the real fix).
  Whole `_raw` API surface deleted. New non-raw
  `bind_storage_buffer_range(IGraphicsBuffer*, offset, size)` replaces the
  range_raw variant. `Render_Lists::init` now allocates 0-sized buffers; the
  per-frame `upload(data, size)` re-specs storage (preserves OpenGL semantics;
  flagged for Phase 2e ring-buffer migration so SDL3 GPU — which has fixed-size
  transfer destinations — doesn't trip on it).
- **1.4f** — `set_mip_range` and `download_texture[_2d]` moved off
  `IGraphicsDevice` onto `IGraphicsTexture`. `download(mip, layer, dest, size)`
  collapses the 2D/layered overloads (`layer=-1` for non-layered).
- **1.4g** — Implicit render/compute pass separation. New
  `begin_compute_pass()` flips pass mode; `set_render_pass()` flips back. SDL3
  backend will emit `End{Render,Compute}GPUPass` on transition and defer
  `BeginGPUComputePass` to the first dispatch so writable bindings are known by
  then. OpenGL backend asserts mode in `dispatch_compute` to catch missing
  begin calls. No `end_*` API — boundaries are bookkeeping only.

Decided to stay GL-shaped (backend absorbs the impedance, no API change):
`memory_barrier(GraphicsMemoryBarrierBits)` (SDL3 auto-tracks transitions →
no-op there); `bind_image_for_compute(..., GraphicsImageAccess)` (SDL3 derives
R/W from pipeline decl); split `bind_texture` + `bind_sampler` (SDL3 backend
flushes per-slot `{tex, sampler}` table at draw time as combined bindings);
`set_line_width` (SDL3 silently no-ops); synchronous `download_*` /
`wait_for_gpu_idle` (SDL3 builds the fence dance internally).

## Sub-phase 1.4c status

- API added: `multi_draw_elements_indirect(mode, index_type, const void* indirect, int count, int stride)` — indirect is byte offset into bound `GL_DRAW_INDIRECT_BUFFER`, or a CPU pointer when buffer 0 is bound (client-side MDI path under `use_client_buffer_mdi` cvar); `draw_elements_instanced_base_vertex_base_instance(...)` for the non-MDI fallback path; `draw_elements(mode, count, index_type, byte_offset)` for particle render; `set_clear_color(r,g,b,a)` over `glClearColor`; `bind_storage_buffer_range_raw(slot, handle, offset, size)` and `bind_indirect_buffer_raw(handle)` as _raw escape hatches for buffers still living as `bufferhandle`; `upload_buffer_raw(handle, size, data)` and `sub_upload_buffer_raw(handle, offset, size, data)` wrapping `glNamedBufferData`/`glNamedBufferSubData` (DYNAMIC_DRAW). `sub_upload_buffer_raw` no-ops on size==0 to match `glNamedBufferSubData` semantics — `build_cascade_cpu` hits this when a shadow cascade has zero visible objects.
- `DrawLocal_RenderPass.cpp`: `glClearColor` → `set_clear_color`; 4× `glDrawArrays(GL_TRIANGLES, 0, 3)` in bloom + particle paths → `draw_arrays`; `glDrawElementsBaseVertex` in `draw_model_simple_no_material` → `draw_elements_base_vertex`; `glDrawElements` in `render_particles` → `draw_elements`; `glDrawElementsInstancedBaseVertexBaseInstance` in `render_lists_old_way` → `draw_elements_instanced_base_vertex_base_instance`; SSBO/UBO binds in `setup_execute_render_lists` and `render_level_to_target` → `bind_storage_buffer_base`/`bind_storage_buffer_base_raw`/`bind_uniform_buffer_base_raw`; `glBindBufferRange(SSBO)` → `bind_storage_buffer_range_raw`; `glBindBuffer(GL_PARAMETER_BUFFER)` → `bind_parameter_buffer`; `glBindBuffer(GL_DRAW_INDIRECT_BUFFER, list.gpu_command_list)` → `bind_indirect_buffer_raw` (raw bufferhandle); `glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0)` → `bind_indirect_buffer(nullptr)`; `glMultiDrawElementsIndirect`/`Count` → `multi_draw_elements_indirect`/`_count`; polygon-offset enable/set/disable → `set_polygon_offset`; 6× `glNamedBufferData`/`SubData` in `build_standard_cpu` + `build_cascade_cpu` → `upload_buffer_raw`/`sub_upload_buffer_raw` (`list.gldrawid_to_submesh_material`, `list.glinstance_to_instance`, `list.gpu_command_list` are still raw `bufferhandle` on `Render_Lists` — migrate when those fields move to `IGraphicsBuffer*`).
- `MODEL_INDEX_TYPE = VertexInputIndexType::uint16` mirror added to `ModelManager.h` next to `MODEL_BUFFER_INDEX_TYPE_SIZE`; consumed by Batch + RenderPass (unity-build collision blocks per-file `static constexpr`).
- `DrawLocal_RenderPass.cpp` contains zero direct `gl*` calls. 184 unit tests + full integration suite green.

## Sub-phase 1.4b status

- API added: `bind_parameter_buffer(IGraphicsBuffer*)` over `GL_PARAMETER_BUFFER` (nullptr unbinds); `set_polygon_offset(bool enabled, float factor, float units)` over `glEnable/Disable(GL_POLYGON_OFFSET_FILL)` + `glPolygonOffset` (Phase 2c folds onto `RenderPipelineState::polygon_offset_*`); `multi_draw_elements_indirect_count(mode, index_type, indirect_byte_offset, count_byte_offset, max_draw_count, stride)` wrapping `glMultiDrawElementsIndirectCount`. Both `bind_indirect_buffer` and `bind_parameter_buffer` are deleted in 2c — the buffer pointer moves onto the indirect draw call itself (see [Pipeline model](#pipeline-model)).
- `DrawLocal_BatchScene.cpp`: 3× `glBindBufferBase(GL_SHADER_STORAGE_BUFFER, …)` on `IGraphicsBuffer*` → `bind_storage_buffer_base`; `scene.gpu_skinned_mats_buffer` (still a raw `bufferhandle` on `RenderScene`) routed via `bind_storage_buffer_base_raw`; `glBindBuffer(GL_PARAMETER_BUFFER)` → `bind_parameter_buffer`; `glBindBuffer(GL_DRAW_INDIRECT_BUFFER)` → `bind_indirect_buffer` (set + unbind); `glMultiDrawElementsIndirectCount` → `multi_draw_elements_indirect_count`; `glEnable(GL_POLYGON_OFFSET_FILL)`/`glPolygonOffset`/`glDisable` → `set_polygon_offset`.
- Local `MODEL_INDEX_TYPE = VertexInputIndexType::uint16` mirror introduced (matches the file-local `MODEL_INDEX_TYPE_GL` constant still referenced by other passes). `glad/glad.h` include dropped.
- `DrawLocal_BatchScene.cpp` contains zero direct `gl*` calls. 184 unit tests + full integration suite green.

## Sub-phase 1.4a status

- API added: `set_line_width(float)` (debug meshbuilder line width — Phase 2c folds onto `RenderPipelineState::line_width`); `bind_indirect_buffer(IGraphicsBuffer*)` over `GL_DRAW_INDIRECT_BUFFER` — `nullptr` unbinds. Phase 2c moves the indirect-buffer pointer onto the draw call (see [Pipeline model](#pipeline-model)).
- `DrawLocal_Lighting.cpp`: 4× `glBindBufferBase(GL_SHADER_STORAGE_BUFFER, …)` → `bind_storage_buffer_base` (all 4 buffers are already `IGraphicsBuffer*`); 2× `glBindBufferBase(GL_UNIFORM_BUFFER, …)` for `ubo.current_frame` / `shadowmap.ubo.info` → `bind_uniform_buffer_base_raw`; 4× `glDrawArrays(GL_TRIANGLES, 0, 3)` → `draw_arrays(Triangles, 0, 3)`; `glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0)` → `bind_indirect_buffer(nullptr)`; debug meshbuilder `glLineWidth` pair → `set_line_width`. `glad/glad.h` include dropped.
- `DrawLocal_Lighting.cpp` contains zero direct `gl*` calls. 184 unit tests + full integration suite green.

## Sub-phase 1.1 status

- API added: `set_scissor`, `disable_scissor`, `draw_elements_base_vertex`, `bind_uniform_buffer_base_raw`, `wait_for_gpu_idle`, `download_texture_2d`.
- `CheckGlErrorInternal_` body moved to `OpenGlDevice.cpp`; `DrawLocal_Debug.cpp` no longer includes `glad.h`.
- `DrawLocal_Misc.cpp` and `DrawLocal_Debug.cpp` contain zero direct `gl*` calls.

## Sub-phase 1.3b status

- API added: `IGraphicsSampler` + `create_sampler(CreateSamplerArgs)` (filter / wrap / reduction); `bind_sampler(slot, IGraphicsSampler*)` (nullptr unbinds); `clear_buffer_uint32(IGraphicsBuffer*, uint32_t)` wrapping `glClearNamedBufferData` (R32UI). `GraphicsSamplerReduction::{WeightedAverage, Min, Max}` carries the filter-minmax intent (`GL_TEXTURE_REDUCTION_MODE_ARB`); SDL3 GPU has no equivalent — Phase 3 swaps the HiZ samplers for a compute min/max pre-pass.
- `SSRSystem::hiz_max_sampler`: raw `uint32` → `IGraphicsSampler*` (LinearMipmapNearest/Linear, ClampToEdge, reduction=Max — depth in reverse-Z, max==closest). All compute-depth + ssr_compute binds route through `gfx().bind_sampler`. Trailing `glDrawArrays`/`glBindSampler` migrated.
- `GpuCullingTest::hiZSampler`: raw `uint32` → `IGraphicsSampler*` (reduction=Min — conservative far-depth for cull). `glBindBufferBase`/`glBindImageTexture`/`glDispatchCompute`/`glMemoryBarrier`/`glDrawArrays`/`glClearNamedBufferData` calls in `debug_overlay`, `do_cull`, `downsample_depth`, `compact_draws`, `zero_instances_in_this`, `build_data` migrated to the abstraction. `zero_instances_in_this` still takes a raw `bufferhandle` (called with `input.cmd_buf->get_internal_handle()`); routed via `bind_storage_buffer_base_raw`. Cleanup of the raw-handle signature deferred to 1.4 where the MDI binding lands.
- Sampler lifetime: both subsystems are global singletons that already leak (no destructors clean up shaders/buffers either); no behavioural change.
- `RenderExtra_SSR.cpp` and `GpuCullingTest.cpp` contain zero direct `gl*` calls. 184 unit tests + full integration suite green.

## Sub-phase 1.3a status

- API added: `dispatch_compute(x,y,z)`; `memory_barrier(uint32_t bits)` over the `GraphicsMemoryBarrierBits` flag enum (`BARRIER_SHADER_STORAGE`, `BARRIER_SHADER_IMAGE_ACCESS`, `BARRIER_COMMAND`, `BARRIER_TEXTURE_FETCH`); `bind_image_for_compute(slot, tex, mip, layer, access)` over `GraphicsImageAccess` (image format inferred from `tex->get_texture_format()`, `layer == -1` → layered/`GL_TRUE`); `bind_storage_buffer_base(slot, IGraphicsBuffer*)` and `bind_storage_buffer_base_raw(slot, uint32_t)` (raw escape for `draw.ubo.current_frame` + the soon-to-migrate MDI buffer).
- Backend fix: `OpenGLTextureImpl` now routes `tCubemap` and `t3D` through `glTextureStorage3D` (previously only `t2DArray` / `tCubemapArray`). `LinearClamped` sampler now sets `GL_TEXTURE_WRAP_R` (harmless for 2D, required for the volfog 3D textures).
- `Volumetric_Fog_System`: `texhandle volume/last_volume` → `IGraphicsTexture*` (t3D, rgba16f, LinearClamped); `bufferhandle param` → `IGraphicsBuffer*` (sized + `BUFFER_USE_DYNAMIC`, per-frame `sub_upload`). Lightcalc + raymarch passes now use `gfx().bind_image_for_compute` + `dispatch_compute` + `memory_barrier(BARRIER_SHADER_IMAGE_ACCESS)`. UBO/SSBO binds route through `bind_uniform_buffer_base` / `bind_storage_buffer_base` (raw variant only for `draw.ubo.current_frame`).
- `DrawLocal_SceneDraw.cpp::draw_height_fog` consumer updated: `glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, volfog.buffer.param)` → `gfx().bind_storage_buffer_base(4, volfog.buffer.param)`; `bind_texture(1, volfog.texture.volume)` (texhandle overload) → `bind_texture_ptr(1, volfog.texture.volume)`.
- Trailing `glDepthMask`/`glEnable`/`glBindVertexArray`/`glBindTexture` cleanup deleted — `device.reset_states()` (which preceded the original cleanup) already restores those.
- `Volumetricfog.cpp` contains zero direct `gl*` calls; `glad/glad.h` include dropped. All 184 unit tests + full integration suite (80 tests) green; `bake_probes` + `demo_level_1` screenshots within their soft-fail bands.

## Sub-phase 1.2b status

- API added: `set_mip_range(IGraphicsTexture*, base, max)` (specular prefilter clamps sampling to mip 0 while writing into higher mips of the same cubemap); `download_texture(tex, mip, layer, dest, dest_size_bytes)` extending `download_texture_2d` with a layer parameter for cube-face readback (uses `glGetTextureSubImage` when `layer >= 0`, `glGetTextureImage` otherwise). `rgb16f` added to the readback format map (irradiance bake).
- `compute_specular_new`: removed direct `glTextureParameteri` / `glActiveTexture` / `glBindTexture` / trailing depth+cull resets. Now uses `gfx().set_mip_range` + `gfx().bind_texture(0, cubemap)` + `gfx().draw_arrays`. Existing per-face `RenderPassState` with `ColorTargetInfo(tex, face, mip)` continues to work via the shared FBO's `glNamedFramebufferTextureLayer` path.
- `compute_irradiance_new`: temp 16×16 RGB16F cubemap created via `gfx().create_texture(tCubemap, CubemapDefault sampler)` (replaces raw `glCreateTextures` + storage + parameter setup); per-face FBO attachment replaced with `gfx().set_render_pass`; per-face `glReadPixels` replaced with `gfx().download_texture(temp_tex, 0, face, ...)`; `glFlush+glFinish` → `gfx().wait_for_gpu_idle()`; temp texture destroyed via `safe_release`.
- Defensive `glBindFramebuffer(GL_FRAMEBUFFER, 0)` at end of `init()` deleted (FBO 0 is the default; the renderer sets its own FBO before any draws).
- BRDFIntegration (`BRDFIntegration::run` + `drawdebug`) intentionally left at raw GL — shader-coupled (raw `Shader` + raw `lut_id` consumed by SSR, RT, RenderPass passes). Migrates with `Shader.cpp` in sub-phase 1.7.
- `EnvProbe.cpp` outside BRDFIntegration contains zero direct `gl*` calls. `editor/bake_probes_test` passes within the screenshot warn band (3 pixels diff at delta≤80, golden unchanged).

## Sub-phase 1.2a status

- API added: `draw_arrays`, `bind_texture(IGraphicsTexture*)`, `bind_uniform_buffer_base(IGraphicsBuffer*)`.
- `SSAO_System` struct: all five raw `fbohandle`s deleted; `texhandle random/depthlinear/deptharray/resultarray` → `IGraphicsTexture*`; `bufferhandle data` → `IGraphicsBuffer*` (`BUFFER_USE_DYNAMIC`).
- All per-frame FBO/attachment management replaced with `gfx().set_render_pass(RenderPassState)` using `ColorTargetInfo(tex, layer, mip)` — the array-layer-per-attachment pattern in deinterleave works through `glNamedFramebufferTextureLayer` inside the backend.
- The finalresolve pass that swapped `glNamedFramebufferDrawBuffer` between `ATT0`/`ATT1` is now three single-attachment passes (reinterleave→result, blur-H→blurred, blur-V→result).
- Backend correctness fix: `OpenGLTextureImpl` now returns `GL_SHORT` (not `GL_FLOAT`) input type for `rgba16_snorm`, and supplies `GL_RGBA` as the input format. `is_float_type()` no longer claims SNORM is float. Required to upload the HBAO random rotation texture via `IGraphicsTexture::sub_image_upload`.
- `Ssao.cpp` contains zero direct `gl*` calls (only `glCheckError()` macro → backend `CheckGlErrorInternal_`).

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

### 2a pilot status

Migrated `BloomDownsampleF` + `BloomUpsampleF`. New struct defs +
`BLOOM_PARAMS_UBO_BINDING = 7` live in `DrawLocal_RenderPass.cpp`
(anonymous namespace) alongside `render_bloom_chain`. Per-pass UBO storage
(`ubo.bloom_downsample_params`, `ubo.bloom_upsample_params`) is created in
`Renderer::Init`. Program-binary cache invalidates automatically (shader
source touched → cached binary stale by timestamp). 185 unit tests + 80
integration tests green.

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

### 2a status — done (group migrations) + dead-shader sweep

All 10 listed groups landed on `refactor/gfx-abstraction`. The actual
order differed from the planned sweep: B (pilot relock) → C → G → K → F →
D → J → A → E → H. Each group is one commit, building on the
canonical pattern (struct in `Shaders/ShaderBufferShared.txt`, nested
UBO block at slot 7, direct `params.X` access in shader bodies).

Group-specific deviations worth knowing:

- **G (Volfog)** uses the existing volfog UBO at slot 4, not slot 7 — its
  data was already in a UBO; migration just folded the lone
  `set_int("num_lights")` into the same struct and moved that struct
  into the shared header.
- **K (Raytrace debug)** had no live shaders — all three programs were
  created but never dispatched. Closed by deletion rather than migration.
- **H (SSAO/HBAO)** was the originally-skipped group; relanded later
  with the full SSAO/HBAO pipeline (5 shaders, not the 2 in the doc
  plan — `BilateralBlurF` was dead and the rest of HBAO needed to come
  along). See [[#2a follow-up]] below for the `#define` shadow-class
  gotcha that broke `linearizedepth.txt` first time round.
- **D (SSR)** skipped `ssr_apply_upsampled` (never created) and
  `blur_ssr` (created but dispatch sits past an unconditional `return`).
- **E (DDGI)** refactored `set_reflection_uniforms()` and
  `draw_lighting_shared()` to fill a passed-in `gpu::DdgiRuntimeParams&`
  instead of calling `set_*(name)`. `reflectionShared.txt` is now a
  helper-include that relies on the including shader having bound
  `DdgiRuntimeParamsUbo` at slot 7.

Dead-shader sweep (separate commit `3cd5bad9`) removed 7 unreachable
shaders + their program slots + the unreachable dispatch tail in
`do_upsample`: `cpu_vis_to_mdi`, `ssr_apply_upsampled`,
`bilateral_upsample_ddgi`, `ddgiShadeDebugF`, `get_best_cubemap_C`,
`blur_ssr`, `SampleCubemapsF`.

**What 2a does NOT close:** the endgame milestone ("delete the name-based
setter methods from IGraphicsShader") still has ~25 call-sites blocking
it — the PER-DRAW row of the group table is untouched (mesh-builder,
debug-texture display, EnvProbe cubemap bake, decal indirect, cookie
atlas, DDGI debug-probe vis). Per the original doc, those wait for the
push-constants design.

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

Landed across four sequential commits (see Sub-phase 2c status below).

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
| **B3** | Per-frame ring buffer impl for `BUFFER_USE_DYNAMIC` (2e — decision made, impl not done). `IGraphicsBuffer::sub_upload` on DYNAMIC routes through N×size ring, head advances on `begin_frame()`. OpenGL keeps `glNamedBufferSubData`; teeth on SDL3 transfer-buffer path. | **deferred to SDL3 backend** — OpenGL path (`glNamedBufferSubData`) is a no-op on GL, all teeth are on the SDL3 transfer-buffer side. Implement inside the SDL3 backend during Phase 3 rather than as a prereq. |
| **B4** | `IGraphicsDevice::push_debug_group(const char*) / pop_debug_group()`. SDL3 has `SDL_PushGPUDebugGroup` but no timestamp queries — Profiler GPU-timing stubs on SDL3, debug groups stay live. Drops `Framework/Profilier.cpp` + `IntegrationTests/GpuTimer.{h,cpp}` off the static-scanner accept-list. | not started |
| **B5** | SDL2 → SDL3 windowing + `imgui_impl_sdl2` → `imgui_impl_sdl3`. Independent of B1–B4 — parallel work stream. Required before Phase 3. | not started |

**Push-constant model (B1):** slot-indexed block uploads, mirroring `SDL_PushGPU{Vertex,Fragment,Compute}UniformData(cmdbuf, slot, data, size)`. The interface is block-shaped (no name-based scalar setter on top). Per-pass group UBOs at slot 7 stay UBOs — push constants are for *per-draw* data only. 21 callsites all per-draw / per-instance, so the migration target is push-const not slot-7 UBO.

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

## Phase 3.5 — Tessellation terrain replacement (planned)

SDL3 GPU has no tess shaders. Adaptive view-dependent factors in
`MaterialLocal_Shader.cpp:81` (`Program_Manager::create_single_file(...,
is_tesselation=true)`) → per-frame compute pre-pass writing displaced
vertex/index buffers, then indirect draw with a regular vertex shader. Sized
to worst-case LOD. Treat as a sub-project; only required if any live master
shader uses `is_tesselation=true` at SDL3-port-time. Verify before shipping.

## Out of scope

- WebGPU / Emscripten target. SDL3 GPU doesn't target WebGPU. If web becomes
  real later, that's a second backend against this same interface with stricter
  constraints (no SSBO baseline, limited compute). Worth pressure-testing the
  Phase 2 API against WebGPU's binding model on paper before locking it.
- DX12 / Metal / mobile Vulkan — implicitly covered by SDL3 GPU's backends.
- SDL2 → SDL3 windowing + ImGui platform layer migration — required before
  Phase 3 but a separate parallel work stream (B5).

### Footnote — D3D12 enablement (out of scope, future P3.9)

SPIR-V is Vulkan-only. SDL3 GPU on Windows picks backend from the shader
formats supplied to `SDL_CreateGPUDevice`: `SDL_GPU_SHADERFORMAT_SPIRV` →
Vulkan; `SDL_GPU_SHADERFORMAT_DXIL` → D3D12 (signed DXIL); `SDL_GPU_SHADERFORMAT_MSL` → Metal.
Phase 3 ships SPIR-V only → SDL3 runs on its Vulkan backend on Windows.
D3D12 enablement later via `SDL_shadercross` (separate SDL helper lib that
translates SPIR-V → DXIL/MSL at runtime in dev, prebake DXIL for release).
Not a Phase 3 blocker.
