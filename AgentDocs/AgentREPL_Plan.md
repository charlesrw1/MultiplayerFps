# Agent REPL — Design & Implementation Plan

## Goal

Replace the file-based `LuaDebugServer` with a proper TCP socket REPL that allows an AI agent (or any client) to issue commands to the running engine and receive responses in real time.

---

## What Gets Removed

- `Source/IntegrationTests/LuaDebugServer.h`
- `Source/IntegrationTests/LuaDebugServer.cpp`
- Any call sites using `LuaDebugServer::on_enter()` / `LuaDebugServer::poll()`
- The `TestFiles/debug/` file protocol

---

## New Component: `AgentREPL`

### Location
`Source/Framework/AgentREPL.h` / `AgentREPL.cpp`

### Responsibilities
- Listen on `127.0.0.1:<port>` (loopback only — no external connections possible)
- Accept one client connection at a time
- Non-blocking `poll()` for use in the game loop
- Blocking `wait()` for use in test coroutines / debug break scenarios
- Execute received commands via `Cmd_Manager` or `ScriptManager`
- Capture and return output back to the client over the same socket
- Terminate response with a sentinel so the client knows output is complete

---

## Protocol

Line-based text over TCP.

### Command types

| Command | Action |
|---------|--------|
| `cmd:<string>` | `Cmd_Manager::execute(APPEND, string)` — engine keeps running |
| `lua:<code>` | `ScriptManager::reload_from_content(code)` with print captured — engine keeps running |
| `block` | Suspend the engine coroutine. Engine pauses until `continue` is received. Sends `>>BLOCKED\n` to confirm. |
| `continue` | Resume a blocked coroutine. Engine resumes running. Sends `>>OK\n`. |
| `wait_ticks:<n>` | Block, let engine run for N ticks, then pause again. Sends `>>OK\n` when paused. |
| `wait_time:<seconds>` | Block, let engine run for T seconds, then pause again. Sends `>>OK\n` when paused. |

### Default behavior: fire-and-forget

Commands (`cmd:`, `lua:`) execute and the engine immediately continues — `>>OK` is returned right away. Output from that command (print, log) is sent back before `>>OK`.

To capture output that happens _over time_ (e.g. something that prints next frame), send `block` first, then the command, then `continue`.

### Examples

```
# Fire-and-forget — engine keeps running
cmd:change_level mymap
>>OK

# Lua that prints immediately — no block needed
lua:print(Player.inst:get_health())
42.0
>>OK

# Block, inspect, continue
block
>>BLOCKED
lua:print(World.inst:get_entity_count())
17
>>OK
continue
>>OK

# Let engine run 10 ticks then pause
wait_ticks:10
>>OK
lua:print(game_time())
0.166
>>OK
continue
>>OK

# Lua error
lua:bad syntax %%%
>>ERROR: [string "bad syntax %%%"]:1: unexpected symbol near '%'
```

---

## Output Capture

### Console commands (`cmd:`)
`sys_print` with `LogType::LtConsoleCommand`, `Info`, `Warning`, and `Error` will be forwarded to the active socket connection via a temporary log sink, active only for the duration of the command dispatch.

`sys_print` already accepts pluggable sinks (or we add a lightweight one). The sink writes to the socket's send buffer. It is registered before dispatch and unregistered after.

### Lua (`lua:`)
Redirect `print` in Lua to a C function that writes to the socket — same technique already used in `LuaDebugServer`. Redirect is installed before `reload_from_content()` and restored after.

Lua errors (caught via `lua_pcall` status) are sent as `>>ERROR: <msg>`.

---

## `sys_print` Sink

Add a minimal sink interface to `Util.h` / the logging subsystem:

```cpp
// Util.h
struct ILogSink {
    virtual void on_print(LogType type, const char* msg) = 0;
};
void log_push_sink(ILogSink*);   // push onto a per-thread or global stack
void log_pop_sink(ILogSink*);    // remove (by pointer)
```

