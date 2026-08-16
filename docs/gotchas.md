# Gotchas

- The project has a unit tests project. Use it for new tests. It links every project in the solution.

- To run `start()` on a component in the editor, call `set_call_init_in_editor(true)`. To run `update()` each frame, call `set_ticking(true)`. The engine always calls `editor_start()` on every component. Use `set_editor_model()` only as a shorthand for an editor visualization. Lua components can set `init_in_editor` on the `---@class` block instead of calling `set_call_init_in_editor(true)` directly.

- The OpenGL renderer uses reverse-Z and an infinite-Z projection matrix.

- GPU-driven rendering (occlusion, LOD, frustum culling) applies to opaque objects. Occlusion culling splits the fast-path draw into two GPU-culled batches.

- `RenderWindow`'s ortho projection uses only `get_vp_rect().get_size()`, with no offset. So `window.draw()` rect coordinates are viewport-local: `(0,0)` is the top-left of the scene viewport, not the top-left of the window.

- The animation importer's trailing-duplicate-frame strip (`ModelCompile_Animation.cpp`) must use a single near-bit-exact check. Do not use a loop with a loose tolerance. A loose or looped check removes real frames from eased or decelerating loops, such as idle sway. This truncates the clip and causes a snap at the loop point.

- Shader cache: if you change a header included by a shader, the engine does not detect the change and loads the stale shader from cache. The engine does detect changes to the shader source file itself.

- `DOMAIN Particle` exists for MeshBuilder rendering.

- Parenting works only in prefab mode, not in level-edit mode. Instantiating a prefab removes its parenting.

- Codegen.py script is wonky. If you are using multiple VCPROJ, it needs special handling to generate a .gen.cpp for each.

- CLI builds and VS IDE builds can break VS incremental builds. When they alternate, every rebuild recompiles the whole codebase, even with no source changes.

  **Root cause:** an `/errorReport` mismatch in the CL command line. The VS IDE sets `BuildingInsideVisualStudio=true`, so CL gets `/errorReport:prompt`. A bare `msbuild` call sets this to false, so CL gets `/errorReport:queue`. The CL task hashes the full command line into `CL.command.1.tlog`. When the hash changes, CL recompiles every file. So each switch between IDE and CLI triggers a full rebuild. Confirmed from a VS Diagnostic build log: every CL line from the IDE ended in `/errorReport:prompt`; every CLI line ended in `/errorReport:queue`. Otherwise the lines were identical.

  **Fixes (already applied):**
  1. Pin the flag in the project, so the IDE and CLI always match. In `Core.vcxproj`, near the `Microsoft.Cpp.targets` import, add:
     ```xml
     <ItemDefinitionGroup><ClCompile><ErrorReporting>Prompt</ErrorReporting></ClCompile></ItemDefinitionGroup>
     ```
     To verify, run `msbuild.cmd Source/Core.vcxproj ... -v:diag | grep errorReport`. It should show `prompt`. Other projects, such as `App`, need the same pin to be fully immune.
  2. Use the 64-bit MSBuild everywhere: `...\Bin\amd64\MSBuild.exe`, found via `vswhere -find MSBuild\**\Bin\amd64\MSBuild.exe`. This keeps the engine and FileTracker in sync with the IDE. Applied in `msbuild.cmd`, `build_and_test.ps1`, `integration_test.ps1`, and `_vs_attach.ps1`.
  3. The wrapper scripts also pass `/p:BuildingInsideVisualStudio=true`. This independently produces `prompt`.

  **To reset a poisoned build:** delete every `*.tlog` directory under `x64\Debug\intermediate\`. The tlogs regenerate. One full rebuild follows, then incremental builds work again.

  **Note:** phantom `ClInclude` entries were investigated. They are not the cause. Do not chase them for this symptom.
