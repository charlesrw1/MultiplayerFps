# Asset Tools

Static library (`Source/AssetTools/AssetTools.vcxproj`) containing editor-only asset compilation, diagnostics, file operations, and templates. Built only under `EDITOR_BUILD`.

Consuming projects (App, UnitTests, SlimRenderApp) link via `/WHOLEARCHIVE:AssetTools.lib`.

## compiler

`AssetCompiler` namespace (`AssetCompiler.h/cpp`) — compile individual assets or the whole project.

- `compile_asset(gamepath)` — dispatches by extension: `.mis`→model, `.tis`→texture, `.mm`→material, `.lua`→lua check
- `build_all(force_rebuild)` — iterates all game files, compiles stale, updates AssetDiagnostics
- `register_console_commands()` — call at editor startup; registers `ASSET_BUILD_ALL`, `ASSET_REBUILD_ALL`, `ASSET_CHECK_ERRORS`, `ASSET_COMPILE <path>`

Material compile (`mm`) returns success immediately; actual GLSL→SPIR-V compile happens via the asset reload path which requires `spirv_is_initialized()`.

## diagnostics

`AssetDiagnostics` singleton (`AssetDiagnostics.h/cpp`) — per-asset error/warning cache.

- `get_severity(gamepath)` — returns worst severity or nullopt (used by editor for overlay icons)
- `scan_dependencies(gamepath)` — checks source files referenced by `.tis`, `.mis`, `.mi`
- `scan_all()` — iterates all game files
- `save()` / `load()` — persist cache to `Data/.asset_diag_cache.json`

Call `load()` at editor startup, `save()` at shutdown.

## ops

`AssetOps` namespace (`AssetOps.h/cpp`) — file operations that keep references consistent.

- `mv(src_gamepath, dst_folder)` — renames file + sidecar (`.tis`↔`.dds`, `.mis`↔`.cmdl`), rewrites references in `.cmdl`, `.dds`, `.mm`, `.mi`, `.tmap` files
- `rm(gamepath)` — sends to Windows trash bin (`SHFileOperationW FO_DELETE + FOF_ALLOWUNDO`)
- `cp(src, dst)` — plain copy, no reference rewrite
- `find_references(gamepath)` — searches ref-extension files for the gamepath string

Reference replacement uses boundary-aware matching (must be surrounded by `"`, space, `:`, `\n`, `\r`, `\t` or at boundaries) to avoid partial-path false positives.

## packager

`AssetPackager` namespace (`AssetPackager.h/cpp`) — bundle game files for distribution (stub; `package_to_bundle` is not yet implemented).

## templates

`AssetTemplates` namespace (`AssetTemplates.h/cpp`) — create new asset sidecar files from source files.

- `create_tis_for_png(png_gamepath)` — writes `.tis` with `is_normalmap` inferred from "normal"/"Normal" in filename; called automatically by `AssetRegistry` FileWatcher on `.png` arrival
- `create_mis_for_glb(glb_gamepath)` — writes `.mis` with `srcGlbFile` set
- `create_mi_from_template(dir, name, master_mm_path)` — writes `.mi` with `PARENT` line
