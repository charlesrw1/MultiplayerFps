#include <gtest/gtest.h>
#include "Framework/Util.h"

int main(int argc, char** argv) {
	install_crash_handler();
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
