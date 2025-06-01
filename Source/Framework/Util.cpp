#include "Util.h"
#include <cstdio>
#include <cstdlib>
#include <stdexcept>



void handle_assert_internal(const char* cond)
{
	printf("Assertion failed: %s", cond); 
#ifdef WITH_TEST_ASSERT
	if(ProgramTester::get().get_is_in_test())
		throw std::runtime_error();
#else
	std::abort();
#endif // WITH_TEST_ASSERT
}