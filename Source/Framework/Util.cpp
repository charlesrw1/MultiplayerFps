#include "Util.h"
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
// glad.h (pulled into the same unity TU) #defines APIENTRY; windows.h would
// redefine it under a different value, which the strict /WX build promotes
// to error. Drop the prior definition before including windows.h here.
#undef APIENTRY
#include <windows.h>
#include <DbgHelp.h>
#include <share.h>
#pragma comment(lib, "DbgHelp.lib")
#endif

static AssertHookFn s_assert_hook = nullptr;
void set_assert_hook(AssertHookFn fn) { s_assert_hook = fn; }

// Path to the engine log file (e.g. test_game_output.log). The engine's Logger
// owns its own std::ofstream on this file; we cannot reopen it for writing on
// Windows due to the share mode. So we use _fsopen with _SH_DENYNO to share,
// and write+flush directly. The Logger writes via sys_print continue working
// in parallel — the direct write here is a belt-and-braces against std::abort()
// bypassing any pending stream flushes.
static const char* s_assert_log_path = nullptr;
void set_assert_log_path(const char* path) { s_assert_log_path = path; }

// Reentrancy guard: if anything called from emit_line itself asserts (e.g. the
// Logger pipeline), we would recurse infinitely and lose the original message.
// Drop the inner call's output on the floor and let the outer one finish.
static thread_local bool s_in_emit_line = false;

static void emit_line(const char* fmt, ...) {
	if (s_in_emit_line)
		return;
	s_in_emit_line = true;

	char buf[1024];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	buf[sizeof(buf) - 1] = 0;
	va_end(args);

	// stderr for CLI visibility.
	fputs(buf, stderr);
	fputc('\n', stderr);
	fflush(stderr);

#ifdef _WIN32
	// Append directly via _fsopen with _SH_DENYNO so we coexist with the
	// Logger's std::ofstream handle on the same file. Direct write+flush+close
	// ensures the trace lands on disk even if std::abort() bypasses the
	// Logger's stream buffer flush.
	if (s_assert_log_path) {
		if (FILE* f = _fsopen(s_assert_log_path, "ab", _SH_DENYNO)) {
			std::fputs("[Error] ", f);
			std::fputs(buf, f);
			std::fputc('\n', f);
			std::fflush(f);
			std::fclose(f);
		}
	}
#endif

	s_in_emit_line = false;
}

static void print_stack_trace_to_log() {
#ifdef _WIN32
	emit_line("  [stack trace begin]");
	HANDLE process = GetCurrentProcess();
	emit_line("  [got process handle]");
	// SymInitialize is safe to call multiple times; subsequent calls return FALSE
	// without re-initialising, so we just call it unconditionally to be safe even
	// if a prior assert handler already did.
	__try {
		SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		emit_line("  [SymSetOptions crashed]");
		return;
	}
	emit_line("  [after SymSetOptions]");
	__try {
		SymInitialize(process, nullptr, TRUE);
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		emit_line("  [SymInitialize crashed]");
		return;
	}
	emit_line("  [after SymInitialize]");

	void* frames[64];
	const USHORT captured = CaptureStackBackTrace(1 /* skip this frame */, 64, frames, nullptr);
	emit_line("  [stack frames captured: %u]", (unsigned)captured);

	constexpr DWORD MAX_NAME = 512;
	alignas(SYMBOL_INFO) char buffer[sizeof(SYMBOL_INFO) + MAX_NAME];
	auto* sym = reinterpret_cast<SYMBOL_INFO*>(buffer);
	sym->SizeOfStruct = sizeof(SYMBOL_INFO);
	sym->MaxNameLen = MAX_NAME;

	IMAGEHLP_LINE64 line{};
	line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);

	for (USHORT i = 0; i < captured; ++i) {
		const DWORD64 addr = reinterpret_cast<DWORD64>(frames[i]);
		const char* name = "<no symbol>";
		if (SymFromAddr(process, addr, nullptr, sym))
			name = sym->Name;
		DWORD displacement = 0;
		if (SymGetLineFromAddr64(process, addr, &displacement, &line)) {
			emit_line("  [%02u] %s (%s:%lu)", i, name, line.FileName, line.LineNumber);
		} else {
			emit_line("  [%02u] %s", i, name);
		}
	}
	emit_line("  [stack trace end]");
#endif
}

void handle_assert_internal(const char* cond) {
	// Reentrancy guard: an assert raised from inside our own crash-dump path
	// (e.g. from the user-installed hook or from the stack-walker) must not
	// loop. We mark and abort instead.
	static thread_local bool s_in_assert = false;
	if (s_in_assert) {
		fputs("\n[double-fault inside handle_assert_internal — aborting]\n", stderr);
		std::abort();
	}
	s_in_assert = true;

	emit_line("Assertion failed: %s", cond);
	print_stack_trace_to_log();
	if (s_assert_hook)
		s_assert_hook(cond);
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
