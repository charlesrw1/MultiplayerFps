# Graphics Device Abstraction — Sub-phase changelog

Historical record of every Phase 1 and Phase 2 sub-phase as it landed.
Design and forward plan live in [[gfx_abstraction]]; this file is append-only
per-commit status notes split out so the design doc stays readable.

Order is reverse-chronological: most recent at the top. Within each entry,
the body is the as-landed implementation summary (API additions, caller
migrations, deviations from plan, test status).

## Sub-phase B5 status — SDL2 → SDL3 + SDL3_mixer

Single-sweep migration. No SDL2 DLL remains in the binary.

- **vcpkg**: `sdl3` 3.4.0 (upgraded from 3.2.8) + `sdl3-mixer` 3.2.2. Removed
  `sdl2`, `sdl2-mixer`, `sdl2-image`, `sdl2-ttf`, `sdl2-net` from the install
  set (sdl3-mixer 3.2.2 requires SDL3 ≥ 3.4.0).
- **SDL_main**: dropped `SDL2maind.lib` / `SDL2main.lib` `<manual-link>`
  entries from `Source/App/App.vcxproj` and `Source/UnitTests/UnitTests.vcxproj`.
  `Source/App/main.cpp` defines `SDL_MAIN_HANDLED` and includes
  `<SDL3/SDL_main.h>` — SDL3 ships a header-only main shim.
- **Backward-compat sweep**: `SDL_ENABLE_OLD_NAMES` added to every
  `PreprocessorDefinitions` block in `CsRemake.vcxproj`, `App.vcxproj`, and
  `UnitTests.vcxproj`. SDL3 ships `SDL_oldnames.h` (auto-included by the
  umbrella `<SDL3/SDL.h>` only) that aliases `SDL_CONTROLLER_BUTTON_*` →
  `SDL_GAMEPAD_BUTTON_*`, `SDL_GameController*` → `SDL_Gamepad*`,
  `SDL_NUM_SCANCODES` → `SDL_SCANCODE_COUNT`, etc. New
  `Source/Input/Sdl2CompatGamepad.h` is a thin shim that pulls
  `<SDL3/SDL_oldnames.h>` for files including subsystem headers directly. This
  kept ~40 game-side / camera-side / editor-side call-sites unchanged.
- **Window / GL context** (`Source/Render/OpenGlDevice.cpp`,
  `Source/EngineMain_Init.cpp`): `SDL_CreateWindow(title, x, y, w, h, flags)` →
  `SDL_CreateWindow(title, w, h, flags)` (no positional args). `SDL_Init` flags
  narrowed from `SDL_INIT_EVERYTHING` to `SDL_INIT_VIDEO|EVENTS|GAMEPAD|AUDIO`
  (single SDL3 init now covers audio for SDL3_mixer). `SDL_GL_DeleteContext` →
  `SDL_GL_DestroyContext`. `gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)`
  cast required (SDL3 returns `SDL_FunctionPointer`).
- **Event loop** (`Source/EngineMain_Loop.cpp`): omnibus `SDL_WINDOWEVENT`
  split into discrete `SDL_EVENT_WINDOW_*` enums. Resize now handles both
  `SDL_EVENT_WINDOW_RESIZED` and `SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED`. Key /
  mouse / wheel events renamed `SDL_KEYDOWN` → `SDL_EVENT_KEY_DOWN`, etc.
- **Keyboard / mouse** (`Source/Input/InputSystem.{h,cpp}`,
  `Source/UI/GUISystemLocal.cpp`, `Source/LevelEditor/EditorDocViewport.cpp`,
  `Source/Game/TopDownShooter/TopDownPlayer.cpp`): `SDL_GetKeyboardState`
  returns `const bool*` (was `const Uint8*`). `SDL_GetMouseState` /
  `SDL_GetRelativeMouseState` widened to `float*` — call-sites convert at the
  boundary. `SDL_BUTTON(i)` → `SDL_BUTTON_MASK(i)`. `event.key.keysym.scancode`
  → `event.key.scancode`, `event.key.keysym.mod` → `event.key.mod`. `KMOD_CTRL`
  → `SDL_KMOD_CTRL`. `SDL_SetRelativeMouseMode(bool)` →
  `SDL_SetWindowRelativeMouseMode(SDL_Window*, bool)`. `SDL_WarpMouseInWindow`
  args promoted `int` → `float`. `SDL_SetWindowFullscreen` takes plain `bool`
  (no `SDL_WINDOW_FULLSCREEN_DESKTOP` flag).
