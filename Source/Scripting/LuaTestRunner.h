#pragma once
#include "Framework/ClassBase.h"
#include "Scripting/ScriptFunctionCodegen.h"
#include <string>

// Lua-callable shim that hands per-test results to the active C++ TestRunner.
// Used by Data/scripts/integration_test_framework.lua during the Lua phase.
//
// The C++ TestRunner sets `active_sink` on entering the Lua phase and clears
// it again before exiting, so report()/set_done() never run with a stale
// pointer.
class LuaTestRunner : public ClassBase
{
public:
	CLASS_BODY(LuaTestRunner);

	struct Sink
	{
		virtual ~Sink() = default;
		virtual void report(const std::string& name, bool passed, const std::string& message) = 0;
		virtual void set_done() = 0;
	};
	static Sink* active_sink;

	REF static void report(std::string name, bool passed, std::string message);
	REF static void set_done();
};
