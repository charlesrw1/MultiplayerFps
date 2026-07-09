#ifndef UTIL_H
#define UTIL_H
#include <cstdint>
#include "Framework/Handle.h"

void handle_assert_internal(const char* msg);
typedef void (*AssertHookFn)(const char* condition);
void set_assert_hook(AssertHookFn fn);
// Tell the assert handler where to append crash output (e.g. test_game_output.log).
// std::abort() bypasses C++ stream flush, so we write directly via fopen/fclose.
void set_assert_log_path(const char* path);
// Install SetUnhandledExceptionFilter that prints a [CRASH] header + symbolised
// stack trace to stderr and (if set_assert_log_path was called) the engine log,
// then exits non-zero. Without this, an SEH crash (e.g. access violation) under
// a test runner just exits with 0xC0000005 and no diagnostic — agents see only
// "program crashed" with nothing to investigate. No-op on non-Windows.
void install_crash_handler();
// Writes a minidump of the current process state to `path`. No-op on
// non-Windows; returns false on failure. Useful for capturing a snapshot
// from a probe point without crashing — and used by the crash-dump smoke
// test to validate the underlying dbghelp pipeline.
bool write_snapshot_minidump(const char* path);
#ifdef WITH_ASSERT
#define ASSERT(x)                                                                                                      \
	do {                                                                                                               \
		if (!(x)) {                                                                                                    \
			handle_assert_internal(#x);                                                                                \
		}                                                                                                              \
	} while (0);

#else
#define ASSERT(x)
#endif
bool CheckGlErrorInternal_(const char* file, int line);
#define gfx_check_gl_error() CheckGlErrorInternal_(__FILE__, __LINE__)
double GetTime();
double TimeSinceStart();

enum LogType
{
	Error,
	Warning,
	Info,
	Debug,
	LtConsoleCommand, // special
};

void Fatalf(const char* format, ...);
void sys_print(LogType type, const char* fmt, ...);

char* string_format(const char* fmt, ...);

inline const char* print_get_bool_string(bool b) {
	return b ? "True" : "False";
}

struct lColor;
struct Color32
{
	Color32() = default;
	Color32(unsigned int c);
	Color32(int r, int g, int b, int a = 0xff);
	Color32(const lColor& lcolor);

	uint8_t r = 0;
	uint8_t g = 0;
	uint8_t b = 0;
	uint8_t a = 0xff;

	uint32_t to_uint() const { return *(uint32_t*)this; }
};
#define COLOR_WHITE Color32(0xff, 0xff, 0xff, 0xff)
#define COLOR_BLACK Color32(0, 0, 0, 0xff)
#define COLOR_BLUE Color32(0, 0, 0xff, 0xff)
#define COLOR_RED Color32(0xff, 0, 0, 0xff)
#define COLOR_GREEN Color32(0, 0xff, 0, 0xff)
#define COLOR_PINK Color32(0xff, 0, 0xff, 0xff)
#define COLOR_CYAN Color32(0, 0xff, 0xff, 0xff)

struct Buffer
{
	char* buffer;
	size_t length;
};

const float PI = 3.1415926536;
const float TWOPI = PI * 2.f;
const float HALFPI = PI * 0.5f;
const float INV_PI = 1.f / PI;
const float SQRT2 = 1.41421362;
const float INV_SQRT2 = 1 / SQRT2;

#include "Framework/Profiler.h" // CPU_SCOPE / GPU_SCOPE / RENDER_SCOPE / CPU_FUNCTION / GPU_FUNCTION

#endif // !UTIL_H
