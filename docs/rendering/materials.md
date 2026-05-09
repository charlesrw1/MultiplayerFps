# Materials

Two-tier system: **master materials** (`.mm`, shader code + param decls) and **material instances** (`.mi`, parameter overrides). Masters are text-only; instances are editable in text or the visual material editor.

## Authoring Master Materials

Four parts: `OPT`, `VAR`, `DOMAIN`, shader blocks (`_VS_BEGIN`/`_VS_END`, `_FS_BEGIN`/`_FS_END`).

### `OPT` — option parameters

`OPT <what> <value>`. First value listed is the default.

- `AlphaTested` (`false` / `true`) — uses `Opacity` output.
- `BlendMode` (`Opaque` / `Blend` / `Add`) — transparency.
- `LightingMode` (`Lit` / `Unlit`).
- `ShowBackfaces` (`false` / `true`) — disables backface culling.
- Decals: `WriteNormal`, `WriteAlbedo`, `WriteRoughMetal` — see [[rendering/decals]].

### `VAR` — input parameters

`VAR <type> <name> <default>`. Accessed in shader code as if they were GLSL variables.

| Type | Example |
|---|---|
| `texture2D` | `VAR texture2D MyTexture "my_texture.dds"` |
| `float` | `VAR float MyFloat 0.0` |
| `vec4` | `VAR vec4 MyColor 255 255 255 255` (uint8) |

Built-in default textures:

| Name | Contents |
|---|---|
| `"_white"` | (1,1,1,1) |
| `"_black"` | (0,0,0,0) |
| `"_flat_normal"` | (0.5, 0.5, 1.0) |

### `DOMAIN` — shader type

`DOMAIN <what>`:

- `Default` — standard deferred
- `Decal` — see [[rendering/decals]]
- `PostProcess`
- `Terrain`, `UI`, `Particle`

### Shader inputs

Available in `_FS_BEGIN`/`_FS_END`:

- `(vec4) g.viewpos_time` — xyz: camera pos, w: time
- `(vec4) g.viewfront` — xyz: camera front
- `(mat4) g.viewproj`
- `(vec3) FS_IN_FragPos` — world-space fragment pos
- `(vec3) FS_IN_Normal` — non-normalized world normal
- `(vec2) FS_IN_Texcoord`

### Shader outputs

- `(vec3) WORLD_POSITION_OFFSET` — vertex shader only; world-space vertex offset.
- `(vec3) BASE_COLOR` — albedo
- `(float) ROUGHNESS`
- `(float) METALLIC`
- `(vec3) EMISSIVE` — added to lit color
- `(vec3) NORMALMAP` — tangent-space
- `(float) OPACITY` — `AlphaTested` + transparent opacity
- `(float) AOMAP` — multiplied with `BASE_COLOR`

### Material Instance (`.mi`) format

```
TYPE MaterialInstance
PARENT <path/to/master.mm>
VAR <parameter_name> <value>   # override defaults
```

Paths relative to `Data/`. Override only what differs from master defaults.

---

## Internals (Agent Notes)

### Key files

- `Source/Render/MaterialPublic.h` — public API (`MaterialInstance`, `DynamicMatUniquePtr`)
- `Source/Render/MaterialLocal.h` — internal class decls
- `Source/Render/MaterialLocal.cpp` — implementation (~1200 lines)
- `Source/Render/DynamicMaterialPtr.h` — custom deleter for dynamic-mat unique_ptr
- `Data/*.mm`, `Data/*.mi`
- `Shaders/MASTER/` — master shader templates

### Class hierarchy

```
MaterialInstance (IAsset)
  └── MaterialImpl
        ├── params: vector<MaterialParameterValue>
        ├── masterImpl: unique_ptr<MasterMaterialImpl>  (if this IS a master)
        └── masterMaterial: shared_ptr<MaterialInstance> (if this is an instance)

MasterMaterialImpl
  └── param_defs: vector<MaterialParameterDefinition>
  └── blend state, lighting mode, usage type, material ID

MaterialManagerLocal (singleton via matman global)
  ├── MaterialShaderTable        — shader_key → program_handle cache
  ├── AllMaterialTable           — GPU buffer slot allocator (BitmapAllocator, max 1024)
  ├── TextureBindingHasher       — deduplicates texture binding sets
  ├── DynamicMaterialAllocator   — pool for runtime materials
  └── dirty_list                 — materials needing GPU upload
```

### Loading flow

1. `MaterialImpl::load_from_file()` — dispatches by `.mm`/`.mi` extension.
2. **Master**: `DictParser` reads `VAR`/`OPT`/`DOMAIN`/shader blocks; param offsets assigned, packed into ≤64 bytes.
3. **Instance**: loads parent (recursive), copies defaults, applies overrides.
4. `post_load()` — assigns material ID, adds to `dirty_list`.
5. `AllMaterialTable::register_material()` — allocates 64-byte GPU slot.

### GPU buffer layout

- 64-byte slot per material in shared SSBO.
- `MAX_MATERIALS = 1024` concurrent.
- float = 4B; vec4 = 16B; Color32 = 4B.
- Textures live in a separate bindings array (not in the 64B slot).
- Params sorted by type for alignment at load.

### Shader compilation

- `EDITOR_BUILD` only.
- `MasterMaterialImpl::create_glsl_shader()` — generates `.glsl` from master template + material code; injects `layout(binding=N)` for textures, rewrites param refs to SSBO reads.
- `#include` is recursive with **no cycle detection** — circular include = infinite loop.
- Runtime: `MaterialManagerLocal::compile_mat_shader()` compiles on demand, cached by `shader_key`.
- `shader_key` = 32-bit: material ID (23) + MSF flags (9).

### Dynamic materials

- `MaterialManagerLocal::create_dynmaic_material_unsafier()`
- Copies parent master ref + all param values.
- `DynamicMatUniquePtr` calls `free_dynamic_material()` on drop.
- Pool reuse via `DynamicMaterialAllocator`.
- Lua: `alloc_dynamic_mat` / `free_dynamic_mat`.

### Material reloading

- Hot-reload via asset system.
- `MaterialInstance::move_construct()` re-parses, finds dependent instances, reloads them.
- `MaterialManagerLocal::on_reloaded_material()` triggers `BuildSceneData_CpuFast`.

### Parameter setting (runtime)

```cpp
mat->set_float_parameter(name, f);
mat->set_floatvec_parameter(name, vec4);
mat->set_u8vec_parameter(name, Color32);
mat->set_tex_parameter(name, texture);
```

Each call marks dirty; upload in `pre_render_update()`.

### Known issues / brittleness

2. `DictParser` has no error recovery — any parse error throws `MasterMaterialExcept`.
3. No `#include` cycle detection.
4. Param 64-byte fit checked only at load.
5. `move_construct()` "FIXME: unsafe for materials already referencing us".
6. Fixed 1024-material limit; no defragmentation.
7. Hardcoded master shader paths in `get_master_shader_path()`.
8. Missing texture throws (no graceful fallback).

### Integration points

- **Asset system**: `MaterialInstance : IAsset`, hot-reload aware.
- **Drawing**: `DrawLocal.cpp` uses `matman.get_mat_shader()` + `matman.get_gpu_material_buffer()`.
- **Components**: `MeshComponent`, `DecalComponent`, `ParticleComponent` hold material refs.
- **Lua**: param setters + dynamic-mat alloc/free.
