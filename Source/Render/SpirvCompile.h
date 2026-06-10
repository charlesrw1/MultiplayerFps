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

// One D3D resource binding produced by spirv-cross's HLSL backend, keyed by
// the SPIR-V `layout(binding=N)` value it was translated from. With default
// CompilerHLSL options the register index equals the SPIR-V binding number
// (per-type register space: t/s/b/u are independent), but DX11 backend code
// (D2) should read this table rather than assume that.
enum class HlslRegisterKind
{
	CBV,     // cbuffer      -> register(bN)
	SRV,     // texture/SSBO -> register(tN)
	UAV,     // RW resource  -> register(uN)
	Sampler, // sampler      -> register(sN)
};

struct HlslResourceBinding
{
	std::string name;
	uint32_t spirv_binding = 0;
	uint32_t register_index = 0;
	HlslRegisterKind kind{};
};

struct HlslBlob
{
	std::string source;
	std::vector<HlslResourceBinding> bindings;
	std::string error;  // empty on success

	bool ok() const { return error.empty() && !source.empty(); }
};

// SPIR-V -> HLSL source via SPIRV-Cross (CompilerHLSL, shader model 5.0).
// Entry point is always "main" (SPIRV-Cross's default for the HLSL backend).
// `debug_name` is folded into any error message.
HlslBlob spirv_to_hlsl(const SpirvBlob& spirv, const std::string& debug_name);

struct DxbcBlob
{
	std::vector<uint8_t> code;
	std::string error;  // empty on success

	bool ok() const { return error.empty() && !code.empty(); }
};

// HLSL source -> DXBC bytecode via D3DCompile. `target_profile` e.g.
// "vs_5_0", "ps_5_0", "cs_5_0" — pick from SpirvStage by the caller since
// only D3DCompile needs the profile string. `debug_name` is folded into any
// error message.
DxbcBlob compile_hlsl_to_dxbc(const std::string& hlsl_source,
							  const std::string& target_profile,
							  const std::string& debug_name);
