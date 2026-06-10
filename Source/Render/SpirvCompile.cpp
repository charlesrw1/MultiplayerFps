// GLSL -> SPIR-V via glslang. Output drives the SDL3 GPU Vulkan backend in
// Phase 3 and the test_spirv_pipeline.cpp reflection cross-check in P3.1.

#include "SpirvCompile.h"
#include "Framework/Util.h"

#include <glslang/Public/ShaderLang.h>
#include <glslang/Public/ResourceLimits.h>
#include <glslang/SPIRV/GlslangToSpv.h>

#include <spirv_cross/spirv_hlsl.hpp>

// d3dcompiler.h pulls in <windows.h>, which defines min/max macros that
// collide with std::/glm:: min/max calls in other TUs of this unity build.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <d3dcompiler.h>
#include <wrl/client.h>

// windows.h's old memory-model macros collide with `near`/`far` parameter
// and local-variable names elsewhere in this unity build.
#undef near
#undef far

#include <mutex>

// Static libs from vcpkg. Linker picks up via vcpkg's MSBuild-integration
// lib-search-path injection. Debug variants have a "d" suffix.
#ifdef _DEBUG
#  pragma comment(lib, "glslangd.lib")
#  pragma comment(lib, "glslang-default-resource-limitsd.lib")
#  pragma comment(lib, "SPIRVd.lib")
#  pragma comment(lib, "spirv-cross-cored.lib")
#  pragma comment(lib, "spirv-cross-glsld.lib")
#  pragma comment(lib, "spirv-cross-hlsld.lib")
#else
#  pragma comment(lib, "glslang.lib")
#  pragma comment(lib, "glslang-default-resource-limits.lib")
#  pragma comment(lib, "SPIRV.lib")
#  pragma comment(lib, "spirv-cross-core.lib")
#  pragma comment(lib, "spirv-cross-glsl.lib")
#  pragma comment(lib, "spirv-cross-hlsl.lib")
#endif

#pragma comment(lib, "d3dcompiler.lib")

namespace {

bool g_initialized = false;
std::mutex g_init_mutex;

EShLanguage to_eshlang(SpirvStage s) {
	switch (s) {
	case SpirvStage::Vertex:      return EShLangVertex;
	case SpirvStage::Fragment:    return EShLangFragment;
	case SpirvStage::Geometry:    return EShLangGeometry;
	case SpirvStage::TessControl: return EShLangTessControl;
	case SpirvStage::TessEval:    return EShLangTessEvaluation;
	case SpirvStage::Compute:     return EShLangCompute;
	}
	return EShLangVertex;
}

} // namespace

void spirv_compile_init() {
	std::lock_guard<std::mutex> lk(g_init_mutex);
	if (g_initialized) return;
	glslang::InitializeProcess();
	g_initialized = true;
}

void spirv_compile_shutdown() {
	std::lock_guard<std::mutex> lk(g_init_mutex);
	if (!g_initialized) return;
	glslang::FinalizeProcess();
	g_initialized = false;
}

SpirvBlob compile_glsl_to_spirv(SpirvStage stage,
								const std::string& glsl_source,
								const std::string& debug_name) {
	SpirvBlob out;
	ASSERT(g_initialized && "spirv_compile_init() must be called first");

	const EShLanguage lang = to_eshlang(stage);
	glslang::TShader shader(lang);

	const char* src_str = glsl_source.c_str();
	const int   src_len = (int)glsl_source.size();
	const char* src_name = debug_name.c_str();
	shader.setStringsWithLengthsAndNames(&src_str, &src_len, &src_name, 1);

	// Shaders are authored for OpenGL semantics (gl_VertexID, gl_InstanceID).
	// Vulkan SPIR-V exposes the same vertex/instance index under gl_VertexIndex
	// / gl_InstanceIndex. Alias them via preamble so the existing source tree
	// compiles unchanged for both backends. glslang's setPreamble injects the
	// text immediately after the #version directive.
	shader.setPreamble(
		"#define gl_VertexID gl_VertexIndex\n"
		"#define gl_InstanceID gl_InstanceIndex\n"
	);

	// Vulkan client, SPIR-V target. Matches what SDL_GPU_SHADERFORMAT_SPIRV
	// expects from SDL3 GPU's Vulkan backend.
	shader.setEnvInput(glslang::EShSourceGlsl, lang, glslang::EShClientVulkan, 100);
	shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_2);
	shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_5);

	const TBuiltInResource* resources = GetDefaultResources();
	const int default_version = 460;
	const EProfile default_profile = ECoreProfile;
	const bool force_default_version_and_profile = false;
	const bool forward_compatible = false;
	const EShMessages messages = (EShMessages)(EShMsgDefault | EShMsgSpvRules | EShMsgVulkanRules);

	if (!shader.parse(resources, default_version, default_profile,
					  force_default_version_and_profile, forward_compatible, messages)) {
		out.error = std::string("glslang parse failed [") + debug_name + "]:\n" +
					shader.getInfoLog() + shader.getInfoDebugLog();
		return out;
	}

	glslang::TProgram program;
	program.addShader(&shader);
	if (!program.link(messages)) {
		out.error = std::string("glslang link failed [") + debug_name + "]:\n" +
					program.getInfoLog() + program.getInfoDebugLog();
		return out;
	}

	glslang::SpvOptions spv_options{};
	spv_options.generateDebugInfo = false;
	spv_options.stripDebugInfo = true;
	spv_options.disableOptimizer = true;  // P3.1 spike — optimize later

	glslang::GlslangToSpv(*program.getIntermediate(lang), out.code, &spv_options);
	if (out.code.empty()) {
		out.error = std::string("GlslangToSpv produced empty SPIR-V for ") + debug_name;
	}
	return out;
}

