#include "Test.h"
#include "Framework/Util.h"
#include "Framework/Config.h"
#ifdef WITH_TESTS
void ProgramTester::run_all()
{
	sys_print("--------- Running Tests ----------\n");
	sys_print("-num tests: %d\n", (int)allTests.size());
	int er = 0;
	for (int i = 0; i < allTests.size(); i++) {
		int numErrs = allTests[i].func();
		if (numErrs == 0)
			sys_print("``` %s good\n", allTests[i].strName);
		else
			sys_print("!!! %s has %d errors\n", allTests[i].strName, numErrs);
		er += numErrs;
	}
	if (er == 0)
		sys_print("``` all tests passed\n");
	else
		sys_print("!!! tests had %d errors\n", er);
}

DECLARE_ENGINE_CMD(RUN_TESTS)
{
	ProgramTester::get().run_all();
}

#endif