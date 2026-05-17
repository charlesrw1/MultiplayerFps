#include <gtest/gtest.h>
#include <cstdlib>
#include <filesystem>

// Gates Phase 1 of the graphics-device abstraction: no `gl*` function calls
// outside the OpenGL backend / accept-listed transitional callers. The
// authoritative scan is `Scripts/check_no_gl_leaks.py`; this test just runs
// it and asserts exit 0. Failure means a sub-system was touched and slipped
// raw GL back in — fix the offending file or extend the accept-list with a
// documented reason. See docs/rendering/gfx_abstraction.md sub-phase 1.9.
TEST(LegacyGlCalls, NoneOutsideBackend) {
#ifdef _WIN32
	namespace fs = std::filesystem;
	if (!fs::exists("Scripts/check_no_gl_leaks.py"))
		GTEST_SKIP() << "scanner not present (test was run outside repo root)";

	// py.exe ships with Windows + a Python install; the project CLAUDE.md
	// documents `py` as the canonical Python invocation.
	const int rc = std::system("py Scripts\\check_no_gl_leaks.py > nul");
	ASSERT_EQ(rc, 0) << "raw gl* calls detected outside backend; run "
						 "`py Scripts\\check_no_gl_leaks.py` for the file:line list";
#else
	GTEST_SKIP() << "OpenGL-leak scanner is currently Windows-only";
#endif
}
