// Source/IntegrationTests/LuaDebugServer.h
#pragma once

struct lua_State;

// File-based Lua REPL that lets an AI agent inspect game state while a test is paused.
//
// Usage in a test coroutine:
//   co_await t.debug_break();
//
// While paused:
//   Write any Lua expression/statement to: TestFiles/debug/cmd.lua
//   Read output from:                      TestFiles/debug/output.txt
//   Resume the test by creating:           TestFiles/debug/continue.txt
//
// print() in executed commands is captured to output.txt.
class LuaDebugServer
{
public:
	// Called once when the test first hits debug_break(). Clears stale files, prints banner.
	static void on_enter(const char* test_name);
	// Call each frame while paused. Executes any pending cmd.lua and writes output.txt.
	// Returns true when continue.txt is detected (test should resume).
	static bool poll(lua_State* L);
};
