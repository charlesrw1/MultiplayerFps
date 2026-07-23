// eval_lua: runs arbitrary Lua source against the running engine's Lua state and converts
// whatever it returns into JSON. Kept in its own file (rather than AgentBridgeCommands.cpp)
// since it's the only handler that needs the raw Lua C API.
#include "AgentBridge.h"
#include "Scripting/ScriptManager.h"
extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}
#include <stdexcept>
#include <string>

namespace {

// Converts the Lua value at stack index idx into an equivalent JSON value. Tables are treated as
// JSON arrays when every key is a contiguous 1..n integer sequence (the common Lua "array" idiom),
// otherwise as JSON objects with non-string keys stringified. Depth-limited since Lua tables can
// be cyclic (a runaway conversion is a much worse failure mode than a truncated result).
nlohmann::json lua_value_to_json(lua_State* L, int idx, int depth = 0) {
	idx = lua_absindex(L, idx);
	if (depth > 8)
		return "<max table depth exceeded>";

	switch (lua_type(L, idx)) {
	case LUA_TNIL:
		return nullptr;
	case LUA_TBOOLEAN:
		return (bool)lua_toboolean(L, idx);
	case LUA_TNUMBER:
		if (lua_isinteger(L, idx))
			return (long long)lua_tointeger(L, idx);
		return lua_tonumber(L, idx);
	case LUA_TSTRING: {
		size_t len = 0;
		const char* s = lua_tolstring(L, idx, &len);
		return std::string(s, len);
	}
	case LUA_TTABLE: {
		lua_Integer array_len = (lua_Integer)lua_rawlen(L, idx);
		bool is_array = array_len > 0;
		if (is_array) {
			// Confirm there are no extra (non-1..n) keys hiding alongside the array part.
			lua_Integer key_count = 0;
			lua_pushnil(L);
			while (lua_next(L, idx) != 0) {
				key_count++;
				lua_pop(L, 1);
			}
			is_array = (key_count == array_len);
		}

		if (is_array) {
			nlohmann::json arr = nlohmann::json::array();
			for (lua_Integer i = 1; i <= array_len; i++) {
				lua_rawgeti(L, idx, i);
				arr.push_back(lua_value_to_json(L, -1, depth + 1));
				lua_pop(L, 1);
			}
			return arr;
		}

		nlohmann::json obj = nlohmann::json::object();
		lua_pushnil(L);
		while (lua_next(L, idx) != 0) {
			std::string key;
			if (lua_type(L, -2) == LUA_TSTRING) {
				size_t len = 0;
				const char* s = lua_tolstring(L, -2, &len);
				key.assign(s, len);
			} else {
				// Non-string key (number/bool/etc) - stringify a scratch copy so we don't
				// disturb the key lua_next needs on the stack for the next iteration.
				lua_pushvalue(L, -2);
				key = lua_tostring(L, -1);
				lua_pop(L, 1);
			}
			obj[key] = lua_value_to_json(L, -1, depth + 1);
			lua_pop(L, 1);
		}
		return obj;
	}
	default:
		return std::string("<") + lua_typename(L, lua_type(L, idx)) + ">";
	}
}

nlohmann::json eval_lua_cmd(const nlohmann::json& args) {
	if (!args.contains("code") || !args["code"].is_string())
		throw std::runtime_error("eval_lua requires a string 'code' arg");
	std::string code = args["code"];

	if (!ScriptManager::inst)
		throw std::runtime_error("lua not initialized");
	lua_State* L = ScriptManager::inst->get_lua_state();
	if (!L)
		throw std::runtime_error("lua not initialized");

	const int top_before = lua_gettop(L);

	// Try as an expression first (like a REPL: `eval("1+1")` should just work), fall back to
	// loading verbatim so statements/blocks (`eval("do_thing(); return 1")`) still work.
	std::string wrapped = "return " + code;
	if (luaL_loadstring(L, wrapped.c_str()) != LUA_OK) {
		lua_pop(L, 1);
		if (luaL_loadstring(L, code.c_str()) != LUA_OK) {
			std::string err = lua_tostring(L, -1);
			lua_pop(L, 1);
			throw std::runtime_error(err);
		}
	}

	if (lua_pcall(L, 0, LUA_MULTRET, 0) != LUA_OK) {
		std::string err = lua_tostring(L, -1);
		lua_pop(L, 1);
		throw std::runtime_error(err);
	}

	const int nres = lua_gettop(L) - top_before;
	nlohmann::json result = nullptr;
	if (nres == 1) {
		result = lua_value_to_json(L, top_before + 1);
	} else if (nres > 1) {
		nlohmann::json arr = nlohmann::json::array();
		for (int i = 0; i < nres; i++)
			arr.push_back(lua_value_to_json(L, top_before + 1 + i));
		result = arr;
	}
	lua_settop(L, top_before);

	nlohmann::json r;
	r["result"] = result;
	return r;
}

} // namespace

// @cmd: executes Lua source against the running engine's Lua state and returns its value as JSON.
// Tries the code as an expression first (`return <code>`), falling back to running it verbatim
// for statements/blocks. Errors (syntax or runtime) come back as a normal bridge error.
// @usage: eval_lua {"code": "<lua source, e.g. \"1+1\" or \"return get_player()\">"}
AGENT_BRIDGE_COMMAND(eval_lua, eval_lua_cmd);
