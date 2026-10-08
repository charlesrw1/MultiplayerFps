# External C++ game projects

Issue: [#26](https://github.com/charlesrw1/MultiplayerFps/issues/26)

## Goal

Create, build, and run a C++ game outside the CsRemake repository. The game owns its source, Lua scripts, assets, generated files, and executable outputs. Creating a game must not edit the engine worktree.

## Project contract

An external project contains `game.json`, `Source/`, `Content/`, and `Tests/`. `game.json` defines the project name and engine checkout. Paths in the manifest are relative to the manifest unless they are absolute.

The first implementation statically links game code into project-specific editor and game executables. It does not add DLL modules, hot reload, packaging, or a stable plugin ABI.

## Changes

1. Remove direct dependencies from generic engine code to `MyGame` types.
2. Let reflection codegen scan an external game source tree while resolving engine types.
3. Parameterize the App target with `CsGameRoot`, `CsGameName`, and `CsTargetKind`.
4. Write executable and generated output under the external project's `Build/` directory.
5. Add `cscli new`, `build`, `editor`, and `run` project commands.
6. Generate a runtime `.ini` from `game.json` as a compatibility layer for the current config system.
7. Test project creation and build an out-of-tree project.

## Acceptance criteria

- `cscli new Asteroids --path D:/Gamedev/Asteroids` creates a complete project.
- `cscli build --project D:/Gamedev/Asteroids` compiles reflected C++ from that directory.
- `cscli editor --project D:/Gamedev/Asteroids` launches a project-local editor executable.
- `cscli run --project D:/Gamedev/Asteroids` launches the same game without editor mode.
- Lua and assets resolve under `D:/Gamedev/Asteroids/Content`.
- The engine worktree remains unchanged after external project creation and building.
