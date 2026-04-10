# Decal Material Authoring — Agent Notes

Last updated: 2026-04-10

## Overview

Decals are projected materials painted onto surfaces in the deferred G-buffer pass. They share the same `.mm` / `.mi` material format as regular materials but use `DOMAIN Decal` and opt-in write flags that control which G-buffer channels they write.

This document covers:
- Texture import settings (`.tis`)
- Decal master material structure (`.mm`)
- Decal instance files (`.mi`)
- Parallax occlusion mapping (POM) in decals
- Reference files for each

---

## File Locations

| Asset type | Location convention |
|-----------|-------------------|
| Textures | `Data/materials/decals/<decal_name>/` |
| Texture import settings | Same directory as textures, one `.tis` per `.png`/`.jpg` |
| Master material | `Data/materials/decals/<master_name>.mm` |
| Instance material | `Data/materials/decals/<decal_name>.mi` (or in the texture subdir) |

Existing references:
- `Data/materials/decals/decal_normalmap_pom.mm` — normal-only decal master with POM
- `Data/materials/decals/crack1_decal.mi` — crack1 instance
- `Data/bulletDecal.mm` — simple alpha-blended decal (no POM)
- `Data/pavement_crack_decal.mm` — pavement crack with `WriteRoughMetal` + `WriteAlbedo`
- `Data/leaves_decal1.mm` — leaf decal with `WriteNormal` + `WriteAlbedo`

---

## Texture Import Settings (`.tis`)

Each source texture needs a `.tis` sidecar so the editor compiles it to `.dds`.

**Format:**
```
!json
{
 "__classname": "TextureImportSettings",
 "is_generated": false,
 "is_normalmap": <bool>,
 "is_srgb": <bool>,
 "resize_width": <int>,
 "src_file": "<filename.png>"
}
```

**Rules by texture type:**

| Texture role | `is_normalmap` | `is_srgb` |
|-------------|---------------|----------|
| Normal map | `true` | `false` |
| Mask / height / AO / roughness | `false` | `false` |
| Albedo / color | `false` | `false` (engine does gamma in shader) |

- `resize_width`: 1024 is standard; 0 means no resize.
- `src_file`: just the filename, not a path — the `.tis` lives next to the source image.
- After writing `.tis` files, reimport in the editor to compile `.dds` outputs.

---

## Master Material Format (`.mm`) — Decal Domain

```
TYPE MaterialMaster
DOMAIN Decal
OPT BlendMode Blend

# Optional write-flag opts (omit to skip that channel):
OPT WriteNormal
OPT WriteAlbedo
OPT WriteRoughMetal

VAR texture2D Normal    "_flat_normal"
VAR texture2D Mask      "_white"
VAR float SomeParam     1.0
VAR vec4  Color         255 255 255 255

_FS_BEGIN
void FSmain()
{
    OPACITY   = texture(Mask, FS_IN_Texcoord).r;
    NORMALMAP = /* decoded normal */;
    BASE_COLOR = /* if WriteAlbedo */;
    ROUGHNESS  = /* if WriteRoughMetal */;
}
_FS_END
```

**Key domain / opt flags:**

| Flag | Effect |
|------|--------|
| `DOMAIN Decal` | Required. Routes material through the decal render path. |
| `OPT BlendMode Blend` | Alpha-blends the decal onto the surface (almost always needed). |
| `OPT WriteNormal` | Writes `NORMALMAP` output to G-buffer normal channel. |
| `OPT WriteAlbedo` | Writes `BASE_COLOR` output to G-buffer albedo channel. |
| `OPT WriteRoughMetal` | Writes `ROUGHNESS` / `METALLIC` outputs. |

> **Note:** All four write flags are required to be explicit. Without `WriteAlbedo`, the albedo G-buffer channel is never touched — this was a bug (fixed 2026-04-10) where albedo was written unconditionally, causing normal-only decals to darken surfaces. The fix is in `Shaders/MASTER/MasterDecalShader.txt`.

**Built-in default textures** (use as VAR defaults when no real texture yet):

| Name | Contents |
|------|---------|
| `"_white"` | Solid white (1,1,1,1) |
| `"_black"` | Solid black (0,0,0,0) |
| `"_flat_normal"` | Flat normal (0.5, 0.5, 1.0) |

**Built-in fragment inputs** available in `FSmain()`:

| Variable | Type | Description |
|----------|------|-------------|
| `FS_IN_Texcoord` | `vec2` | Projected UV on decal quad |
| `FS_IN_FragPos` | `vec3` | World-space fragment position |
| `FS_IN_TBN` | `mat3` | Tangent-bitangent-normal matrix |
| `g.viewpos_time.xyz` | `vec3` | Camera world position |

