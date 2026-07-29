+++
order = -3
+++

# Command Line Bridge

This covers cscli.exe, which is a tool that agents can use to access the running game/editor and run commands on it.

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

## Notes

- engine can take ~10sec to spin up (espically first time runs), so don't try to connect to cscli immeditaely after launching, check tasklist
 