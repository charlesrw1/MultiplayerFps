#pragma once
#include "Framework/ClassBase.h"
#include "Scripting/ScriptFunctionCodegen.h"
#include <string>

class LuaTestRunner : public ClassBase
{
public:
	CLASS_BODY(LuaTestRunner);
	// Called from Lua when all tests are done.
	// Writes JUnit XML to TestFiles/integration_lua_results.xml, then quits.
	REF static void finish(int pass, int fail, std::string failures);
};
