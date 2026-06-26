# Texture Pipeline

Covers how .png, .tis, and .dds interact from source to GPU.

## Three texture kinds

| Kind | Source | Load path | Notes |
|------|--------|-----------|-------|
| Game texture | .png/.jpg | compile → .dds, load .dds | BC-compressed, mip chain |
| UI / direct texture | .png | load .png directly | full RGBA8, nearest or linear |
| HDR / EXR | .hdr/.exr | load directly | float formats |

## .tis sidecar

Every game texture and UI texture that needs non-default settings has a `.tis` sidecar. It is a ClassBase object serialised as `!json\n<json>`.

Key fields:

- `src_file` — relative filename of the source image (e.g. `folder_closed.png`).
- `load_source_file` — when true, skip texconv and load the source image directly (UI textures). No `.dds` is produced.
- `nearest_filtering` — load with nearest/point sampler instead of linear.
- `is_srgb`, `is_normalmap`, `make_uncompressed`, `resize_width` — compiler hints; ignored for UI textures.
- `simplifiedColor` — average pixel colour computed by the compiler and stored back; read-only in the inspector.

The file watcher auto-creates a `.tis` with `src_file = <basename>` when a new `.png` appears in the Data directory. For UI textures, set `load_source_file = true` via the inspector to opt out of compilation.

## compile_texture_asset flow

```
compile_texture_asset(gamepath)
  ├─ open stem.dds + stem.tis
  ├─ parse .tis → TextureImportSettings
  ├─ if .dds newer than .tis → return true  (up-to-date)
  ├─ if load_source_file     → return true  (UI texture, no-op)
  ├─ if src_file not found   → return false (missing source)
  └─ spawn texconv → write .dds, update simplifiedColor in .tis
```

`gamepath` may be `.png`, `.dds`, or `.tis` — the stem is always stripped before opening either sidecar, so the freshness check is always `.dds` vs `.tis` timestamps regardless of how the asset was requested.

## nearest_filtering at load time

`Texture::load_asset` calls `read_tis_nearest_filtering(path)` for every `.png`/`.jpg` load (editor and shipped builds alike). It opens `stem.tis`, parses just the `nearest_filtering` JSON field, and returns the result — no full ClassBase deserialisation. Missing `.tis` → `false`.

`force_nearest` on the `Texture` object (set by `force_load_for_ui`) overrides and always wins.

## Asset Browser representation

The browser shows each logical texture as exactly one entry:

| Files on disk | Browser entry | Notes |
|---|---|---|
| `foo.tis` + `foo.dds` | `foo.dds` | game texture — compiled output is canonical |
| `foo.tis` + `foo.png` (no `.dds`) | `foo.png` | UI texture — .tis has empty `src_file` |
| `foo.dds` (no `.tis`) | `foo.dds` | baked/orphan — shows Info diagnostic |

`.tis` and source `.png`/`.jpg` files are never shown as top-level entries.
When a `.dds` is compiled (appears on disk), the `foo.png` entry is removed and `foo.dds` takes its place; when a `.dds` is deleted, the reverse happens.

## Asset Inspector

Selecting a `.dds` or `.tis` in the Asset Browser opens the texture inspector which shows a thumbnail, format string, mip selector, hover-zoom, simplified colour swatch, and editable settings. Apply writes the `.tis` and re-triggers `compile_texture_asset`. Source: `Source/Assets/AssetInspectorPane.cpp`.

## Editor-only vs. shipped build

`compile_texture_asset` is `#ifdef EDITOR_BUILD`. In shipped builds only `read_tis_nearest_filtering` runs, reading the single `nearest_filtering` field from the `.tis`. All other `.tis` fields are irrelevant at runtime.
