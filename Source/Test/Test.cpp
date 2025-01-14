#include "Test.h"
#include "Framework/Util.h"
#include "Framework/Config.h"
#ifdef WITH_TESTS
bool ProgramTester::run_all(bool print_good)
{
	sys_print("--------- Running Tests ----------\n");
	sys_print("-num tests: %d\n", (int)allTests.size());
	int er = 0;
	for (int i = 0; i < allTests.size(); i++) {
		test_failed = false;
		expression = reason = "";

		is_in_test = true;
		try {
			allTests[i].func();
		}
		catch (...) {
			set_test_failed("<unknown>", "threw an exception");
		}
		is_in_test = false;

		if (!test_failed) {
			if(print_good)
				sys_print("``` %s:%s good\n", allTests[i].category, allTests[i].sub_category);
		}
		else {
			sys_print("!!! %s:%s FAILED (%s:%s)\n", allTests[i].category,allTests[i].sub_category, expression,reason);
			er++;
		}
	}
	if (er == 0)
		sys_print("``` all tests passed\n");
	else
		sys_print("!!! tests had %d errors\n", er);
	return er == 0;
}

DECLARE_ENGINE_CMD(RUN_TESTS)
{
	bool res = ProgramTester::get().run_all(true);
}

#endif