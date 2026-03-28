# External Module

Third-party libraries. **Do not read source files in this directory** — use only this AGENTS.md for context.

## Libraries

### Graphics & Rendering
- **glad** (`glad.c`, `glad/`) — OpenGL function loader; must be initialized before any GL calls
- **imgui** (`imgui.*`) — Dear ImGui immediate-mode GUI; core, demo, tables, draw list, widgets
- **imgui_impl_opengl3** — ImGui OpenGL 3.x backend
- **imgui_impl_sdl2** — ImGui SDL2 platform backend
- **ImGuizmo** (`ImGuizmo.*`) — 3D transform gizmos (translate/rotate/scale) in-viewport; used by LevelEditor
- **ImSequencer** (`ImSequencer.*`) — Timeline/sequencer widget for animation editing
- **imnodes** (`imnodes.*`) — Node graph editor widget; used for animation graph UI

### Asset Formats
- **cgltf** (`cgltf.h`, `cgltf_write.*`) — Single-header GLTF/GLB parser and writer; used by AssetCompile
- **stb_image** (`stb_image.h`) — Image loading (PNG, JPG, BMP, TGA, etc.)
- **stb_image_write** (`stb_image_write.h`) — Image saving
- **tinyexr** (`tinyexr.h`) — OpenEXR HDR image format support
- **miniz** (`miniz.c/h`) — ZIP compression/decompression

### Utilities
- **json** (`json.hpp`) — nlohmann/json; used for serialization and config
- **tracy** (`tracy/`) — Frame/zone profiler; wrap zones with `ZoneScoped` / `FrameMark`
- **KHR** (`KHR/`) — Khronos extension headers required by cgltf

### Build Artifacts
- **x64/** — Prebuilt binaries/libs for 64-bit Windows

## Usage Notes
- ImGui must be initialized with both the OpenGL3 and SDL2 backends before use
- tracy profiling is opt-in; define `TRACY_ENABLE` to activate
- cgltf is the sole GLTF parser; AssetCompile owns all GLTF reading/writing
- glad must be called once after OpenGL context creation: `gladLoadGLLoader(...)`
