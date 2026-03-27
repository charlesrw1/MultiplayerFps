// Source/IntegrationTests/TestRegistry.cpp
#include "TestRegistry.h"
#include <cstring>

std::vector<TestEntry>& TestRegistry::all() {
	static std::vector<TestEntry> s_tests;
	return s_tests;
}

void TestRegistry::register_test(TestEntry e) {
	all().push_back(std::move(e));
}

std::vector<TestEntry> TestRegistry::get_filtered(TestMode mode, const char* glob) {
	std::vector<TestEntry> out;
	for (auto& e : all()) {
		if (e.mode != mode)
			continue;
		if (!glob || glob[0] == '\0' || test_glob_match(glob, e.name))
			out.push_back(e);
	}
	return out;
}

bool test_glob_match(const char* pattern, const char* str) {
	if (!pattern || pattern[0] == '\0')
		return true;
	if (*pattern == '*') {
		if (test_glob_match(pattern + 1, str))
			return true;
		if (*str)
			return test_glob_match(pattern, str + 1);
		return false;
	}
	if (*str == '\0')
		return false;
	if (*pattern == '?' || *pattern == *str)
		return test_glob_match(pattern + 1, str + 1);
	return false;
}
