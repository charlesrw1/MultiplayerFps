#ifndef UTIL_H
#define UTIL_H
#include <cstdint>

#define ASSERT(x) \
	do { if(!(x)) {	\
	printf("Assertion failed: %s", #x); std::abort(); \
	} }while (0);

bool CheckGlErrorInternal_(const char* file, int line);
#define glCheckError() CheckGlErrorInternal_(__FILE__,__LINE__)

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

#endif // !UTIL_H
