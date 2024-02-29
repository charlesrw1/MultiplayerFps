#ifndef UTIL_H
#define UTIL_H
#include <cstdint>

#define ASSERT(x) \
	do { if(!(x)) {	\
	printf("Assertion failed: %s", #x); std::abort(); \
	} }while (0);
bool CheckGlErrorInternal_(const char* file, int line);
#define glCheckError() CheckGlErrorInternal_(__FILE__,__LINE__)
double GetTime();
double TimeSinceStart();

void Fatalf(const char* format, ...);
void sys_print(const char* fmt, ...);

struct Color32
{
	uint8_t r = 0;
	uint8_t g = 0;
	uint8_t b = 0;
	uint8_t a = 0xff;
};
#define COLOR_WHITE Color32{0xff,0xff,0xff,0xff}
#define COLOR_BLACK Color32{0,0,0,0xff}
#define COLOR_BLUE Color32{0,0,0xff,0xff}
#define COLOR_RED Color32{0xff,0,0,0xff}
#define COLOR_GREEN Color32{0,0xff,0,0xff}
#define COLOR_PINK Color32{0xff,0,0xff,0xff}
#define COLOR_CYAN Color32{0,0xff,0xff,0xff}

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

typedef Buffer File_Buffer;

// useful helper for txt data files
bool file_getline(const File_Buffer* file, Buffer* str_buffer, int* index, char delimiter = '\n');

class Files
{
public:
	enum {
		LOOK_IN_ARCHIVE = 1,
		TEXT = 2,
	};

	static File_Buffer* open(const char* path, int flags = LOOK_IN_ARCHIVE);
	static void close(File_Buffer*& file);
	static void init();
};

class Profiler
{
public:
	static void init();
	static void start_scope(const char* name, bool include_gpu);
	static void end_scope(const char* name);
	static void end_frame_tick();
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
#define GPUSCOPESTART(name) MAKEPROF_(name, true)
#define CPUSCOPESTART(name) MAKEPROF_(name, false)


#else
#define CPUPROF(type)
#define GPUPROF(type)
#endif





#endif // !UTIL_H
