#include <gtest/gtest.h>
#include <cstdio>
#include <cstring>
#include "Framework/Util.h"

// Hidden crash self-test: --crash-self-test installs the SEH handler, points
// the log at crash_selftest.log next to CWD, then deliberately dereferences
// null in a function we own so the resulting .dmp has user-code frames with
// recoverable locals. Used by Scripts/analyze_dump.ps1 verification.
struct CrashSelfTestPayload {
	int marker;
	const char* name;
	float xyz[3];
};

static void run_crash_self_test() {
	set_assert_log_path("crash_selftest.log");
	install_crash_handler();
	volatile int marker_local = 0xC0DEFACE;
	const char* marker_string = "crash-self-test-marker";
	CrashSelfTestPayload payload{ 0xBEEF, "payload-name", { 1.5f, 2.5f, 3.5f } };
	CrashSelfTestPayload* payload_ptr = &payload;
	int* nullp = nullptr;
	*nullp = (int)marker_local + (int)std::strlen(marker_string) + payload_ptr->marker;
}

int main(int argc, char** argv) {
	for (int i = 1; i < argc; ++i) {
		if (std::strcmp(argv[i], "--crash-self-test") == 0) {
			run_crash_self_test();
			return 0; // unreachable
		}
	}
	install_crash_handler();
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
