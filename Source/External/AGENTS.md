# External Module

Third-party libraries bundled in-tree. **Do not read source files here** — use this file as the substitute.

## Libraries

Graphics / UI:
- **glad** — OpenGL function loader. Must call `gladLoadGLLoader(...)` once after GL context creation, before any GL call.
- **imgui** — Dear ImGui immediate-mode GUI (core + demo + tables + widgets). Requires both the OpenGL3 and SDL3 backends initialized.
- **imgui_impl_opengl3**, **imgui_impl_sdl3** — ImGui backends for our GL3 + SDL3 setup.
- **ImGuizmo** — In-viewport translate/rotate/scale gizmos; used by LevelEditor.
- **ImSequencer** — Timeline widget; used for animation editing.
- **imnodes** — Node-graph widget; used for the animation graph UI.

Asset formats:
- **cgltf** (parser + writer) — Sole GLTF/GLB reader/writer. AssetCompile owns all GLTF I/O.
- **stb_image** / **stb_image_write** — PNG/JPG/BMP/TGA load + save.
- **tinyexr** — OpenEXR HDR images.
- **miniz** — ZIP compression.

Utilities:
- **json** (nlohmann) — Serialization and config.
- **tracy** — Frame/zone profiler. Opt-in: define `TRACY_ENABLE` to activate; wrap with `ZoneScoped` / `FrameMark`.
- **KHR/** — Khronos extension headers required by cgltf.

Build artifacts:
- **x64/** — Prebuilt 64-bit Windows binaries/libs.
