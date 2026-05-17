# SDL3 GPU Backend — Next Steps

Operational sequence for landing the SDL3 GPU backend now that Phase 2
prerequisites (B1–B5) are done. Companion to [[gfx_abstraction]] — that file
is the canonical plan; this one is the **ordered punch list**.

Critical path:
**P3.1 → P3.1.5 → P3.3 → resource impls → P3.2 + P3.4 (parallel) → P3.5 → P3.6**

Two pre-flight cleanups can land any time (independent of the critical path).

## Pre-flight cleanups (independent, do anytime)

### C1 — Delete dead geometry-shader path
- `Program_Manager::create_raster_geo` (`DrawLocal_Device.{h,cpp}`) has zero callers (`rg "create_raster_geo\("`).
- Delete `create_raster_geo` + `program_def::geo` field + the `!def.geo.empty()` branch in `recompile_do` (`DrawLocal_Device.cpp:94`).
- Delete `IGraphicsDevice::create_shader_vert_frag_geo` + `opengl_create_shader_vert_frag_geo` + `compile_vert_frag_geo` in `OpenGlShaderImpl.cpp`.
- SDL3 GPU has no geometry-shader stage — keeping the API surface alive would force a stub that asserts. Easier to delete now.
- **Why now:** shrinks the SDL3 backend's `IGraphicsShader` factory by one method and removes a follow-up footgun.

