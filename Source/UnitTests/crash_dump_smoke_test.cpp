#include <gtest/gtest.h>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include "Framework/Util.h"

// Validates the minidump-writing pipeline (DbgHelp link, MINIDUMP_TYPE flags,
// path handling) by snapshotting the current process. We cannot test the SEH
// path in-process without killing the test runner. See [[debugging/crash_dumps]].
TEST(CrashDumpSmoke, WritesNonEmptyDump) {
#ifdef _WIN32
	namespace fs = std::filesystem;
	const fs::path dump = fs::temp_directory_path() / "csremake_unittest_snapshot.dmp";
	std::error_code ec;
	fs::remove(dump, ec); // ignore "file not found"

	ASSERT_TRUE(write_snapshot_minidump(dump.string().c_str()))
		<< "write_snapshot_minidump returned false";

	ASSERT_TRUE(fs::exists(dump));
	// A minimal minidump with our flags is well above 4 KB in practice. If
	// this shrinks, the flag set has degraded — fail loudly so the regression
	// is caught here rather than in the next real crash investigation.
	ASSERT_GT(fs::file_size(dump), 4096u);

	// Leave the dump behind if KEEP_CRASH_DUMP_SNAPSHOT=1 so analyze_dump.ps1
	// can be exercised against it manually. Default is to clean up.
	char keepbuf[8] = {};
	size_t keeplen = 0;
	getenv_s(&keeplen, keepbuf, sizeof(keepbuf), "KEEP_CRASH_DUMP_SNAPSHOT");
	if (keeplen == 0 || keepbuf[0] != '1')
		fs::remove(dump, ec);
	else
		std::printf("dump preserved at %s\n", dump.string().c_str());
#else
	GTEST_SKIP() << "Crash dumps are Windows-only";
#endif
}