- **Gamepad** (`Source/Input/InputSystem.{h,cpp}`): `SDL_GameController*`
  family wholesale renamed to `SDL_Gamepad*`. `SDL_NumJoysticks()` →
  `SDL_GetJoysticks(&count)` (returns array, free with `SDL_free`).
  `SDL_GameControllerOpen` / `Close` / `FromInstanceID` / `GetButton` / `GetAxis`
  → `SDL_OpenGamepad` / `SDL_CloseGamepad` / `SDL_GetGamepadFromID` /
  `SDL_GetGamepadButton` / `SDL_GetGamepadAxis`. Event field `event.cdevice` /
  `caxis` / `cbutton` → `gdevice` / `gaxis` / `gbutton`. Semantic shift:
  ADDED/REMOVED `which` is now uniformly an SDL_JoystickID (instance id) in
  SDL3 — `Device::index` is now an instance id; `default_dev_index` stores the
  first device's instance id rather than slot 0. 4-gamepad cap enforced by
  `devices.size() >= 4` rather than `index < 4`.
- **ImGui platform** (`Source/External/imgui_impl_sdl3.{h,cpp}`): vendored
  from imgui v1.91.0 (earliest tag with SDL3 backend). Patched for the project's
  imgui 1.89.4 core — removed `io.AddMouseSourceEvent` calls (added in 1.89.5),
  removed `SDLK_F13`..`F24` / `SDLK_AC_BACK`/`FORWARD` keymap cases (added in
  v1.91), renamed `PlatformSetImeDataFn` → `SetPlatformImeDataFn` (signature
  reverted from `(ctx, vp, data)` to `(vp, data)`), dropped `SDL_TRUE` /
  `SDL_FALSE` cast on `SDL_CaptureMouse`. `imgui_impl_sdl2.{h,cpp}` deleted.
  `External.vcxproj` + `CsRemake.vcxproj` + both `.filters` updated.
- **Audio port** (`Source/Sound/SoundLocal.{h,cpp}`, `Source/Sound/SoundPublic.h`):
  `Mix_Chunk*` → `MIX_Audio*` (3 type refs in headers). `Mix_OpenAudio(freq,fmt,ch,bufsize)`
  → `MIX_Init()` + `MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr)`.
  Channel-index voice model replaced with a pre-allocated `MIX_Track*` pool
  sized by `snd_max_voices` cvar — each `SoundPlayerInternal::voice_index`
  indexes into both the `active_voices` and `tracks` vectors.
  `Mix_PlayChannel(idx, chunk, loops)` →
  `MIX_SetTrackAudio(track, audio)` + `MIX_SetTrackLoops(track, loops)` +
  `MIX_PlayTrack(track, 0)`. `Mix_HaltChannel` → `MIX_StopTrack(track, 0)` +
  `MIX_SetTrackAudio(track, nullptr)` to release the audio binding.
  `Mix_Volume(idx, 0..128)` → `MIX_SetTrackGain(track, 0..1)`. `Mix_SetPanning`
  → `MIX_SetTrackStereo(track, MIX_StereoGains{l,r})` (0..1 floats).
  `Mix_LoadWAV` → `MIX_LoadAudio(mixer, path, /*predecode=*/true)`;
  `Mix_FreeChunk` → `MIX_DestroyAudio`. Duration computed via
  `MIX_AudioFramesToMS(MIX_GetAudioDuration(audio))` (SDL3_mixer doesn't expose
  raw PCM length).
- **DSP simplification**: the entire `PlaybackSpeedEffectHandler<int16_t>`
  manual-resample template deleted — pitch is now `MIX_SetTrackFrequencyRatio`
  (SDL3_mixer's built-in rate-conversion). `LowPassFilter` ported from
  `int16_t` to plain `float` (SDL3_mixer cooks everything to float32 before
  the post-mix callback), and registered via `MIX_SetTrackCookedCallback`
  at track-creation time rather than per-play.
- **Tests**: 185 unit tests green. Integration suite green for both `game` and
  `editor` modes (84 game tests, 51 editor tests, ~135 total covering renderer
  hot-reload, asset rebind, physics, nav, lua, FPS / bike / car / topdown /
  obs gameplay smokes).
- **DLL footprint**: post-build inventory shows `SDL3.dll`, `SDL3_mixer.dll`;
  no `SDL2*.dll`. Static-scanner accept-list unchanged.

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

## Sub-phase 2a status — done (group migrations) + dead-shader sweep

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
  along). See `gfx_abstraction.md#2a follow-up` for the `#define`
  shadow-class gotcha that broke `linearizedepth.txt` first time round.
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

## Sub-phase 2a pilot status

Migrated `BloomDownsampleF` + `BloomUpsampleF`. New struct defs +
`BLOOM_PARAMS_UBO_BINDING = 7` live in `DrawLocal_RenderPass.cpp`
(anonymous namespace) alongside `render_bloom_chain`. Per-pass UBO storage
(`ubo.bloom_downsample_params`, `ubo.bloom_upsample_params`) is created in
`Renderer::Init`. Program-binary cache invalidates automatically (shader
source touched → cached binary stale by timestamp). 185 unit tests + 80
integration tests green.

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

