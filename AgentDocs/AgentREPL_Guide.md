# Agent REPL Guide

How to connect to and use the engine's TCP socket REPL as an AI agent.

---

## Connecting

The engine listens on `127.0.0.1:9999` (loopback only) once `AgentREPL::inst->start()` has been called. You can connect with `nc` (netcat):

```bash
nc 127.0.0.1 9999
```

On connect you receive:
```
>>CONNECTED
```

Only one client at a time is accepted. The connection stays open until you disconnect.

---

## Command Reference

| Command | Effect | Response |
|---------|--------|----------|
| `cmd:<string>` | Run a console command via `Cmd_Manager` (APPEND mode — executes next tick) | `>>OK` |
| `lua:<code>` | Execute a Lua string via `ScriptManager::reload_from_content` | output lines, then `>>OK` or `>>ERROR: msg` |
| `block` | Pause the engine game loop | `>>BLOCKED` |
| `continue` | Resume from a `block` | `>>OK` |
| `wait_ticks:<n>` | (while blocked) let engine run N ticks, then re-pause | `>>OK` when re-paused |
| `wait_time:<secs>` | (while blocked) let engine run T seconds, then re-pause | `>>OK` when re-paused |
| `resume` | Signal a `debug_break()` test coroutine to continue | `>>OK` |

**Every command ends with exactly one sentinel line** (`>>OK`, `>>BLOCKED`, or `>>ERROR: ...`). Always read until you see a sentinel before sending the next command.

---

## Output Forwarding

While connected, the following log output is forwarded to your socket automatically:
- `Error` messages
- `Warning` messages
- `Info` messages
- Console command output (`LtConsoleCommand`)

`Debug`-level messages are suppressed (too noisy).

For `lua:` commands, Lua's `print()` function is redirected to the socket for the duration of the command.

---

## Fire-and-Forget (engine keeps running)

Use `cmd:` or `lua:` without `block`. The engine continues ticking normally.

```bash
# Run a console command
printf 'cmd:r_reload_shaders\n' | nc 127.0.0.1 9999
# → >>OK

# Execute Lua and read back immediate output
printf 'lua:print(tostring(Player.inst))\n' | nc 127.0.0.1 9999
# → table: 0x...
# → >>OK
```

Note: `cmd:` uses APPEND mode — the command executes at the start of the **next** game loop tick, not immediately. Any output from that command will appear on the socket via the log sink in subsequent frames.

---

## Blocking — Pause the Engine

Send `block` to freeze the game loop. While blocked, the engine does not tick. Use this to inspect stable state.

```
block
>>BLOCKED
lua:print(Player.inst:get_position())
(1.0, 0.0, 3.5)
>>OK
lua:print(World.inst:entity_count())
42
>>OK
continue
>>OK
```

You can issue as many `cmd:` and `lua:` commands as you like while blocked. Send `continue` when done.

---

## Waiting — Let the Engine Run While Blocked

Use `wait_ticks` or `wait_time` inside a blocked session to advance the engine a controlled amount, then re-pause automatically.

```
block
>>BLOCKED
lua:print(game_time())
0.000
>>OK
wait_ticks:60
>>OK                  ← engine ran 60 ticks, now paused again
lua:print(game_time())
1.000
>>OK
continue
>>OK
```

```
block
>>BLOCKED
wait_time:2.5
>>OK                  ← engine ran 2.5 seconds, now paused again
lua:print(game_time())
2.500
>>OK
continue
>>OK
```

---

## Resuming a Test debug_break()

Integration tests can call `co_await t.debug_break()` to pause mid-test and open the REPL. While a test is paused:

- `block` / `continue` / `lua:` / `cmd:` all work normally for inspection
- `resume` tells the test coroutine to continue past the `debug_break()` point

```
block
>>BLOCKED
lua:print(enemy_count())
3
>>OK
continue
>>OK
resume
>>OK           ← test coroutine now continues
```

---

## Scripting from Bash

Use `printf` (not `echo`) to send multi-line sessions. Read until the connection closes or you see the final sentinel.

```bash
# Single command
printf 'lua:print(Player.inst:get_health())\n' | nc 127.0.0.1 9999

# Block, inspect, continue — keep connection open with -q flag or use a here-doc
printf 'block\nlua:print(game_time())\ncontinue\n' | nc -q 1 127.0.0.1 9999

# Multi-line Lua (semicolons as statement separators)
printf 'lua:local h = Player.inst:get_health(); print(h > 0)\n' | nc 127.0.0.1 9999
```

For longer sessions, open an interactive connection:
```bash
nc 127.0.0.1 9999
```
Then type commands manually, reading sentinels as they arrive.

---

## Common Patterns

### Check a value
```
lua:print(some_global_var)
```

### Call a function and inspect result
```
lua:local result = some_system:do_thing(); print(tostring(result))
```

### Wait for a specific game state
```
block
>>BLOCKED
wait_ticks:10
>>OK
lua:print(level_loaded())
false
>>OK
wait_ticks:30
>>OK
lua:print(level_loaded())
true
>>OK
continue
>>OK
```

### Trigger a console command and observe log output
```
cmd:dump_entity_list
>>OK
```
Any `sys_print` output from that command will appear on the socket in the following frame.

---

## Error Handling

Lua errors return `>>ERROR: <message>` instead of `>>OK`:

```
lua:this is not valid lua
>>ERROR: [string "agent_repl"]:1: <eof> expected near 'is'
```

Unknown command prefixes also return an error:
```
foo:bar
>>ERROR: unknown command: foo:bar
```

If the engine is not available (e.g. ScriptManager not yet initialized), `lua:` returns:
```
>>ERROR: ScriptManager not available
```

---

## Notes

- **Loopback only** — connections from other machines are rejected at the OS level.
- **Single connection** — a second client is refused until the first disconnects.
- **Main thread** — all commands execute on the engine main thread. No locking needed, but `block` genuinely halts the game loop.
- **`cmd:` is APPEND mode** — safe for all commands, but output arrives on the next tick rather than immediately. If you need to see output right away, follow with `block` + `wait_ticks:1`.
