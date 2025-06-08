#pragma once
#include "Framework/Util.h"
#include <stdexcept>
#include <vector>
#include <string>

#define checkTrueComment(code, comment) \
	if(!(code)) { \
		_CrtDbgBreak();	\
		ProgramTester::get().set_test_failed(#code, comment); \
		throw std::runtime_error("er"); \
	}

#define checkTrue(code) \
	checkTrueComment(code,"expected true")

#define checkThrow(code) \
	try { \
		code; \
		ProgramTester::get().set_test_failed(#code, "expected exception"); \
		throw std::runtime_error("er"); \
	} \
	catch (...) { \
	} \

using std::string;
typedef void(*test_func_t)();
class ProgramTester
{
public:
	static ProgramTester& get() {
		static ProgramTester inst;
		return inst;
	}
	bool run_all(bool print_good);
	void add_test(const char* category, const  test_func_t func) {
		allTests.push_back({ category, func });
	}
	void set_test_failed(const char* expression, const char* reason) {
		test_failed = true;
		this->expression = expression;
		this->reason = reason;

	}
	bool get_is_in_test() const { return is_in_test; }
private:
	struct Test {
		const char* category = "";
		test_func_t func = nullptr;
	};
	std::vector<Test> allTests;
	bool test_failed = false;
	string expression = "";
	string reason = "";
	bool is_in_test = false;
};

struct AutoTestCaseAdd {
	AutoTestCaseAdd(const char* name, test_func_t f) {
		ProgramTester::get().add_test(name, f);
	}
};

#define ADD_TEST(name) void test_##name(); \
static AutoTestCaseAdd autotester_##name = AutoTestCaseAdd(#name,test_##name); \
void test_##name()

class UnitTestUtil
{
public:
	static std::string get_text_of_file(const std::string& text);
};