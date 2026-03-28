#pragma once
#include <cstdint>
#include <memory>
#include <string>

// TCP socket REPL for AI agents (and other tooling) to issue commands to the running engine.
//
// Listens on 127.0.0.1 only — loopback, no external access possible.
//
// Protocol (line-based text):
//   cmd:<string>       — Cmd_Manager::execute(APPEND, ...)  fires and engine continues
//   lua:<code>         — ScriptManager::reload_from_content(...)  fires and engine continues
//   block              — engine loop pauses; poll() blocks until continue/wait_ticks/wait_time
//   continue           — resumes from a block
//   wait_ticks:<n>     — while blocked, let engine run N ticks then re-pause
//   wait_time:<secs>   — while blocked, let engine run T seconds then re-pause
//
// Responses end with >>OK or >>ERROR: <msg>  so clients know the command is complete.
// block responds with >>BLOCKED.
//
// Usage:
//   AgentREPL::inst->start();          // once at startup
//   AgentREPL::inst->poll();           // every game loop tick
class AgentREPL
{
public:
	static AgentREPL* inst;

	AgentREPL();
	~AgentREPL();

	void start(int port = 9999);
	void stop();

	// Call each game loop tick.
	// - Accepts a pending connection if none active.
	// - Drains non-blocking commands (cmd:, lua:), responds >>OK.
	// - If "block" received, enters the blocked loop (halts game loop until "continue"
	//   or a wait_ticks/wait_time expires).
	void poll();

	bool is_running() const;

	// Returns true (and clears the flag) when the agent has sent "resume" while not
	// in a block. Used by GameTestRunner to resume a debug_break() coroutine wait.
	bool take_resume_requested();

private:
	struct Impl;
	std::unique_ptr<Impl> impl_;
};
