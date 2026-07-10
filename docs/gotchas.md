- Project has a unit tests project, already use it. It links all projects in solution.

- must call set_call_init_in_editor(true) if you want components to call start() in editor mode. must call set_ticking(true) to tick. editor_start() is called on all components. set_editor_model() should only be used as a shorthand way for an editor visualization. Lua components can get this automatically via `---editor, init_in_editor` on the `---@class` block instead of calling set_call_init_in_editor(true) themselves.

- renderer for opengl uses reverse-Z and infinite Z projection matrix

- gpu driven rendering (occlusion, lod, frustum) for opaque objects. Occlusion culling splits the fast-path draw into two separate GPU-culled batches

- RenderWindow's ortho projection is built from get_vp_rect().get_size() only, with no offset — meaning window.draw()
  rect coordinates are viewport-local (0,0 = top-left of the scene viewport), not window-local

- animation import's trailing-duplicate-frame strip (ModelCompile_Animation.cpp) must be a single near-bit-exact
  check, not a loop with a loose tolerance — a loose/looped check chews into real frames on eased/decelerating
  loops (e.g. idle sway), truncating the clip and causing a snap at the loop point
  
 - shader cache. if you change a header that is included in a shder, engine doesnt pick up on that and loads stale shader from cache. it does pick up on the source file changes, but not includes.
 
 - DOMAIN Particle for meshbuilder rendering
 
 - parenting only is in prefab mode, not in level edit mode. instantiating a prefab with parenting removes the parenting.

 - VS incremental builds get broken (every rebuild recompiles everything, no source changes) whenever a CLI/agent build alternates with the VS IDE: PROVEN ROOT CAUSE is a `/errorReport` mismatch in the CL command line. VS sets `BuildingInsideVisualStudio=true` -> CL gets `/errorReport:prompt`; a bare `msbuild` sets it false -> `/errorReport:queue`. The CL task hashes the full command line into `CL.command.1.tlog`; when it changes, CL recompiles EVERY file. So each tool switch (IDE<->CLI) triggers a full rebuild, and it looks like it "never settles" if CLI builds keep happening between IDE builds. (Confirmed from a VS Diagnostic build log: every CL line ended `/errorReport:prompt`; CLI ended `/errorReport:queue` — otherwise identical.)
   Fixes (all applied):
   1. Pin the flag in the project so IDE and CLI always match: `<ItemDefinitionGroup><ClCompile><ErrorReporting>Prompt</ErrorReporting></ClCompile></ItemDefinitionGroup>` in CsRemake.vcxproj (near the Microsoft.Cpp.targets import). Verify: a bare `msbuild.cmd Source/CsRemake.vcxproj ... -v:diag | grep errorReport` should now show `prompt`. (Other projects — App etc. — need the same pin to be fully immune.)
   2. Use the 64-bit MSBuild everywhere (`...\Bin\amd64\MSBuild.exe`, via vswhere `-find MSBuild\**\Bin\amd64\MSBuild.exe`) so the engine/FileTracker also match the IDE. Fixed in msbuild.cmd + build_and_test.ps1 + integration_test.ps1 + _vs_attach.ps1.
   3. Wrapper scripts also pass `/p:BuildingInsideVisualStudio=true` (independently yields prompt).
   Reset after poisoning: delete every `*.tlog` dir under `x64\Debug\intermediate\` (regenerates; one full rebuild, then incremental returns).
   Note: phantom ClInclude entries were investigated and are NOT the cause — don't chase them for this symptom.