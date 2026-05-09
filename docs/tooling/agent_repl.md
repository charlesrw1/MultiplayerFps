# Agent REPL Guide

TCP socket REPL for AI agents to drive the engine.

---

## Connecting

Listens on `127.0.0.1:9999` (loopback only) after `AgentREPL::inst->start()`. Single client; subsequent connects refused until disconnect.

```bash
nc 127.0.0.1 9999
```

On connect: `>>CONNECTED`.

---

## Command Reference

| Command | Effect | Response |
|---------|--------|----------|
| `cmd:<string>` | Run console command via `Cmd_Manager` (APPEND — runs next tick) | `>>OK` |
| `lua:<code>` | Execute Lua via `ScriptManager::reload_from_content` | output, then `>>OK` or `>>ERROR: msg` |
| `block` | Pause game loop | `>>BLOCKED` |
| `continue` | Resume from `block` | `>>OK` |
| `wait_ticks:<n>` | (blocked) run N ticks, re-pause | `>>OK` |
| `wait_time:<secs>` | (blocked) run T seconds, re-pause | `>>OK` |
| `resume` | Continue a `debug_break()` test coroutine | `>>OK` |

Every command terminates with exactly one sentinel (`>>OK`, `>>BLOCKED`, or `>>ERROR: ...`). Read until sentinel before sending next command.

---

## Output Forwarding

Forwarded to socket while connected: `Error`, `Warning`, `Info`, console command output (`LtConsoleCommand`). `Debug`-level suppressed.

For `lua:` commands, Lua `print()` is redirected to the socket for that command.

---

## Fire-and-Forget (engine keeps running)

`cmd:` or `lua:` without `block` — engine ticks normally.

```bash
printf 'cmd:r_reload_shaders\n' | nc 127.0.0.1 9999
# >>OK

printf 'lua:print(tostring(Player.inst))\n' | nc 127.0.0.1 9999
# table: 0x...
# >>OK
```

`cmd:` is APPEND — runs at start of next tick, output arrives on subsequent frames. For immediate output, follow with `block` + `wait_ticks:1`.

---

## Blocking — Pause the Engine

`block` freezes the loop. Engine does not tick until `continue`. Use to inspect stable state.

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

Any number of `cmd:`/`lua:` allowed while blocked.

---

## Waiting — Let the Engine Run While Blocked

`wait_ticks` / `wait_time` advance the engine a controlled amount, then re-pause.

```
block
>>BLOCKED
wait_ticks:60
>>OK                  ← ran 60 ticks, paused again
lua:print(game_time())
1.000
>>OK
wait_time:2.5
>>OK                  ← ran 2.5s, paused again
continue
>>OK
```

---

## Resuming a Test debug_break()

Integration tests can `co_await t.debug_break()` to pause mid-test. While paused:

- `block`/`continue`/`lua:`/`cmd:` work normally.
- `resume` advances the test coroutine past the `debug_break()`.

```
lua:print(enemy_count())
3
>>OK
resume
>>OK           ← test coroutine continues
```

---

## Scripting from Bash

Use `printf` (not `echo`). Read until sentinel or close.

```bash
# Single command
printf 'lua:print(Player.inst:get_health())\n' | nc 127.0.0.1 9999

# Keep connection open across multiple commands: -q 1
printf 'block\nlua:print(game_time())\ncontinue\n' | nc -q 1 127.0.0.1 9999

# Multi-line Lua via semicolons
printf 'lua:local h = Player.inst:get_health(); print(h > 0)\n' | nc 127.0.0.1 9999
```

For longer sessions, run `nc 127.0.0.1 9999` interactively.

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
`sys_print` output appears on the socket in the following frame.

---

## Error Handling

```
lua:this is not valid lua
>>ERROR: [string "agent_repl"]:1: <eof> expected near 'is'

foo:bar
>>ERROR: unknown command: foo:bar
```

If `ScriptManager` is not yet initialized:
```
>>ERROR: ScriptManager not available
```

---

## Notes

- **Loopback only** — non-localhost connections rejected at OS level.
- **Single connection** — second client refused.
- **Main thread** — commands run on engine main thread; no locking needed, `block` genuinely halts the loop.
- **`cmd:` APPEND** — safe for all commands; output arrives next tick. Use `block` + `wait_ticks:1` for immediate output.
