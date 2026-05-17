#pragma once
#include <cstdint>

typedef uint32_t texhandle;
typedef uint32_t fbohandle;
typedef uint32_t bufferhandle;
typedef uint32_t proghandle;
typedef uint32_t vertexbufferhandle;
typedef uint32_t indexbufferhandle;
typedef uint32_t vertexarrayhandle;

typedef int program_handle;

// Pipeline blend modes. Lives here (not MaterialLocal.h) so IGraphicsDevice.h
// can reference it without pulling material internals.
#undef OPAQUE // windows header leaking
enum class BlendState : int8_t
{
	OPAQUE,
	BLEND,
	ADD,
	MULT,
	SCREEN,
	PREMULT_BLEND
};