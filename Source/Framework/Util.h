#ifndef UTIL_H
#define UTIL_H
#include <cstdint>
#include "Framework/Handle.h"

void handle_assert_internal(const char* msg);
#ifdef WITH_ASSERT
#define ASSERT(x) \
	do { if(!(x)) {	\
		handle_assert_internal(#x); \
	} }while (0);

#else
#define ASSERT(x)
#endif
bool CheckGlErrorInternal_(const char* file, int line);
#define glCheckError() CheckGlErrorInternal_(__FILE__,__LINE__)
double GetTime();
double TimeSinceStart();

enum LogType
{
	Error,
	Warning,
	Info,
	Debug,
	LtConsoleCommand,	// special
};

void Fatalf(const char* format, ...);
void sys_print(LogType type, const char* fmt, ...);

char* string_format(const char* fmt, ...);

inline const char* print_get_bool_string(bool b) {
	return b ? "True" : "False";
}


struct Color32
{
	Color32() = default;
	Color32(unsigned int c);
	Color32(int r, int g, int b, int a=0xff);

	uint8_t r = 0;
	uint8_t g = 0;
	uint8_t b = 0;
	uint8_t a = 0xff;

	uint32_t to_uint() const {
		return *(uint32_t*)this;
	}
};
#define COLOR_WHITE Color32(0xff,0xff,0xff,0xff)
#define COLOR_BLACK Color32(0,0,0,0xff)
#define COLOR_BLUE Color32(0,0,0xff,0xff)
#define COLOR_RED Color32(0xff,0,0,0xff)
#define COLOR_GREEN Color32(0,0xff,0,0xff)
#define COLOR_PINK Color32(0xff,0,0xff,0xff)
#define COLOR_CYAN Color32(0,0xff,0xff,0xff)


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


// todo: remove this, only works with single thread
class Profiler
{
public:
	static void init();
	static void start_scope(const char* name, bool include_gpu);
	static void end_scope(const char* name);
	static void end_frame_tick(float dt);
};

struct Profile_Scope_Wrapper
{
	Profile_Scope_Wrapper(const char* name, bool gpu) :myname(name) {
		Profiler::start_scope(name, gpu);
	}
	~Profile_Scope_Wrapper() {
		Profiler::end_scope(myname);
	}

	const char* myname;
};
#define PROFILING

#ifdef PROFILING
#define MAKEPROF_(type, include_gpu) Profile_Scope_Wrapper PROFILEEVENT##__LINE__(type, include_gpu)
#define CPUFUNCTIONSTART MAKEPROF_(__FUNCTION__, false)
#define GPUFUNCTIONSTART MAKEPROF_(__FUNCTION__, true)
#define GPUSCOPESTART(name) Profile_Scope_Wrapper profileevent##name(#name, true)
#define CPUSCOPESTART(name) Profile_Scope_Wrapper profileevent##name(#name, false)


#else
#define CPUPROF(type)
#define GPUPROF(type)
#endif





#endif // !UTIL_H
