# Graphics Device Abstraction

Migration of all rendering behind `IGraphicsDevice` (Source/Render/IGraphicsDevice.h) so an SDL3 GPU backend can sit alongside the current OpenGL backend.

Global accessor: `gfx()` (free function). The OpenGL singleton is built by `gfx_init_opengl()` during `Renderer::init` and torn down by `gfx_shutdown()`.

## Phase plan

1. **Wrap.** Every `gl*` call moves behind `IGraphicsDevice`. API stays GL-shaped — by-name uniforms, apply-at-draw pipeline state, immediate draws. Goal: zero `gl*` calls outside `Source/Render/Opengl*`. No behavior changes.
2. **Redesign.** With everything funneled through the interface, reshape the API on the OpenGL backend only — synthetic UBOs, baked pipeline objects, command encoder, frame lifecycle, transfer ring buffers.
3. **Port.** Add the SDL3 GPU backend against the redesigned interface. GLSL → SPIR-V via glslang/shaderc at runtime (dev) or prebaked blobs (release).

A **null/passthrough leak-detector backend** lands at the end of Phase 1 (1.5) as the gate that proves the wrap is complete.

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
| `state.vao = vao_ptr->get_internal_handle()` — RenderPass, BatchScene, EnvProbe, RT_Shade | ~4 | Pipeline-state already takes a `vertexarrayhandle`; in Phase 2c `bind_pipeline(IGraphicsRasterPipeline*)` swallows it. For Phase 1 wrap: change `RenderPipelineState::vao` to `IGraphicsVertexInput*` and resolve inside the backend. |
| `RenderDump.cpp` integration test — pulls handle for `glReadPixels`            |   1   | Test-side. Move to `gfx().download_texture` once available.                       |
| Commented-out callers                                                           |   ~9  | Delete during wrap.                                                               |

**Risk.** All identified sites are routable through Phase 1 wrap additions. None require a redesign-pass extension. Plan estimate of "17 known" was low — actual count is ~75 raw calls across ~25 files, but they cluster into the ~12 API additions listed above.

The null/passthrough backend (Phase 1.5) will assert that every `get_internal_handle()` call comes from inside `Source/Render/Opengl*` or sits on an explicit accept-list.

## Phase 1 sub-phases

Total `gl*` site count is ~800 across ~25 files. Phase 1 lands as discrete sub-phases — each builds, tests, and is committed independently so regressions can be bisected to one subsystem.

| Sub-phase | Scope | gl-call count | API additions | Status |
| --------- | ----- | -------------:| ------------- | ------ |
| **1.1** | `DrawLocal_Misc.cpp`, `DrawLocal_Debug.cpp` | ~14 | scissor, draw_elements_base_vertex, bind_uniform_buffer_base_raw, wait_for_gpu_idle, download_texture; move `CheckGlErrorInternal_` to backend | done |
| **1.2a** | `Ssao.cpp` | ~100 | draw_arrays, bind_texture, bind_uniform_buffer_base; migrate raw FBOs to `gfx().set_render_pass`; migrate raw textures + UBO to `IGraphicsTexture` / `IGraphicsBuffer`; fix `rgba16_snorm` input type in backend | done |
| 1.2b | `EnvProbe.cpp` | ~106 | cubemap mip clamping, temp render target, `glReadPixels`, BRDFIntegration (defer shader-coupled lines to 1.7) | done |
| **1.3a** | `Volumetricfog.cpp` | ~33 | dispatch_compute, memory_barrier (flag enum), bind_image_for_compute, bind_storage_buffer_base (+ `_raw`); t3D path in backend; add WRAP_R to LinearClamped sampler | done |
| 1.3b | `RenderExtra_SSR.cpp`, `GpuCullingTest.cpp` | ~52 | `IGraphicsSampler` + bind_sampler, glClearNamedBufferData wrapper | not started |
| 1.3c | `RT/RaytraceTest_Probe.cpp`, `RT/RaytraceTest_Shade.cpp` | ~45 | buffer map/unmap for readback (probe relocation, invalid-count) | not started |
| 1.4 | `DrawLocal_BatchScene.cpp`, `DrawLocal_Lighting.cpp`, `DrawLocal_RenderPass.cpp` | ~70 | bind_indirect_buffer, bind_parameter_buffer, multi_draw_elements_indirect (count), color-mask state | not started |
| 1.5a | `DecalBatcher.cpp` | ~12 | per-attachment color masks as immediate setters (baked in 2c) | not started |
| 1.6 | `DrawLocal_SceneDrawInternal.cpp` (orchestration) | ~14 | leftover binding + draw glue | not started |
| 1.7 | `Shader.cpp` migration → inside `Source/Render/Opengl*` | 71 | shader compile/link entirely backend-internal; expose `IGraphicsShader` + `create_shader(...)` | not started |
| 1.8 | Window/swapchain + ImGui wrap | ~30 (`SDL_GL_*`, `gladLoad*`, swap, vsync, imgui_render) | factory move from `EngineMain_Init.cpp`; `set_vsync`, `present`, `imgui_render` | not started |
| 1.9 | `r.gpu.no_legacy_calls=1` assertion test + rip per-subsystem `r.gfx.wrap.*` flags | — | gates phase boundary | not started |
| **1.5 (post-1.x)** | Null/passthrough leak-detector backend | — | gate that proves the wrap is complete | not started |

Migration rule per sub-phase: any GL call still needed by a non-backend caller becomes an `IGraphicsDevice` method (with a `_raw` suffix when the parameter is still a `bufferhandle`/`vertexarrayhandle`; those raw escape hatches disappear in Phase 2 once the corresponding resources route through `IGraphicsBuffer*` / `IGraphicsVertexInput*`).

## Sub-phase 1.1 status

- API added: `set_scissor`, `disable_scissor`, `draw_elements_base_vertex`, `bind_uniform_buffer_base_raw`, `wait_for_gpu_idle`, `download_texture_2d`.
- `CheckGlErrorInternal_` body moved to `OpenGlDevice.cpp`; `DrawLocal_Debug.cpp` no longer includes `glad.h`.
- `DrawLocal_Misc.cpp` and `DrawLocal_Debug.cpp` contain zero direct `gl*` calls.

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
