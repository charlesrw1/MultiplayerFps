+++
order = -3
+++
# Command Line Bridge

`cscli.exe` lets you run commands and Lua on a running editor or game instance.

```
cscli - CLI for driving a running CsRemake editor/game instance over AgentBridge

Usage: cscli [global options] <subcommand> [args...]

Subcommands:
  status                     Show mode/pid/project_base/port of the connected instance
  eval "<lua code>"          Execute Lua against the running instance's Lua state
  command [name] [args...]   Run a console command; omit name to list all; add --help to describe one
  instances                  List cscli-discoverable running instances (editor and/or game)
  log [--lines N]            Print the tail of the connected instance's engine log (default 200)

Global options:
  --format <human|json|tsv|ndjson>  Output format (default: human)
  --timeout <seconds>                Connect/response timeout (default: 10)
  --port <n>  --pid <n>  --mode <editor|game>
                                      Disambiguate when more than one instance is running
  --quiet                             Suppress "connecting to..." status lines
  --verbose                           On failure, also print the raw JSON error response
  -h, --help                          Show this help
  -V, --version                       Show cscli's version

Examples:
  cscli status
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
2. Wait until the process is running. Use `tasklist` if you need to confirm.
3. Run `cscli status` to connect.

Do not call `cscli` immediately after launch.

See [[scripting/lua_cookbook]] for `eval` examples.
