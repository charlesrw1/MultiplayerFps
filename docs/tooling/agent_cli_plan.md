+++
title = "Agent CLI implementation plan"
description = "Implementation contract for the typed cscli inspect-act-verify workflow."
+++

# Agent CLI implementation plan

## Goal

Turn `cscli` from a small remote console into a self-describing agent control surface. An agent must be able to discover typed operations, select the correct process, inspect editor state, make safe document changes, capture the rendered result, and wait for the engine to become ready.

## This change

1. Add metadata to every AgentBridge command: description, availability, side-effect level, input schema, and output schema.
2. Add bridge discovery commands that return the complete command catalog or one command descriptor.
3. Add `cscli tools`, `cscli call`, and `cscli wait-ready`. `call` invokes any AgentBridge command with a JSON object, so new bridge commands do not require a new hard-coded CLI subcommand.
4. Enrich status and instance discovery with project name, project file, readiness, current level, current editor document, document dirty state, and frame number. Add `--project` instance targeting.
5. Add read-only scene hierarchy inspection.
6. Add editor document save and undo commands through `IEditorApi2`. These operations use the editor's existing command and document APIs.
7. Add live composite-frame capture to a caller-selected PNG path.
8. Document the machine-readable output and add focused tests for descriptor serialization and CLI argument conversion where practical.

## Command surface

```text
cscli [--project <name|file|data-dir>] tools [name]
cscli [target options] call <name> [--args <json-object>]
cscli [target options] wait-ready [--wait-timeout <seconds>]

cscli call scene_hierarchy
cscli call editor_save
cscli call editor_undo
cscli call capture_frame --args '{"path":"TestFiles/screenshots/live.png"}'
```

`cscli eval`, `command`, `status`, `instances`, and `log` remain compatible.

## Safety rules

- The bridge remains loopback-only.
- Command descriptors identify `read_only` and `mutating` operations.
- Editor commands fail with a structured error when no editor document is active.
- Save and undo go through the editor API. They do not mutate editor internals directly.
- Scene hierarchy inspection is read-only and returns handles that can be used in later commands.
- Frame capture rejects an empty path and creates only the requested parent directories.
- `call` accepts a JSON object only. It does not construct a shell or console command string.

## Non-goals

- Entity creation and reflected property mutation. These require a transactional editor-command API and will follow this foundation.
- Redo. The current `UndoRedoSystem` deletes commands as it undoes them and has no redo history. Adding a correct redo model is a separate editor change.
- Detached jobs and cancellation.
- Starting builds and tests from `cscli`; the existing PowerShell entry points remain authoritative for now.
- Packaging, authentication, cloud services, or source-control commands.
- Remote-network access.

## Acceptance checks

1. The solution builds in Debug x64.
2. Unit tests pass, apart from documented pre-existing failures.
3. `cscli --help` documents the new commands and targeting option.
4. A running instance exposes a typed catalog through `cscli tools --format json`.
5. `cscli call ping` succeeds without a dedicated CLI wrapper.
6. `cscli wait-ready` returns success only after status reports `ready`.
7. Editor save and undo reject game-mode instances and work through the editor API.
8. `capture_frame` writes a PNG and reports its dimensions and resolved path.
