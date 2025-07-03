#pragma once

// create functions in the codegen, with lua func signature
// these functions are tied to classtypeinfo
// when loading script, upload these functions
// disallow field access for classes? just setters/getters? code gen the setters getters?

struct lua_State;
class ScriptingSystem
{
public:
	static ScriptingSystem* inst;
	// initialized AFTER class system!
	ScriptingSystem();
	~ScriptingSystem();
private:
	lua_State* lua = nullptr;
};
