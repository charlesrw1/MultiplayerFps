#pragma once
// GLSL source loader: resolves "#include" directives recursively from the
// engine "Shaders\\" directory, prepends a #version line + caller-supplied
// #defines, and tracks per-line origin so a driver error string of the form
// "ERROR: 0:<line>: ..." can be translated back to the original source file
// and line number.
//
// Pure text processing — no GL, no SPIR-V. Consumed by both the OpenGL
// backend (OpenGlShaderImpl.cpp) and the SDL3 GPU SPIR-V pipeline
// (SpirvCompile.cpp). Lifted from the anon namespace of OpenGlShaderImpl.cpp
// in Phase 3.1.

#include <string>
#include <vector>

struct ShaderSource
{
	std::string source;
	int line_count = 0;

	struct FileAndRange
	{
		std::string filename;
		int line_start = 0;
		int line_count = 0;
		int input_line_start = 0;
	};
	std::vector<FileAndRange> ranges;

	bool empty() const { return source.empty(); }
	const char* c_str() const { return source.c_str(); }

	// Parse a driver-style error string ("ERROR: 0:<line>: ...") and print
	// "shader error in <file> on line <orig-line>" via sys_print(Error).
	// Best-effort; silently returns if the prefix does not match.
	void print_error(const std::string& driver_error) const;
};

// Build a GLSL "#define X\n#define Y\n..." directive block from a comma-
// separated list (the engine convention used at every caller). Empty tokens
// are skipped.
std::string format_shader_defines(const std::string& comma_list);

// Load and preprocess a shader. `path` is resolved relative to the engine
// "Shaders\\" directory when `path_is_relative` is true, otherwise taken
// verbatim. `defines` must already be in directive form (use
// format_shader_defines() to build it). `version_prefix` is prepended verbatim;
// pass an empty string to omit the version line entirely.
ShaderSource load_shader_source(const std::string& path,
								const std::string& defines,
								bool path_is_relative = true,
								const char* version_prefix = "#version 460 core\n");
