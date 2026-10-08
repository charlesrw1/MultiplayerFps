# External C++ projects

An external game keeps its code and content outside the engine checkout. CsRemake builds the engine libraries and statically links the project's C++ sources into project-local executables.

## Create a project

```powershell
cscli new Asteroids --path D:/Gamedev/Asteroids
```

The command creates this layout:

```text
Asteroids/
  Asteroids.sln
  Asteroids.vcxproj
  game.json
  Source/
  Content/
    Scenes/Main.tmap
    scripts/main.lua
    eng/                    default engine assets and starter scene dependencies
    Models/
    Textures/
  Tests/
  Build/
  x64/                    generated, gitignored
```

`game.json` is the source of project configuration:

```json
{
  "name": "Asteroids",
  "engine": "C:/path/to/CsRemake",
  "applicationClass": "DefaultGameApplication"
}
```

The name must be a valid C++ identifier. Relative engine paths resolve from `game.json`.
Set `applicationClass` to a reflected `Application` subclass in `Source/` when the game needs a
custom update loop, such as a Canvas2d game.

`cscli new` copies a curated runtime asset bundle: default engine assets, shared renderer
materials/shaders, UI fonts, and starter sphere/cube models. It then uses the template scene for
`Content/Scenes/Main.tmap`. The engine's 1.7 GiB sample library and its all-encompassing bundle
are deliberately excluded, so a fresh project stays small and does not try to load unrelated
sample assets.

Open `Asteroids.sln` in Visual Studio for the normal C++ workflow. The project displays
the game sources and content, and its Debug/Release x64 **Build** command invokes the
engine's external-game build with this project as its input. The generated editor executable
is also selected as the debugger command.

## Build and run

Run commands from the project directory, or pass `--project`:

```powershell
cscli build
cscli editor --wait-ready
cscli run
cscli test
```

```powershell
cscli build --project D:/Gamedev/Asteroids --target editor
cscli editor --project D:/Gamedev/Asteroids --no-build
cscli build --project D:/Gamedev/Asteroids --restart-running
```

`build` accepts `--target editor`, `--target game`, `--target test`, or `--target all`. The default is `all`. `cscli test` builds the test target (unless `--no-build` is supplied), compiles `Tests/**/*.cpp`, and runs it with `--tests editor`. Use `--mode game` for `GAME_TEST` cases (the default is `editor`), and pass `--pattern <glob>` more than once to select a subset. Project Lua tests belong under `Content/scripts/tests/`; the copied `integration_test_framework.lua` registers them. Both accept any engine build configuration through `--configuration`; the default is `Debug`.

Outputs are under the project:

```text
x64/Debug/App.exe
x64/Debug/intermediate/App/
Build/generated/ExternalGame.gen.cpp
Build/generated/lua/game_lua_stubs.lua
```

`App.exe` is self-launching when run directly from this generated layout: it reads the adjacent
`game.json`, changes to the configured engine root, and applies `Build/project.ini`. Explicit
`--engine-root` and `--project` arguments always take precedence. Engine-owned builds are unchanged
because they have no adjacent `game.json`.

Runtime artifacts stay with an external project: logs are written to `Logs/output.log`, compiled
shader binaries to `Cache/ShaderCache/`, and editor layouts/configuration to `Config/Editor/`.
`cscli new` seeds `Config/Editor/` from the engine's `Cfg/` directory, including the default
`imgui.ini` and `editor.ini`; subsequent editor changes remain local to that game.

The generated `Build/project.ini` adapts `game.json` to the current runtime configuration system. Do not edit it.

Use `--restart-running` after changing native C++ while this project's editor or game is open. It
closes matching instances, rebuilds the one `App.exe`, then restores the modes that were open. The
normal `build` command deliberately does not close anything; the restart option is explicit because
it may need to force-close an unresponsive process and therefore discard unsaved editor work.

## C++ and content

Every `.cpp` and `.h` below `Source/` is part of the game target. Reflection codegen scans the external headers and resolves their base types against the engine checkout. Add source files without editing the generated Visual Studio project.

The runtime uses `Content/` as `g_project_base`. Lua files therefore use the existing `Content/scripts/` convention. Engine-owned assets fall back to the engine asset root loaded from `EngineVars.ini`.

This initial workflow uses static linking. C++ changes require a rebuild and process restart; use
`cscli build --restart-running` to perform both steps in one command.
