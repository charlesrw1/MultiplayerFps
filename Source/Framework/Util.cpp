#include "Util.h"
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

static AssertHookFn s_assert_hook = nullptr;
void set_assert_hook(AssertHookFn fn) { s_assert_hook = fn; }

void handle_assert_internal(const char* cond) {
	if (s_assert_hook)
		s_assert_hook(cond);
	printf("Assertion failed: %s", cond);
#ifdef WITH_TEST_ASSERT
	if (ProgramTester::get().get_is_in_test())
		throw std::runtime_error();
#else
	std::abort();
#endif // WITH_TEST_ASSERT
}

Color32::Color32(unsigned int c) {
	r = c & 0xff;
	g = (c >> 8) & 0xff;
	b = (c >> 16) & 0xff;
	a = (c >> 24) & 0xff;
}

Color32::Color32(int r, int g, int b, int a) : r(r), g(g), b(b), a(a) {}
