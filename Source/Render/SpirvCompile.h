#pragma once
// GLSL -> SPIR-V compile, via glslang. Output is a uint32 word stream that
// SDL3 GPU's Vulkan backend can consume directly (or that SPIRV-Cross can
// reflect over).
//
// Source strings should be produced by ShaderSourceLoader::load_shader_source
// with version_prefix="#version 460\n" (no "core" keyword for Vulkan
// profile). glslang accepts the OpenGL-style "#version 460 core" too but
// emits a profile warning.
//
// Threading: glslang uses process-global state. spirv_compile_init() must be
// called before the first compile and spirv_compile_shutdown() at teardown.
// All compile_glsl_to_spirv calls must be on the same thread (typically the
// renderer thread). Added in Phase 3.1.

#include <cstdint>
#include <string>
#include <vector>

enum class SpirvStage
{
	Vertex,
	Fragment,
	Geometry,
	TessControl,
	TessEval,
	Compute
};

struct SpirvBlob
{
	std::vector<uint32_t> code;
	std::string error;  // empty on success

	bool ok() const { return error.empty() && !code.empty(); }
};

// One-time process init / shutdown. Idempotent — second call to init is a
// no-op, second shutdown is a no-op.
void spirv_compile_init();
void spirv_compile_shutdown();

// Compile a single shader stage. `glsl_source` must include a #version
// directive. `debug_name` (e.g. engine-relative shader path) is folded into
// any error message returned in SpirvBlob::error. Returns SpirvBlob with
// .error set on failure, or .code populated on success.
SpirvBlob compile_glsl_to_spirv(SpirvStage stage,
								const std::string& glsl_source,
								const std::string& debug_name);
