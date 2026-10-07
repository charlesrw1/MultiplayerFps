+++
order = -3
+++
# Command Line Bridge

`cscli.exe` is the agent-facing control surface for a running editor or game. It discovers live instances, exposes typed AgentBridge tools, runs console commands and Lua, and returns structured output.

```
cscli - CLI for driving a running CsRemake editor/game instance over AgentBridge

Usage: cscli [global options] <subcommand> [args...]

Subcommands:
  status                     Show process, project, document, level, and readiness state
  wait-ready                 Wait until the selected instance reports ready
  tools [name]               List typed bridge tools or describe one
  call <name> [--args JSON]  Invoke any typed bridge tool
  eval "<lua code>"          Execute Lua against the running instance's Lua state
  command [name] [args...]   Run a console command; omit name to list all; add --help to describe one
  instances                  List cscli-discoverable running instances (editor and/or game)
  log [--lines N]            Print the tail of the connected instance's engine log (default 200)

Global options:
  --format <human|json|tsv|ndjson>  Output format (default: human)
  --timeout <seconds>                Connect/response timeout (default: 10)
  --port <n>  --pid <n>  --mode <editor|game>  --project <name|file|data-dir>
                                      Disambiguate when more than one instance is running
  --quiet                             Suppress "connecting to..." status lines
  --verbose                           On failure, also print the raw JSON error response
  -h, --help                          Show this help
  -V, --version                       Show cscli's version

Examples:
  cscli status
  cscli wait-ready --wait-timeout 30
  cscli tools
  cscli tools scene_hierarchy
  cscli call scene_hierarchy
  cscli call capture_frame --args "{\"path\":\"TestFiles/screenshots/live.png\"}"
  cscli eval "1+1"
  cscli command give_weapon rifle 30
  cscli command give_weapon --help
  cscli command
  cscli instances
  cscli log --lines 50
```

## Connect to a Running Instance

The engine can take about 10 seconds to start. The first run is slower.

1. Launch the editor or game. See [[tooling/run-scripts]].
2. Run `cscli wait-ready --wait-timeout 60`. This polls structured status and succeeds only when the instance is ready.
3. Run `cscli status` to inspect the selected instance.

Use `--project`, `--pid`, or `--port` when more than one instance is running. `--project` matches the project name, selected project ini, or project data directory.

## Typed Tools

`cscli tools` returns every AgentBridge operation with its availability, side-effect level, input schema, and output schema. Use `cscli tools <name>` before invoking an unfamiliar tool.

`cscli call` accepts one JSON object. It calls the bridge command directly; it does not concatenate or execute a console command string.

```powershell
cscli tools capture_frame
cscli call capture_frame --args '{"path":"TestFiles/screenshots/live.png"}'
cscli --format json call scene_hierarchy
cscli --mode editor call editor_save
cscli --mode editor call editor_undo
```

Built-in typed tools include:

| Tool | Availability | Effect | Purpose |
|---|---|---|---|
| `status` | any | read-only | Process, project, readiness, level, document, and frame state |
| `scene_hierarchy` | any | read-only | Stable entity/component handles and world transforms |
| `capture_frame` | any | read-only | Save the latest composited frame to a PNG |
| `editor_save` | editor | mutating | Save through the active editor document API |
| `editor_undo` | editor | mutating | Undo through the active editor document API |
| `eval_lua` | any | mutating | Evaluate Lua and return JSON-compatible values |
| `run_command` | any | mutating | Execute an engine console command |
| `export_tmap` | any | read-only | Export map or prefab meshes to glTF |

## Output Contract

Use `--format json` for automation. A successful bridge call returns `{"ok":true,"result":...}`. A command failure returns `{"ok":false,"error":"..."}` and exit code 1. Transport or discovery failure uses exit code 3. Invalid CLI input uses exit code 2.

`tools` also supports TSV and NDJSON. Human output is the default.

See [[scripting/lua_cookbook]] for `eval` examples.
