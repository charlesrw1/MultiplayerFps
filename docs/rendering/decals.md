# Decal Material Authoring — Agent Notes

Decals share `.mm`/`.mi` format with regular materials but use `DOMAIN Decal` and explicit `Write*` opt flags to control which G-buffer channels they touch.

## File Locations

| Asset | Location |
|---|---|
| Source textures | `Data/materials/decals/<name>/` |
| `.tis` import settings | Next to each `.png`/`.jpg` |
| Master `.mm` | `Data/materials/decals/<master>.mm` |
| Instance `.mi` | `Data/materials/decals/<name>.mi` |

Reference files:
- `Data/materials/decals/decal_normalmap_pom.mm` — normal-only master with POM
- `Data/materials/decals/crack1_decal.mi` — crack1 instance
- `Data/bulletDecal.mm` — simple alpha-blended (no POM)
- `Data/pavement_crack_decal.mm` — `WriteRoughMetal` + `WriteAlbedo`
- `Data/leaves_decal1.mm` — `WriteNormal` + `WriteAlbedo`

## Texture Import Settings (`.tis`)

One sidecar per source texture; editor compiles to `.dds` on reimport.

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

| Texture role | `is_normalmap` | `is_srgb` |
|---|---|---|
| Normal map | `true` | `false` |
| Mask / height / AO / roughness | `false` | `false` |
| Albedo / color | `false` | `false` (gamma in shader) |

- `resize_width`: 1024 standard; 0 = no resize.
- `src_file`: filename only (sidecar lives next to the image).

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

| Flag | Effect |
|---|---|
| `DOMAIN Decal` | Required. Routes through decal path. |
| `OPT BlendMode Blend` | Alpha-blend onto surface (almost always needed). |
| `OPT WriteNormal` | Writes `NORMALMAP` to G-buffer normal. |
| `OPT WriteAlbedo` | Writes `BASE_COLOR` to G-buffer albedo. |
| `OPT WriteRoughMetal` | Writes `ROUGHNESS` / `METALLIC`. |

**Gotcha:** Write flags must be explicit. Without `WriteAlbedo`, the albedo channel is untouched. Bug fixed 2026-04-10 in `Shaders/MASTER/MasterDecalShader.txt` where albedo wrote unconditionally and darkened surfaces under normal-only decals.

Built-in default textures for `VAR` defaults:

| Name | Contents |
|---|---|
| `"_white"` | (1,1,1,1) |
| `"_black"` | (0,0,0,0) |
| `"_flat_normal"` | (0.5, 0.5, 1.0) |

Fragment inputs in `FSmain()`:

| Variable | Type | Description |
|---|---|---|
| `FS_IN_Texcoord` | `vec2` | Projected UV on decal quad |
| `FS_IN_FragPos` | `vec3` | World-space fragment position |
| `FS_IN_TBN` | `mat3` | Tangent-bitangent-normal matrix |
| `g.viewpos_time.xyz` | `vec3` | Camera world position |

Outputs:

| Variable | Controls |
|---|---|
| `OPACITY` | Alpha / blend weight |
| `NORMALMAP` | Tangent-space normal (vec3, raw — NOT octahedral) |
| `BASE_COLOR` | Linear RGB albedo |
| `ROUGHNESS` | 0–1 |
| `METALLIC` | 0–1 |
| `AOMAP` | 0–1 |

## Normal Map Decoding

Standard codebase pattern — copy verbatim:

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

Use: `NORMALMAP = uncompress_normal(texture(Normal, uv).xyz);`

## Parallax Occlusion Mapping (POM) in Decals

Offsets UVs by tangent-space view dir to fake depth. Requires a grayscale height map: `0 = deep`, `1 = raised`. A crack mask (white=surface, black=crack) works as a proxy.

Complete implementation from `decal_normalmap_pom.mm`:

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

POM `VAR` defaults:

```
VAR texture2D HeightMap "_black"   # flat by default; instance overrides
VAR float PomScale      0.04       # 0.02–0.08 typical
VAR float PomLayers     8.0        # doubles at glancing angles
```

## Material Instance Format (`.mi`)

```
TYPE MaterialInstance
PARENT materials/decals/decal_normalmap_pom.mm

VAR Normal    materials/decals/crack1/crack1_normal.dds
VAR Mask      materials/decals/crack1/crack1_mask.dds
VAR HeightMap materials/decals/crack1/crack1_mask.dds
```

- All paths relative to `Data/`.
- Reference compiled `.dds`, not source `.png`.
- Override only what differs from master defaults.

## Automation Script

`Scripts/gen_crack_decal.py` writes `.tis` for each source texture, the master `.mm` with inline GLSL, and the instance `.mi`. Copy + edit `CRACK1_DIR` / texture filenames / material strings for new decals. Run with `py Scripts/gen_crack_decal.py`.

## Checklist: Adding a New Decal Material

1. Drop source textures in `Data/materials/decals/<name>/`.
2. Run/adapt the gen script → produces `.tis`, `.mm`, `.mi`.
3. Reimport in editor → compiles `.dds`.
4. Load `.mi` into a `DecalComponent` (scene or Lua).
5. For POM: master needs `VAR texture2D HeightMap`, instance must point it at a valid grayscale texture.
