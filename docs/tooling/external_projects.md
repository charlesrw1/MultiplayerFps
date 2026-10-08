# External C++ projects

An external game keeps its code and content outside the engine checkout. CsRemake builds the engine libraries and statically links the project's C++ sources into project-local executables.

## Create a project

```powershell
cscli new Asteroids --path D:/Gamedev/Asteroids
```

The command creates this layout:

```text
Asteroids/
  game.json
  Source/
  Content/
    Scenes/Main.tmap
    scripts/main.lua
  Tests/
  Build/
```

`game.json` is the source of project configuration:

```json
{
  "name": "Asteroids",
  "engine": "C:/path/to/CsRemake",
  "startupScene": "Scenes/Main.tmap"
}
```

The name must be a valid C++ identifier. Relative engine paths resolve from `game.json`.

## Build and run

Run commands from the project directory, or pass `--project`:

```powershell
cscli build
cscli editor --wait-ready
cscli run
```

```powershell
cscli build --project D:/Gamedev/Asteroids --target editor
cscli editor --project D:/Gamedev/Asteroids --no-build
```

`build` accepts `--target editor`, `--target game`, or `--target all`. The default is `all`. It accepts any engine build configuration through `--configuration`; the default is `Debug`.

Outputs are under the project:

```text
Build/bin/x64/Debug/AsteroidsEditor.exe
Build/bin/x64/Debug/Asteroids.exe
Build/generated/ExternalGame.gen.cpp
Build/generated/lua/game_lua_stubs.lua
```

The generated `Build/project.ini` adapts `game.json` to the current runtime configuration system. Do not edit it.

## C++ and content

Every `.cpp` and `.h` below `Source/` is part of the game target. Reflection codegen scans the external headers and resolves their base types against the engine checkout. Add source files without editing a Visual Studio project.

The runtime uses `Content/` as `g_project_base`. Lua files therefore use the existing `Content/scripts/` convention. Engine-owned assets fall back to the engine asset root loaded from `EngineVars.ini`.

This initial workflow uses static linking. C++ changes require a rebuild and process restart.
