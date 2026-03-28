// Source/IntegrationTests/LuaDebugServer.cpp
#define _CRT_SECURE_NO_WARNINGS
#include "LuaDebugServer.h"
#include <cstdio>
#include <fstream>
#include <string>
#include <filesystem>

extern "C"
{
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

static const char* CMD_FILE = "TestFiles/debug/cmd.lua";
static const char* OUTPUT_FILE = "TestFiles/debug/output.txt";
static const char* CONT_FILE = "TestFiles/debug/continue.txt";
static const char* ORIG_PRINT_KEY = "_lua_debug_server_orig_print";

namespace fs = std::filesystem;

// ---- print capture --------------------------------------------------------

static FILE* s_out = nullptr;

static int lua_print_capture(lua_State* L) {
	if (!s_out)
		return 0;
	int n = lua_gettop(L);
	for (int i = 1; i <= n; i++) {
		if (i > 1)
			fputc('\t', s_out);
		size_t len = 0;
		const char* s = luaL_tolstring(L, i, &len);
		fwrite(s, 1, len, s_out);
		lua_pop(L, 1);
	}
	fputc('\n', s_out);
	fflush(s_out);
	return 0;
}

static void redirect_print(lua_State* L) {
	lua_getglobal(L, "print");
	lua_setfield(L, LUA_REGISTRYINDEX, ORIG_PRINT_KEY);
	lua_pushcfunction(L, lua_print_capture);
	lua_setglobal(L, "print");
}

static void restore_print(lua_State* L) {
	lua_getfield(L, LUA_REGISTRYINDEX, ORIG_PRINT_KEY);
	lua_setglobal(L, "print");
	lua_pushnil(L);
	lua_setfield(L, LUA_REGISTRYINDEX, ORIG_PRINT_KEY);
}

// ---- public API -----------------------------------------------------------

void LuaDebugServer::on_enter(const char* test_name) {
	fs::create_directories("TestFiles/debug");
	fs::remove(CMD_FILE);
	fs::remove(OUTPUT_FILE);
	fs::remove(CONT_FILE);

	fprintf(stderr, "\n========================================\n");
	fprintf(stderr, "[DEBUG BREAK]  test: %s\n", test_name);
	fprintf(stderr, "  cmd   -> %s\n", CMD_FILE);
	fprintf(stderr, "  out   <- %s\n", OUTPUT_FILE);
	fprintf(stderr, "  resume: create %s\n", CONT_FILE);
	fprintf(stderr, "========================================\n\n");
	fflush(stderr);
}

bool LuaDebugServer::poll(lua_State* L) {
	if (fs::exists(CONT_FILE)) {
		fs::remove(CONT_FILE);
		fprintf(stderr, "[DEBUG] Resuming test.\n");
		fflush(stderr);
		return true;
	}

	if (!fs::exists(CMD_FILE))
		return false;

	// Read and delete command file atomically-ish
	std::string code;
	{
		std::ifstream f(CMD_FILE);
		code.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
	}
	fs::remove(CMD_FILE);

	if (code.empty())
		return false;

	// Open output file, redirect print, execute, restore
	s_out = fopen(OUTPUT_FILE, "w");
	redirect_print(L);

	if (luaL_dostring(L, code.c_str()) != LUA_OK) {
		const char* err = lua_tostring(L, -1);
		if (s_out)
			fprintf(s_out, "[ERROR] %s\n", err ? err : "(unknown)");
		fprintf(stderr, "[DEBUG] Lua error: %s\n", err ? err : "(unknown)");
		lua_pop(L, 1);
	}

	restore_print(L);
	if (s_out) {
		fclose(s_out);
		s_out = nullptr;
	}

	fprintf(stderr, "[DEBUG] Command done -> %s\n", OUTPUT_FILE);
	fflush(stderr);
	return false;
}