namespace {

// Records one resource's (name, spirv binding, register kind) into `out`.
// `register_index` mirrors the spirv binding: with no HLSL_BINDING_AUTO_*
// flags set, SPIRV-Cross's HLSL backend uses the SPIR-V binding decoration
// directly as the register index within each type's register space (t/s/b/u
// are independent spaces, so no cross-type collisions from the shared GLSL
// binding namespace).
void add_binding(std::vector<HlslResourceBinding>& out, const spirv_cross::Compiler& comp,
				  const spirv_cross::Resource& res, HlslRegisterKind kind) {
	if (!comp.has_decoration(res.id, spv::DecorationBinding))
		return;
	HlslResourceBinding b;
	b.name = res.name;
	b.spirv_binding = comp.get_decoration(res.id, spv::DecorationBinding);
	b.register_index = b.spirv_binding;
	b.kind = kind;
	out.push_back(std::move(b));
}

} // namespace

HlslBlob spirv_to_hlsl(const SpirvBlob& spirv, const std::string& debug_name) {
	HlslBlob out;
	if (!spirv.ok()) {
		out.error = std::string("spirv_to_hlsl called with invalid SPIR-V [") + debug_name + "]";
		return out;
	}

	try {
		spirv_cross::CompilerHLSL comp(spirv.code);

		spirv_cross::CompilerHLSL::Options opts;
		opts.shader_model = 50;
		opts.support_nonzero_base_vertex_base_instance = true;
		comp.set_hlsl_options(opts);

		out.source = comp.compile();

		const auto active = comp.get_active_interface_variables();
		const auto resources = comp.get_shader_resources(active);

		for (const auto& r : resources.uniform_buffers)
			add_binding(out.bindings, comp, r, HlslRegisterKind::CBV);
		for (const auto& r : resources.storage_buffers) {
			const bool read_only = comp.has_decoration(r.id, spv::DecorationNonWritable);
			add_binding(out.bindings, comp, r, read_only ? HlslRegisterKind::SRV : HlslRegisterKind::UAV);
		}
		for (const auto& r : resources.storage_images) {
			const bool read_only = comp.has_decoration(r.id, spv::DecorationNonWritable);
			add_binding(out.bindings, comp, r, read_only ? HlslRegisterKind::SRV : HlslRegisterKind::UAV);
		}
		for (const auto& r : resources.separate_images)
			add_binding(out.bindings, comp, r, HlslRegisterKind::SRV);
		for (const auto& r : resources.separate_samplers)
			add_binding(out.bindings, comp, r, HlslRegisterKind::Sampler);
		for (const auto& r : resources.sampled_images) {
			// Combined image+sampler: HLSL backend splits into a Texture (SRV)
			// and a SamplerState (Sampler), both at the same register index.
			add_binding(out.bindings, comp, r, HlslRegisterKind::SRV);
			add_binding(out.bindings, comp, r, HlslRegisterKind::Sampler);
		}

		if (out.source.empty())
			out.error = std::string("SPIRV-Cross produced empty HLSL for ") + debug_name;
	} catch (const std::exception& e) {
		out.error = std::string("SPIRV-Cross HLSL failed [") + debug_name + "]: " + e.what();
	}
	return out;
}

DxbcBlob compile_hlsl_to_dxbc(const std::string& hlsl_source,
							  const std::string& target_profile,
							  const std::string& debug_name) {
	DxbcBlob out;

	UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
	flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

	Microsoft::WRL::ComPtr<ID3DBlob> code_blob;
	Microsoft::WRL::ComPtr<ID3DBlob> error_blob;
	const HRESULT hr = D3DCompile(hlsl_source.data(), hlsl_source.size(),
								   debug_name.c_str(), nullptr, nullptr,
								   "main", target_profile.c_str(), flags, 0,
								   code_blob.GetAddressOf(), error_blob.GetAddressOf());
	if (FAILED(hr)) {
		out.error = std::string("D3DCompile failed [") + debug_name + ", " + target_profile + "]";
		if (error_blob)
			out.error += std::string(":\n") + (const char*)error_blob->GetBufferPointer();
		return out;
	}

	const uint8_t* begin = (const uint8_t*)code_blob->GetBufferPointer();
	out.code.assign(begin, begin + code_blob->GetBufferSize());
	return out;
}
