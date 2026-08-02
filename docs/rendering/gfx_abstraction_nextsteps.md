# DX11 Backend — Status / Punch List

M1 of [[gfx_abstraction]]: a second `IGraphicsDevice` backend
(`Source/Render/Dx11/`), cvar-selected alongside OpenGL
(`r.render_backend dx11`). Supersedes the earlier SDL3-GPU plan in this file
(SDL3 stayed windowing/imgui-platform only; see [[gfx_abstraction]] B5).

Clip space already matches between GL (`glClipControl(LOWER_LEFT,
ZERO_TO_ONE)` + reverse-Z) and DX11 — no projection remap. NDC +Y up in both.
Toolchain: GLSL -> SPIR-V (glslang, shared with GL) -> HLSL (spirv-cross
CompilerHLSL, SM5.0) -> DXBC (`D3DCompile`).

## D-phase status

- **D0 — toolchain** (done): `Source/Render/SpirvCompile.{h,cpp}` +
  `spirv_to_hlsl` / `compile_hlsl_to_dxbc`. `HlslResourceBinding` table maps
  `spirv_binding -> register_index` per `t/s/b/u` register kind.
- **D1 — skeleton + swapchain clear** (done): `Source/Render/Dx11/*`,
  `GraphicsDeviceType::Dx11`, `gfx_init_dx11` (`D3D11CreateDeviceAndSwapChain`,
  FL11_1).
- **D2 — resource impls** (done): shaders (disk-cached `.dx11bin` DXBC +
  binding tables), buffers (incl. raw SRV/UAV ranges for storage/indirect),
  textures+samplers (incl. rgb->rgba widen), vertex input.
- **D3 — pipeline state, binds, draws** (done): `Dx11StateCache`
  (rasterizer/depth-stencil/blend/input-layout, cached), bind-table flush at
  draw/dispatch, push constants -> dynamic cbuffers, all `draw_elements*`
  variants. `multi_draw_elements_indirect` is a CPU loop over
  `DrawIndexedInstancedIndirect` (matches the M0 `r_indirect_loop=1` path —
  DX11 has no native MultiDrawIndirect). `multi_draw_elements_indirect_count`
  and scaled `blit_textures` remain `DX11_STUB`/`ASSERT` (not exercised by
  any current call site; all `blit_textures` calls are 1:1 ->
  `CopySubresourceRegion`).
- **D4 — origin/winding audit** (done): `set_viewport`/`set_scissor` take
  GL-convention bottom-left-origin (x,y) (see `IGraphicsDevice.h`); flipped to
  DX11's top-left-origin via `current_rt_height` (set by `set_render_pass`).
  `gl_FragCoord`-based UV derivation (TAA, bloom, SSR, hbao, ddgi, etc.) is
  self-consistent per-backend — no shader changes needed. Rasterizer
  `FrontCounterClockwise = TRUE` matches GL's CCW default (NDC +Y-up
  unchanged, no clip-space flip).
- **D5 — imgui + tail** (done): `imgui_impl_dx11` (vendored into
  `Source/External/`, paired with `ImGui_ImplSDL3_InitForD3D`); debug groups
  via `ID3DUserDefinedAnnotation`; timer queries via `ID3D11Query`
  `TIMESTAMP`+`TIMESTAMP_DISJOINT` (each `IGraphicsTimerQuery` instance
  brackets its own disjoint query); `set_vsync` -> `Present(sync, 0)`.
- **D6 — docs/tests** (this doc + scanner + toolchain test): static scanner
  `Scripts/check_no_dx11_leaks.py` — `ID3D11*`/`D3DCompile*` accept-listed
  only under `Source/External/`, `Source/Render/Dx11/`, and
  `Source/Render/SpirvCompile.{h,cpp}` (shared toolchain, D0). Run via
  `py Scripts/check_no_dx11_leaks.py`.
  `test_spirv_pipeline.cpp` gained `renderer/spirv_hlsl_dxbc`, exercising the
  HLSL->DXBC leg end to end.

## D7+ — post-doc regressions found resuming this work (in progress)

DX11 hadn't been exercised since the D7-follow-up commits (`9bd9768d`,
`d3357d8d`); RmlUi integration landed on top afterwards as OpenGL-only
(`2d80ce00` et al.) without gating DX11. Full-game-pass boot
(`-Backend dx11`) was broken from engine init onward. Fixed so far:

- **RmlUi crash on DX11 boot**: `RmlUiSystem::init()` unconditionally
  asserted `Rml::CreateContext()` succeeded, but DX11's `rmlui_init()` is an
  intentional no-op (RmlUi render interface is OpenGL-only, see
  `Dx11Device.cpp`) so `Rml::SetRenderInterface` is never called and
  `CreateContext` legitimately returns null. `RmlUiSystem::init/update/
  load_document` now null-check `context` and no-op instead of asserting —
  matches the pattern every other method on the class already used.