### C2 — Doc cleanup
- Delete the **B3** row from the "Phase 2 prerequisites" QUESTIONS block in [[gfx_abstraction#Phase 2 prerequisites (open) — must land before Phase 3]]. B3 (per-frame ring buffer for `BUFFER_USE_DYNAMIC`) folds into the SDL3 transfer-buffer ring inside `SDL3GpuBufferImpl.cpp` — it is **not** a blocking prereq.
- Add a P3.1.5 row to the Phase 3 table (see below).

## P3.1 — Toolchain spike (IN FLIGHT, separate agent)

Owned by another agent. Scope per [[gfx_abstraction#Phase 3 — SDL3 GPU backend port (planned)]]:

- `vcpkg install glslang spirv-cross`; `<VcpkgApplocalDeps>true</>` for auto-copy.
- Lift source-loader from `OpenGlShaderImpl.cpp` anon namespace to shared `Source/Render/ShaderSourceLoader.{h,cpp}`.
- New `Source/Render/SpirvCompile.{h,cpp}`:
  - `glslang::InitializeProcess` → `TShader.parse` → `glslang::GlslangToSpv`.
  - SPIRV-Cross reflection extracting `{num_samplers, num_storage_textures, num_storage_buffers, num_uniform_buffers}` per stage.
- Test: 5 shaders (lit master, bloom, cull, vfog, raytrace) round-trip GLSL → SPIR-V → SPIRV-Cross reflection. Reflection counts must match `glGetProgramInterfaceiv` on the OpenGL build.
- **No backend code yet.**

Downstream depends on `ShaderSourceLoader` + `SpirvCompile` being callable from `Source/Render/SDL3Gpu/`.

## P3.1.5a — Explicit layout(location=N) sweep (DONE)

Landed: `Scripts/assign_shader_locations.py` regenerates explicit
`layout(location=N)` on every stage varying and fragment color attachment
across `Shaders/`. Driven by `Scripts/shader_links.json`. Idempotent.
48 files rewritten; GL build remains green (185/185). Run the script
again whenever a new V/F pair lands.

## P3.1.5 — Backend skeleton + bring-up (NEW, not yet in doc)

Goal: SDL3 GPU device is initialised and clears the swapchain. No draws. This is the **harness everything else plugs into.**

1. **Create `Source/Render/SDL3Gpu/` TUs** (mirror of OpenGl layout):
   - `SDL3GpuDevice.cpp` — `gfx_init_sdl3gpu` + `IGraphicsDevice` impl (all virtuals `ASSERT(false)` stubs initially).
   - `SDL3GpuShaderImpl.cpp` — `IGraphicsShader` impl (stub).
   - `SDL3GpuBufferImpl.cpp` — `IGraphicsBuffer` impl (stub).
   - `SDL3GpuTextureImpl.cpp` — `IGraphicsTexture` impl (stub).
   - `SDL3GpuLocal.h` — pipeline-hash cache, internal helpers.
2. **Backend selection.** Cvar or compile flag — TBD; cvar is more flexible for parity testing. `EngineMain_Init` routes to `gfx_init_sdl3gpu(SDL_Window*)` when the flag is set, `gfx_init_opengl` otherwise.
3. **Device + swapchain bring-up:**
   - `SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, debug=true_in_dev, name=nullptr)`.
   - `SDL_ClaimWindowForGPUDevice(device, window)`.
   - Query swapchain format via `SDL_GetGPUSwapchainTextureFormat`.
   - Implement `begin_frame` → `SDL_AcquireGPUCommandBuffer` (just enough to test).
   - Implement `acquire_swapchain_texture` → `SDL_WaitAndAcquireGPUSwapchainTexture` lazy.
   - Implement `submit_and_present` → `SDL_SubmitGPUCommandBuffer`.
   - Implement `set_render_pass` with **clear-only** support (one color target, clear color, no draws). `SDL_BeginGPURenderPass` + immediate `SDL_EndGPURenderPass`.
4. **Verify:** engine boots with the cvar set, screen clears to a known color each frame, no crash on `submit_and_present`, ImGui (B5's `imgui_impl_sdl3`) hidden until P3.4 wires bindings. All other `IGraphicsDevice` calls assert.
5. **Static scanner:** add `Source/Render/SDL3Gpu/` to the SDL_GPU accept-list; assert SDL_GPU calls outside that dir stay zero.

## P3.3 — Frame skeleton extension

Already partially landed in P3.1.5 above. P3.3 finishes it:
- `set_render_pass` translates full `std::span<ColorTargetInfo>` → `SDL_GPUColorTargetInfo[]`:
  - `load_op = CLEAR if wants_clear else LOAD`, `store_op = STORE`.
  - Depth target → `SDL_GPUDepthStencilTargetInfo` with `wants_depth_clear` / `clear_depth_val`.
- `begin_compute_pass` defers `SDL_BeginGPUComputePass` to first dispatch (writable bindings must be known at pass-begin in SDL3).
- PassMode tracker (already in OpenGL backend from 1.4g) ports as-is.
- End-pass-on-new-pass logic — `set_render_pass` ends any active pass first.

## Resource impls (sequential, all required before P3.2 is testable)

### `SDL3GpuShaderImpl.cpp`
- `create_shader_vert_frag(vert, frag, defines)` → load both via `ShaderSourceLoader` → `SpirvCompile` per stage → `SDL_CreateGPUShader` per stage with the `PerStageCounts` from SPIRV-Cross reflection.
- `create_shader_compute` → same, single stage.
- `create_shader_single_file` → split via existing `#ifdef _VERTEX_/_FRAGMENT_` macro convention (see `OpenGlShaderImpl.cpp:make_shader`), then compile both.
- `reflect()` populates `Reflection` from cached SPIRV-Cross output.
- **Skip:** `create_shader_vert_frag_geo` (deleted in C1), `create_shader_single_file_tess` (out of scope — assert).
- **Skip:** program-binary cache. SDL3 has its own pipeline cache; a source-side SPIR-V cache is a possible micro-opt, not required.
- `Program_Manager::recompile_all` hot-reload works via the same factory path.

### `SDL3GpuBufferImpl.cpp` (folds in B3)
- `SDL_CreateGPUBuffer` for static/storage buffers.
- **Transfer-buffer ring** for `BUFFER_USE_DYNAMIC` — N×size ring (N = max frames in flight, target 3). `sub_upload` allocates a slot, `SDL_MapGPUTransferBuffer` → memcpy → `SDL_UnmapGPUTransferBuffer` → `SDL_UploadToGPUBuffer` in current command buffer. Head advances on `begin_frame()`.
- Static upload (immutable buffers): one-shot transfer buffer at create time.
- Lifetime: on `release()`, call the OpenGL-backend-equivalent `sdl3gpu_backend_buffer_released` to clear cached indirect/parameter slots.

### `SDL3GpuTextureImpl.cpp` + samplers
- `SDL_CreateGPUTexture` from `CreateTextureArgs`. Format translation table.
- `SDL_CreateGPUSampler` from `CreateSamplerArgs`. **Skip `Min`/`Max` reduction** — covered by P3.6 HiZ rewrite.
- `IGraphicsTexture::download` for the debug/test path → `SDL_DownloadFromGPUTexture` + transfer buffer.
- `copy_texture` / `generate_mipmaps` — `SDL_BlitGPUTexture` for copies; mipmap gen probably needs a compute pre-pass (verify).

### `SDL3GpuVertexInput`
- `CreateVertexInputArgs` → cached vertex layout description usable in pipeline create. No SDL3 VAO equivalent — the description is consumed at pipeline-create time, not bind time.

## P3.2 — Pipeline cache (parallelizable with P3.4)

- Inside `SDL3GpuDevice::set_pipeline`: `unordered_map<uint64_t, SDL_GPUGraphicsPipeline*>` keyed by `XXH3_64bits(&RenderPipelineState, sizeof(...))`.
- Miss → `SDL_CreateGPUGraphicsPipeline`:
  - rasterizer ← `fill_mode + cull + front_face + polygon_offset_*`
  - depth-stencil ← `depth_test + depth_write + depth_func`
  - per-attachment blend ← existing `blend_state` fields
  - color/depth target formats ← current pass's attachment formats. **Therefore `set_pipeline` must run inside a render pass** (already enforced by PassMode).
  - vertex input ← `IGraphicsVertexInput*` description
- Invalidate cache entries on `Program_Manager::recompile_all` (walk map, free entries whose shader pointer was just released).
- Hash padding hazard: `RenderPipelineState` must be POD with no padding holes. Add `static_assert(std::has_unique_object_representations_v<RenderPipelineState>)` or zero-init the struct in its ctor.

## P3.4 — Bind-table flushing (parallelizable with P3.2)

- Per-stage tables in `SDL3GpuDevice`:
  - `vert_samplers[N] = {tex, samp}`, `frag_samplers[N]`
  - `vert/frag/compute_uniform_buffers[N]`
  - `vert/frag/compute_storage_buffers[N]`
  - `vert/frag/compute_storage_textures[N]`
- `bind_texture` / `bind_sampler` / `bind_*_buffer_*` write the tables (no immediate SDL3 call).
- Draw / dispatch flushes dirty tables via `SDL_BindGPU{Vertex,Fragment,Compute}{Samplers,UniformBuffers,StorageBuffers,StorageTextures}`.
- Vertex buffers from `IGraphicsVertexInput` layout via `SDL_BindGPUVertexBuffers`.
- Per-draw push constants via `SDL_PushGPU{Vertex,Fragment,Compute}UniformData(slot, data, size)` — B1's slot-indexed API already matches.

**Verification milestone after P3.2 + P3.4:** single triangle + fullscreen-quad test renders on both backends. `test_shader_reflection.cpp` runs on SDL3 — counts must match GL.

## P3.5 — Y-flip + winding

- SDL3 NDC is +Y down (vs GL's +Y up). Choose:
  - Flip projection matrix row 1 inside SDL3 view-constant upload (preferred — single site).
  - Swap front-face winding default in `set_pipeline` (CCW ↔ CW).
- Audit `gl_FragCoord.y` / screen-UV-from-NDC consumers in the post-process stack:
  - TAA resolve
  - Bloom composite
  - SSR apply
- Visual diff catches regressions via `bake_probes_test` + `demo_level_1_shots`.

## P3.6 — HiZ filter-minmax replacement

- `GraphicsSamplerReduction::{Min, Max}` has no SDL3 equivalent.
- New `Data/Shaders/HiZReduce.glsl` compute pass building min/max reduced mip chains.
- Consumers: `SSRSystem::hiz_max_sampler`, `GpuCullingTest::hiZSampler`.

## Verification gate (run after P3.5/P3.6)

- `test_pipeline_cache.cpp` — bike demo ≤200 unique pipelines, ≤16 decal variants.
- `test_sdl3_parity.cpp` — same scene both backends, SSIM ≥ 0.98 per pass (G-buffer MRT, shadow cascades, deferred lighting, decal blend, SSAO, SSR, TAA, bloom, post-composite).
- `test_dynamic_buffer_ring.cpp` — 3 frames in flight, no slot overwrite.
- `test_shader_reflection.cpp` — 5 representative shaders, SDL3 reflection counts match GL.
- `bake_probes_test`, `demo_level_1_shots` — within soft-fail bands on both backends.
- Static scanner stays green — every `SDL_GPU*` call lives inside `Source/Render/SDL3Gpu/`.

## Out of scope (defer)

- **Tessellation** — `create_shader_single_file_tess`. SDL3 GPU has no tess stage. Skip per project decision; assert if any master shader uses `is_tesselation=true` at port time.
- **DX12 / DXIL** — covered by future P3.9 footnote in [[gfx_abstraction]]. Phase 3 ships SPIR-V → SDL3 Vulkan backend on Windows.
- **WebGPU / Emscripten** — separate backend, not this work.
- **Wireframe `set_polygon_fill_mode(Line)`** — re-expressed via a different debug path when SDL3 lands; not blocking initial parity.
