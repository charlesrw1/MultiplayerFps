# Material System

## Overview

The material system handles loading, compiling, and managing shader materials for rendering. It supports master materials, material instances, dynamic materials, and hot reloading.

## Key Files

- `Source/Render/MaterialPublic.h` — public API (`MaterialInstance`, `DynamicMatUniquePtr`)
- `Source/Render/MaterialLocal.h` — internal class declarations
- `Source/Render/MaterialLocal.cpp` — all implementation (~1200 lines)
- `Source/Render/DynamicMaterialPtr.h` — custom deleter for dynamic material unique_ptr
- `Data/*.mm` — master material files
- `Data/*.mi` — material instance files
- `Shaders/MASTER/` — master shader templates

## Class Hierarchy

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

## Material File Formats

### Master Material (`.mm`)
```
TYPE MaterialMaster
OPT <OptionType> <value>     # BlendMode, LightingMode, AlphaTested, ShowBackfaces, etc.
VAR <type> <name> <default>  # texture2D, float, float_vec4, vec4, bool
DOMAIN <type>                # Default|PostProcess|Terrain|Decal|UI|Particle
_VS_BEGIN ... _VS_END        # vertex shader code
_FS_BEGIN ... _FS_END        # fragment shader code
```

### Material Instance (`.mi`)
```
TYPE MaterialInstance
PARENT <path/to/master.mm>
VAR <parameter_name> <value>  # override defaults
```

## Loading Flow

1. `MaterialImpl::load_from_file()` — detects `.mm` vs `.mi` by extension
2. **Master**: parses VAR/OPT/DOMAIN/shader blocks via `DictParser`, assigns param offsets, packs into ≤64 bytes
3. **Instance**: loads parent master (recursive), copies defaults, overrides from file
4. `post_load()` — assigns unique material ID, adds to dirty_list
5. `AllMaterialTable::register_material()` — allocates 64-byte GPU buffer slot

## GPU Buffer Layout

- Each material occupies a **64-byte slot** in a shared SSBO
- Max **1024 concurrent materials** (`MAX_MATERIALS`)
- Floats: 4 bytes, Float vec4: 16 bytes, Color32: 4 bytes
- Textures: stored separately in texture bindings array (not in 64-byte slot)
- Parameters sorted by type for alignment during load

## Shader Compilation

- Only in `EDITOR_BUILD`
- `MasterMaterialImpl::create_glsl_shader()` generates `.glsl` from master template + material code
- Injects texture `layout(binding=N)`, replaces param refs with SSBO buffer reads
- Handles `#include` recursively (no cycle detection — risk of infinite loop)
- Runtime: `MaterialManagerLocal::compile_mat_shader()` compiles `.glsl` on demand, cached by `shader_key`
- `shader_key` = 32-bit: material ID (23 bits) + MSF flags (9 bits)

## Dynamic Materials

- Created via `MaterialManagerLocal::create_dynmaic_material_unsafier()` (note typo in name)
- Copies parent's master ref + all param values
- Wrapped in `DynamicMatUniquePtr` (custom deleter calls `free_dynamic_material()`)
- Pool-based reuse via `DynamicMaterialAllocator`
- Exposed to Lua: `alloc_dynamic_mat` / `free_dynamic_mat`

## Material Reloading

- Triggered via asset system hot reload
- `MaterialInstance::move_construct()` re-parses file, finds dependent instances, reloads them
- `MaterialManagerLocal::on_reloaded_material()` triggers scene rebuild via `BuildSceneData_CpuFast`

## Parameter Setting (Runtime)

```cpp
mat->set_float_parameter(name, f);
mat->set_floatvec_parameter(name, vec4);
mat->set_u8vec_parameter(name, Color32);
mat->set_tex_parameter(name, texture);
```

Each call marks the material dirty. Upload happens in `pre_render_update()`.

## Known Issues / Brittleness

1. **`pre_render_update()` is commented out** in `DrawLocal.cpp:3579` — GPU param uploads never happen
2. `DictParser` has no error recovery — any parse error throws `MasterMaterialExcept`
3. Shader `#include` expansion has no cycle detection — circular includes = infinite loop
4. No runtime validation that params fit in 64-byte layout (only checked at load time)
5. `move_construct()` comment: "FIXME: unsafe for materials already referencing us"
6. Fixed 1024-material limit with no defragmentation
7. Hardcoded master shader template paths in `get_master_shader_path()`
8. No graceful degradation on missing textures — throws exception

## Integration Points

- **Asset system**: `MaterialInstance` extends `IAsset`, supports hot reload
- **Drawing**: `DrawLocal.cpp` calls `matman.get_mat_shader()` and `matman.get_gpu_material_buffer()`
- **Components**: `MeshComponent`, `DecalComponent`, `ParticleComponent`, etc. hold material refs
- **Lua**: param setters and dynamic mat alloc/free bound to script

## TODO (from TODO.md)

Unit tests and integration tests needed for:
- Master material file parsing (valid and invalid inputs)
- Material instance loading (parent resolution, param overrides)
- Dynamic material lifecycle (create, set params, free, pool reuse)
- Material reloading behavior
- Refactor toward cleaner error handling and re-enable `pre_render_update()`