- **`Dx11Texture::sub_image_upload_3d` array-slice bug**: for 2DArray/
  cubemap/cubemap-array textures, `z` (the array slice) was only applied to
  the upload box's depth range (`box.front/back`), while the subresource
  index was hardcoded to array-slice 0 via `D3D11CalcSubresource(level, 0,
  mips)` — every slice past 0 uploaded to the wrong subresource with an
  invalid box. Fixed: array-shaped textures route `z` into
  `D3D11CalcSubresource(level, z, mips)` and keep the box at depth [0,1];
  true `t3D` volumes keep `z` on the box as before. Also ported the
  existing 4x4 BC-alignment / `generates_mips` whole-subresource fallback
  from `sub_image_upload` into this path (was missing, tripped on small
  cubemap mips).
- **`D3D11_RESOURCE_MISC_GENERATE_MIPS` misapplied**: was gated on
  `mips == 1` at texture-creation time, but the one real caller
  (`TextureUpload.cpp::make_from_data`) allocates the full computed mip
  chain up front and calls `generate_mipmaps()` after uploading mip 0 —
  legal in D3D11 (the flag doesn't require `MipLevels == 1`) but never hit
  the `mips==1` gate, so `GenerateMips()` ran on a resource created without
  the flag. Replaced the inference with an explicit
  `CreateTextureArgs::runtime_generate_mips` opt-in (GL ignores it).

**Not yet fixed** (surfaced by the same `-Backend dx11 renderer/*` run,
first test only — `compact_instances_smoke`):
- `ID3D11Device::CreateInputLayout`: a `R16G16B16A16_UINT` vertex element at
  byte offset 26 violates DX11's 4-byte-alignment requirement for that
  format (GL has no such constraint) — likely the compact-instance packed
  format (`CompactInstancePack`, 24-byte record). Needs either padding the
  shared layout or a DX11-specific split into aligned elements.
- Several shader/pipeline binding-table mismatches during `Draw`: SRV
  dimension `TEXTURE2D` bound where the shader/pipeline expects `BUFFER`
  (and vice versa) at multiple slots; a comparison-filtering sampler
  expected at slot 0 with none bound (shadow sampling); repeated
  `Vertex Shader - Pixel Shader linkage error: SV_Position ... mismatched
  hardware registers` — likely an spirv-cross HLSL codegen issue for
  specific shaders, not yet isolated to one shader/pass. Full DX11 game-pass
  boot doesn't yet reach a clean run of `renderer/*`; needs one bug at a
  time, ideally isolated per shader/pass via `-Pattern` before attempting
  `test_dx11_parity.cpp` below.

## Remaining / out of scope for M1

- **`test_dx11_parity.cpp`** (M1 gate, not yet written): port of
  `test_sdl3_parity` shape — same scene on both backends, SSIM >= 0.98 per
  pass (G-buffer MRT, shadows, deferred, decals, SSAO, SSR, TAA, bloom,
  composite). Blocked on the D7+ issues above — a real diff test needs the
  game pass to actually complete on DX11 first.
- **Scaled `blit_textures`**: shader blit pass, deferred until a call site
  needs it (currently all 1:1).
- **`multi_draw_elements_indirect_count`**: `DX11_STUB`, no GPU-driven
  count-buffer path on DX11 (would need a CPU readback or compute-side draw
  count clamp).
- **`create_shader_vert_frag_geo` / `create_shader_single_file_tess`**:
  `DX11_STUB`, same as GL's geometry/tess paths (out of scope, see C1 in
  history below).
- **DX12 (M2)**: reuses SPIR-V->HLSL pipeline (swap `D3DCompile`->DXC for
  DXIL/SM6.0), format tables, `RenderPipelineState`. New work: command
  list/fence frame model, descriptor heaps, per-resource state tracking +
  barriers, `ExecuteIndirect`.

## Verification

- `Scripts/integration_test.ps1 -Mode game -Pattern "renderer/*"` — GL path,
  must stay green (current: 16/16 incl. `spirv_hlsl_dxbc`).
- DX11 boot test: temporarily set `r.render_backend dx11` in `EngineVars.ini`
  `[app]`, run `App.exe`, confirm `DX11 device created...` log line, no
  `[CRASH]`/dump, revert `EngineVars.ini`.
- `py Scripts/check_no_dx11_leaks.py` must exit 0.

---

## History (pre-M1, SDL3-GPU-era — superseded)

The original plan in this file targeted an SDL3 GPU backend (Vulkan on
Windows). That direction was replaced by the DX11 plan above; SDL3 stayed
windowing/imgui-platform only (B5, done). The pre-flight cleanups below still
landed and remain accurate:

- **C1 — dead geometry-shader path**: `create_raster_geo`,
  `program_def::geo`, `create_shader_vert_frag_geo` /
  `opengl_create_shader_vert_frag_geo` deleted from the GL backend (geometry
  shaders out of scope for both DX11 and the old SDL3 plan).
- **P3.1.5a — explicit `layout(location=N)` sweep**: done,
  `Scripts/assign_shader_locations.py` / `Scripts/shader_links.json`, all
  shader stage I/O has explicit locations (also a DX11 prerequisite —
  spirv-cross's HLSL backend needs them for SV_Position/SV_Target mapping).