- API added: `GraphicsFillMode { Fill, Line }` + `set_polygon_fill_mode(GraphicsFillMode)` over `glPolygonMode(GL_FRONT_AND_BACK, …)` — immediate setter; Phase 2c folds onto `RenderPipelineState::fill_mode` (see `gfx_abstraction.md#pipeline-model`).
- `Renderer::render_bloom_chain` signature migrated from `texhandle` to `IGraphicsTexture*`. Internal `device.bind_texture(0, scene_color_handle)` becomes `bind_texture_ptr(0, scene_color)`.
- `DrawLocal_SceneDrawInternal.cpp`: wireframe pass `glPolygonMode`/`glLineWidth` pair → `gfx().set_polygon_fill_mode` + `gfx().set_line_width`. 3× `glDrawArrays(GL_TRIANGLES, 0, 3)` (TAA resolve, composite, post-process stack) → `gfx().draw_arrays(Triangles, 0, 3)`. `bind_texture(0, scene_color_handle->get_internal_handle())` → `bind_texture_ptr(0, scene_color_handle)`. Trailing defensive `glBindFramebuffer(GL_FRAMEBUFFER, 0)` deleted (FBO 0 is the default; renderer sets its own FBO before any subsequent draws — same rationale as the EnvProbe::init cleanup deleted in 1.2b). Dead-code comments referencing `glNamedFramebufferTexture` / `glBlitNamedFramebuffer` / `GL_COLOR_BUFFER_BIT` removed. `glad/glad.h` include dropped.
- `DrawLocal_SceneDrawInternal.cpp` contains zero direct `gl*` calls and zero `get_internal_handle()` calls. 184 unit tests + full integration suite green.

## Sub-phase 1.5a status

- API added: `set_color_write_mask(int attachment, bool r, bool g, bool b, bool a)` — per-attachment color mask (wraps `glColorMaski`). Immediate setter; Phase 2c folds the mask onto `RenderPipelineState::color_write_masks` (see `gfx_abstraction.md#pipeline-model`) so decal materials carry their own per-attachment write mask as part of pipeline state.
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

- API added: `bind_parameter_buffer(IGraphicsBuffer*)` over `GL_PARAMETER_BUFFER` (nullptr unbinds); `set_polygon_offset(bool enabled, float factor, float units)` over `glEnable/Disable(GL_POLYGON_OFFSET_FILL)` + `glPolygonOffset` (Phase 2c folds onto `RenderPipelineState::polygon_offset_*`); `multi_draw_elements_indirect_count(mode, index_type, indirect_byte_offset, count_byte_offset, max_draw_count, stride)` wrapping `glMultiDrawElementsIndirectCount`. Both `bind_indirect_buffer` and `bind_parameter_buffer` are deleted in 2c — the buffer pointer moves onto the indirect draw call itself (see `gfx_abstraction.md#pipeline-model`).
- `DrawLocal_BatchScene.cpp`: 3× `glBindBufferBase(GL_SHADER_STORAGE_BUFFER, …)` on `IGraphicsBuffer*` → `bind_storage_buffer_base`; `scene.gpu_skinned_mats_buffer` (still a raw `bufferhandle` on `RenderScene`) routed via `bind_storage_buffer_base_raw`; `glBindBuffer(GL_PARAMETER_BUFFER)` → `bind_parameter_buffer`; `glBindBuffer(GL_DRAW_INDIRECT_BUFFER)` → `bind_indirect_buffer` (set + unbind); `glMultiDrawElementsIndirectCount` → `multi_draw_elements_indirect_count`; `glEnable(GL_POLYGON_OFFSET_FILL)`/`glPolygonOffset`/`glDisable` → `set_polygon_offset`.
- Local `MODEL_INDEX_TYPE = VertexInputIndexType::uint16` mirror introduced (matches the file-local `MODEL_INDEX_TYPE_GL` constant still referenced by other passes). `glad/glad.h` include dropped.
- `DrawLocal_BatchScene.cpp` contains zero direct `gl*` calls. 184 unit tests + full integration suite green.

## Sub-phase 1.4a status

- API added: `set_line_width(float)` (debug meshbuilder line width — Phase 2c folds onto `RenderPipelineState::line_width`); `bind_indirect_buffer(IGraphicsBuffer*)` over `GL_DRAW_INDIRECT_BUFFER` — `nullptr` unbinds. Phase 2c moves the indirect-buffer pointer onto the draw call (see `gfx_abstraction.md#pipeline-model`).
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
