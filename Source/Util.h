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


const float PI = 3.1415926536;
const float TWOPI = PI * 2.f;
const float HALFPI = PI * 0.5f;
const float INV_PI = 1.f / PI;
const float SQRT2 = 1.41421362;
const float INV_SQRT2 = 1 / SQRT2;

#endif // !UTIL_H
