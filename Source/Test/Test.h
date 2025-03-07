#pragma once
#include "Framework/Util.h"
#include <stdexcept>
#define WITH_TESTS

#ifdef WITH_TESTS

#define TEST_TRUE_COMMENT(code, comment) \
	if(!(code)) { \
		_CrtDbgBreak();	\
		ProgramTester::get().set_test_failed(#code, comment); \
		throw std::runtime_error("er"); \
	}

#define TEST_TRUE(code) \
	TEST_TRUE_COMMENT(code, "expected true")

	
#define TEST_THROW(code) \
	try { \
		code; \
		ProgramTester::get().set_test_failed(#code, "expected exception"); \
		throw std::runtime_error("er"); \
	} \
	catch (...) { \
	} \


typedef void(*test_func_t)();

#include <vector>
class ProgramTester
{
public:
	static ProgramTester& get() {
		static ProgramTester inst;
		return inst;
	}
	bool run_all(bool print_good);
	void add_test(const char* category, const char* subcategory, test_func_t func) {
		allTests.push_back({ category,subcategory, func });
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
		const char* sub_category = "";
		test_func_t func = nullptr;
	};
	std::vector<Test> allTests;
	bool test_failed = false;
	const char* expression = "";
	const char* reason = "";
	bool is_in_test = false;

};

struct AutoTestCaseAdd {
	AutoTestCaseAdd(const char* name, const char* subcat, test_func_t f) {
		ProgramTester::get().add_test(name,subcat, f);
	}
};

#define ADD_TEST(category, subcategory) void category##subcategory(); \
static AutoTestCaseAdd autotester_##category##subcategory = AutoTestCaseAdd(#category,#subcategory, category##subcategory); \
void category##subcategory()

#else
#define ADD_TEST(func_name, string_name) void  category##subcategory() {
#define TEST_TRUE_COMMENT(code, comment)
#define TEST_THROW(code)
#define TEST_TRUE(code)
#endif