**Output variables** (assign inside `FSmain()`):

| Variable | Controls |
|----------|---------|
| `OPACITY` | Alpha / blend weight of the decal |
| `NORMALMAP` | Tangent-space normal (vec3, NOT normalized octahedral) |
| `BASE_COLOR` | Linear RGB albedo |
| `ROUGHNESS` | Scalar 0–1 |
| `METALLIC` | Scalar 0–1 |
| `AOMAP` | Scalar 0–1 |

---

## Normal Map Decoding

Standard pattern used throughout the codebase — copy it verbatim:

```glsl
vec3 uncompress_normal(vec3 bc_normal)
{
    vec3 s = bc_normal * 2.0 - vec3(1.0);
    float x2 = s.x * s.x;
    float y2 = s.y * s.y;
    s.z = sqrt(max(1.0 - x2 - y2, 0.0));
    return s;
}
```

Call as: `NORMALMAP = uncompress_normal(texture(Normal, uv).xyz);`

---

## Parallax Occlusion Mapping (POM) in Decals

POM offsets UVs based on the view direction in tangent space, making a surface appear to have depth. For decals, it makes cracks / dents look recessed.

**Requires:** a grayscale height map where `0 = deep`, `1 = raised`. A crack mask (white=surface, black=crack) works as a proxy.

**Complete POM implementation** (from `decal_normalmap_pom.mm`):

```glsl
vec3 get_view_tangent()
{
    vec3 V = normalize(g.viewpos_time.xyz - FS_IN_FragPos);
    return FS_IN_TBN * V;
}

vec2 do_parallax_mapping(vec2 uv, vec3 viewTS, float scale, float num_layers)
{
    float layer_depth = 1.0 / num_layers;
    float cur_depth   = 0.0;
    vec2 delta = (viewTS.xy / max(abs(viewTS.z), 0.001)) * scale / num_layers;

    vec2 cur_uv = uv;
    float depth = 1.0 - texture(HeightMap, cur_uv).r;

    while (cur_depth < depth) {
        cur_uv   -= delta;
        depth     = 1.0 - texture(HeightMap, cur_uv).r;
        cur_depth += layer_depth;
    }

    // Refine between last two steps.
    vec2 prev_uv = cur_uv + delta;
    float after  = depth - cur_depth;
    float before = (1.0 - texture(HeightMap, prev_uv).r) - cur_depth + layer_depth;
    return mix(cur_uv, prev_uv, after / (after - before));
}

void FSmain()
{
    vec3 viewTS  = get_view_tangent();
    float layers = mix(PomLayers * 2.0, PomLayers,
                       abs(dot(vec3(0.0, 0.0, 1.0), viewTS)));
    vec2 uv = do_parallax_mapping(FS_IN_Texcoord, viewTS, PomScale, layers);

    OPACITY   = texture(Mask, uv).r;
    NORMALMAP = uncompress_normal(texture(Normal, uv).xyz);
}
```

**Recommended VAR defaults for POM:**

```
VAR texture2D HeightMap "_black"   # flat by default; instance overrides
VAR float PomScale      0.04       # controls apparent depth (0.02–0.08 typical)
VAR float PomLayers     8.0        # quality; doubles at glancing angles
```

---

## Material Instance Format (`.mi`)

```
TYPE MaterialInstance
PARENT materials/decals/decal_normalmap_pom.mm

VAR Normal    materials/decals/crack1/crack1_normal.dds
VAR Mask      materials/decals/crack1/crack1_mask.dds
VAR HeightMap materials/decals/crack1/crack1_mask.dds
```

- `PARENT` path is relative to `Data/`.
- `VAR` texture paths are also relative to `Data/`.
- Only override parameters that differ from the master's defaults.
- Compiled `.dds` files are referenced, not the source `.png`.

---

## Automation Script

`Scripts/gen_crack_decal.py` is the reference script for automating this. It:
1. Writes `.tis` for each source texture (normal, mask, AO).
2. Writes the master `.mm` with inline GLSL.
3. Writes the instance `.mi` with texture path overrides.

To write a similar script for a new decal:
- Copy `gen_crack_decal.py`, update `CRACK1_DIR`, texture filenames, and material content strings.
- Run with: `py Scripts/gen_crack_decal.py`

---

## Checklist: Adding a New Decal Material

1. Place source textures under `Data/materials/decals/<name>/`.
2. Run (or adapt) the generation script to produce `.tis`, `.mm`, `.mi`.
3. Reimport textures in editor → `.dds` files are compiled from `.tis`.
4. Load the `.mi` path into a `DecalComponent` in the scene or Lua.
5. If adding POM: ensure the master has `VAR texture2D HeightMap` and the instance points it at a valid grayscale texture.
