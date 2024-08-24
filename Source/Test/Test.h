#pragma once
#include "Framework/Util.h"
#define WITH_TESTS

#ifdef WITH_TESTS

#if DONT_STOP_ON_ERROR
#define TEST_VERIFY(condition) \
	if(!condition) {\
		return erCount + 1;\
	}
#else
#define TEST_VERIFY(condition) ASSERT(condition)
#endif


#include <vector>

typedef int(*test_func_t)();

class ProgramTester
{
public:
	static ProgramTester& get() {
		static ProgramTester inst;
		return inst;
	}
	void run_all();
	void add_test(const char* name, test_func_t func) {
		allTests.push_back({ name,func });
	}
private:
	struct Test {
		const char* strName = "";
		test_func_t func = nullptr;
	};
	std::vector<Test> allTests;
};

struct AutoTestCaseAdd {
	AutoTestCaseAdd(const char* name, test_func_t f) {
		ProgramTester::get().add_test(name, f);
	}
};

#define ADD_TEST(func_name)  static AutoTestCaseAdd autotester_##func_name = AutoTestCaseAdd(#func_name,func_name);
#else
#define ADD_TEST(func_name);
#endif