`AgentREPL` installs a sink that filters `Error`, `Warning`, `Info`, and `LtConsoleCommand` and writes them to the socket. `Debug` is excluded (too noisy).

---

## `AgentREPL` API

```cpp
class AgentREPL {
public:
    // Call once at startup.
    void start(int port = 9999);
    void stop();

    // Call each game loop tick.
    // - Drains all pending non-blocking commands (cmd:, lua:), sends >>OK for each.
    // - If a `block` command is pending, enters the blocked loop (see below).
    // - Returns immediately if nothing pending or after processing non-blocking commands.
    void poll();

    bool is_running() const;
    bool is_blocked() const;

private:
    // Internal: spins processing commands until `continue` is received or connection drops.
    // Used by poll() when a `block` command arrives.
    // wait_ticks / wait_time return from this loop after the requested duration, then re-enter.
    void run_blocked_loop();
};
```

### Execution on main thread
`poll()` is always called from the main thread. The blocked loop (`run_blocked_loop`) also runs on the main thread — it processes engine ticks internally for `wait_ticks`/`wait_time`, maintaining the game loop cadence while paused.

### Coroutine state machine

```
RUNNING ──[block]──> BLOCKED ──[continue]──> RUNNING
                        │
               [wait_ticks / wait_time]
                        │
                  (ticks elapse)
                        │
                     BLOCKED  (re-pauses, sends >>OK)
```

---

## Integration Points

### Game loop (`EngineMain.cpp` or equivalent)
```cpp
// In main loop, after input / before render:
if (g_agent_repl.is_running())
    g_agent_repl.poll();
```

### Test coroutines (replaces `LuaDebugServer`)
```cpp
// In a test that wants to pause for agent inspection:
g_agent_repl.start(9999);
while (!done)
    g_agent_repl.wait_and_execute_one();  // blocks; a "continue" command sets done=true
```

A built-in `cmd:continue` command signals the test to resume.

### Startup (optional)
Add a console command `agent_repl_start [port]` so it can be toggled at runtime without a recompile.

---

## Security

- Bind address is hardcoded to `127.0.0.1` — OS rejects connections from any other machine.
- Single connection at a time — second client is refused until the first disconnects.
- No authentication needed (loopback + single-user dev machine assumption).

---

## Files Changed / Created

| File | Change |
|------|--------|
| `Source/Framework/AgentREPL.h` | New |
| `Source/Framework/AgentREPL.cpp` | New |
| `Source/Framework/Util.h` | Add `ILogSink`, `log_push_sink`, `log_pop_sink` |
| `Source/Framework/Util.cpp` | Implement sink dispatch in `sys_print` |
| `Source/IntegrationTests/LuaDebugServer.h` | **Delete** |
| `Source/IntegrationTests/LuaDebugServer.cpp` | **Delete** |
| `Source/IntegrationTests/` (call sites) | Replace `LuaDebugServer` usage with `AgentREPL::wait_and_execute_one()` |
| `Source/Framework/AGENTS.md` | Update with new components |

---

## How the Agent Uses This

From a bash shell during a conversation:

```bash
# Fire-and-forget console command
echo "cmd:r_reload_shaders" | nc 127.0.0.1 9999
# → >>OK

# Lua that returns a value immediately
printf 'lua:print(Player.inst:get_health())\n' | nc 127.0.0.1 9999
# → 42.0
# → >>OK

# Block, inspect state, continue
printf 'block\nlua:print(World.inst:get_entity_count())\ncontinue\n' | nc 127.0.0.1 9999
# → >>BLOCKED
# → 17
# → >>OK   (after lua)
# → >>OK   (after continue)

# Wait for 30 ticks then inspect
printf 'block\nwait_ticks:30\nlua:print(game_time())\ncontinue\n' | nc 127.0.0.1 9999
```

The agent reads until `>>OK`, `>>BLOCKED`, or `>>ERROR:` to know each command has been acknowledged.

---

Handle continue inside the AgentREPL itself.

Global Log sink scope, only 1 connection will be active.

Port, default to 9999.