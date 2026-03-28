# Material Hot Reload Integration Tests — Design Spec

**Date:** 2026-03-27
**Scope:** Two integration tests covering hot reload of material instances and master materials, with screenshot regression verification.

---

## Goals

- Prove that `g_assets.reload_sync()` correctly re-reads a modified `.mi` file and updates visible surface color.
- Prove that `g_assets.reload_sync()` on a `.mm` file triggers shader recompile and propagates to dependent instances, with visible output change.
- Confirm both changes visually with the existing `capture_screenshot` / golden-promotion pipeline.

---

## Architecture

### File Location

`Source/IntegrationTests/Tests/Renderer/test_material_hotreload.cpp`

Single new file added alongside the existing `test_basic.cpp`. No new headers or utility files.

### Shared Inline Helpers (file-local)

**`write_game_file(const std::string& path, const std::string& content) -> bool`**
Opens `FileSys::open_write_game(path)`, writes the full string as bytes, returns true on success. Used to create and overwrite temp material files during tests.

**`setup_test_scene(glm::vec3 cam_pos) -> MeshComponent*`**
- Loads an empty level via `co_await ctx.load_level("")` (empty string = blank level; same pattern as existing tests).
- Spawns an entity, attaches `MeshComponent`, sets model to `eng/cube.glb`.
- Spawns a second entity, attaches `CameraComponent`, positions it at `cam_pos` facing the cube origin, enables it.
- Returns the `MeshComponent*` for the caller to apply material overrides.

**`ScopedTempFile`** — RAII guard that calls `FileSys::delete_game_file(path)` in its destructor. Ensures temp `.mi`/`.mm` files are removed even if the test aborts.

---

## Test 1: `renderer/hotreload_material_instance`

**Timeout:** 20 seconds

### Steps

1. `setup_test_scene({0, 1, -3})` — cube at origin, camera pulled back.
2. Write `mats/test_hotreload_inst.mi`:
   ```
   TYPE MaterialInstance
   PARENT defaultPBR.mm
   VAR colorMult 255 0 0 255
   ```
3. `ScopedTempFile` guard on `mats/test_hotreload_inst.mi`.
4. `g_assets.find_sync<MaterialInstance>("mats/test_hotreload_inst.mi")` — sync load.
5. `mesh->set_material_override(mat)`.
6. `co_await ctx.wait_ticks(3)`.
7. `co_await ctx.capture_screenshot("hotreload_inst_before")` — golden: red cube.
8. Overwrite `mats/test_hotreload_inst.mi` with `colorMult 0 0 255 255` (blue).
9. `g_assets.reload_sync(mat)`.
10. `co_await ctx.wait_ticks(3)`.
11. `co_await ctx.capture_screenshot("hotreload_inst_after")` — golden: blue cube.

### What This Proves

The `.mi` parse path (`MaterialImpl::load_instance` → `post_load` → `on_material_loaded`) correctly picks up a changed color parameter and reflects it on screen.

---

## Test 2: `renderer/hotreload_master_material`

**Timeout:** 30 seconds (shader recompile latency)

### Steps

1. `setup_test_scene({0, 1, -3})`.
2. Write `test_hotreload_master.mm`:
   ```
   TYPE MaterialMaster
   _FS_BEGIN
   void FSmain()
   {
       BASE_COLOR = vec3(1.0, 0.0, 0.0);
   }
   _FS_END
   ```
3. `ScopedTempFile` guard on `test_hotreload_master.mm`.
4. `g_assets.find_sync<MaterialInstance>("test_hotreload_master.mm")` — loading a `.mm` path returns the master's default `MaterialInstance`. Apply to cube via `set_material_override`.
5. `co_await ctx.wait_ticks(3)`.
6. `co_await ctx.capture_screenshot("hotreload_master_before")` — golden: solid red.
7. Overwrite `test_hotreload_master.mm` with GLSL that outputs `vec3(0.0, 1.0, 0.0)` (green).
8. `g_assets.reload_sync(mat)` — triggers shader recompile + cascade reload of dependent instances.
9. `co_await ctx.wait_ticks(5)`.
10. `co_await ctx.capture_screenshot("hotreload_master_after")` — golden: solid green.

### What This Proves

The master material reload path (`MasterMaterialImpl::load_from_file` → shader recompile → `on_reloaded_material` cascade) produces a visually different output.

---

## Screenshot Strategy

Both tests use the standard `capture_screenshot` golden workflow:

- First run with `--promote`: captures baselines for all four screenshots (`_before` and `_after` for each test).
- Subsequent runs: diffs against goldens. If hot reload is broken (before = after visually), the `_after` screenshot will fail its golden.
- No custom pixel-diff assertion needed — the golden system provides the regression signal.

---

## Cleanup

- `ScopedTempFile` deletes temp `.mi`/`.mm` on test exit (normal or aborted).
- No persistent changes to `Data/` after the test suite runs.

---

## Constraints & Non-Goals

- No file system watcher involvement — `reload_sync` is called explicitly.
- No assertion on `is_compilied_shader_valid` — screenshot diff is the observable signal.
- Dynamic material (`alloc_dynamic_mat`) path is not tested here — that is a different code path from hot reload.
- No new `.tmap` files — level is built entirely in code.
