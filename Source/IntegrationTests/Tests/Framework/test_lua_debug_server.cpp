// Source/IntegrationTests/Tests/Framework/test_lua_debug_server.cpp
#include "IntegrationTests/TestContext.h"
#include "IntegrationTests/TestRegistry.h"
#include "GameEnginePublic.h"
#include "IntegrationTests/LuaDebugServer.h"
#include "Scripting/ScriptManager.h"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

static const char* TEST_CMD = "TestFiles/debug/cmd.lua";
static const char* TEST_OUTPUT = "TestFiles/debug/output.txt";
static const char* TEST_CONT = "TestFiles/debug/continue.txt";

// Verifies: writing cmd.lua -> poll() executes it -> output.txt contains print() result
static TestTask test_lua_repl_exec(TestContext& t) {
	fs::create_directories("TestFiles/debug");
	fs::remove(TEST_CMD);
	fs::remove(TEST_OUTPUT);
	fs::remove(TEST_CONT);

	{
		std::ofstream f(TEST_CMD);
		f << "print('lua_repl_ok')\n";
	}

	lua_State* L = ScriptManager::inst->get_lua_state();
	bool resumed = LuaDebugServer::poll(L);

	t.check(!resumed, "poll returns false with no continue.txt");
	t.check(!fs::exists(TEST_CMD), "cmd.lua consumed after execution");
	t.check(fs::exists(TEST_OUTPUT), "output.txt written after execution");

	std::string output;
	{
		std::ifstream f(TEST_OUTPUT);
		output.assign(std::istreambuf_iterator<char>(f), {});
	}
	t.check(output.find("lua_repl_ok") != std::string::npos, "output.txt contains expected print value");

	co_return;
}
GAME_TEST("framework/lua_repl_exec", 5.f, test_lua_repl_exec);

// Verifies: continue.txt present -> poll() returns true and deletes the file
static TestTask test_lua_repl_continue(TestContext& t) {
	fs::create_directories("TestFiles/debug");
	fs::remove(TEST_CMD);
	fs::remove(TEST_CONT);

	{ std::ofstream f(TEST_CONT); }

	lua_State* L = ScriptManager::inst->get_lua_state();
	bool resumed = LuaDebugServer::poll(L);

	t.check(resumed, "poll returns true when continue.txt exists");
	t.check(!fs::exists(TEST_CONT), "continue.txt consumed after resume");

	co_return;
}
GAME_TEST("framework/lua_repl_continue", 5.f, test_lua_repl_continue);

// Verifies: Lua syntax errors are written to output.txt as [ERROR] lines
static TestTask test_lua_repl_error_captured(TestContext& t) {
	fs::create_directories("TestFiles/debug");
	fs::remove(TEST_CMD);
	fs::remove(TEST_OUTPUT);
	fs::remove(TEST_CONT);

	{
		std::ofstream f(TEST_CMD);
		f << "this is not valid lua !!!\n";
	}

	lua_State* L = ScriptManager::inst->get_lua_state();
	LuaDebugServer::poll(L);

	t.check(fs::exists(TEST_OUTPUT), "output.txt written on Lua error");

	std::string output;
	{
		std::ifstream f(TEST_OUTPUT);
		output.assign(std::istreambuf_iterator<char>(f), {});
	}
	t.check(output.find("[ERROR]") != std::string::npos, "output.txt contains [ERROR] on bad Lua");

	co_return;
}
GAME_TEST("framework/lua_repl_error_captured", 5.f, test_lua_repl_error_captured);

// Verifies: no cmd.lua present -> poll() returns false and writes no output
static TestTask test_lua_repl_no_cmd(TestContext& t) {
	fs::create_directories("TestFiles/debug");
	fs::remove(TEST_CMD);
	fs::remove(TEST_OUTPUT);
	fs::remove(TEST_CONT);

	lua_State* L = ScriptManager::inst->get_lua_state();
	bool resumed = LuaDebugServer::poll(L);

	t.check(!resumed, "poll returns false with no files present");
	t.check(!fs::exists(TEST_OUTPUT), "no output.txt written when no cmd.lua");

	co_return;
}
GAME_TEST("framework/lua_repl_no_cmd", 5.f, test_lua_repl_no_cmd);
