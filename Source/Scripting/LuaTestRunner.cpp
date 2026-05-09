#include "LuaTestRunner.h"
#include "Framework/SysPrint.h"

LuaTestRunner::Sink* LuaTestRunner::active_sink = nullptr;

void LuaTestRunner::report(std::string name, bool passed, std::string message) {
	if (!active_sink) {
		sys_print(Warning, "LuaTestRunner.report called with no active sink: %s\n", name.c_str());
		return;
	}
	active_sink->report(name, passed, message);
}

void LuaTestRunner::set_done() {
	if (!active_sink) {
		sys_print(Warning, "LuaTestRunner.set_done called with no active sink\n");
		return;
	}
	active_sink->set_done();
}
