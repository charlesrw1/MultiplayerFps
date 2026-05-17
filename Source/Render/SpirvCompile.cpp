// GLSL -> SPIR-V via glslang. Output drives the SDL3 GPU Vulkan backend in
// Phase 3 and the test_spirv_pipeline.cpp reflection cross-check in P3.1.

#include "SpirvCompile.h"
#include "Framework/Util.h"

#include <glslang/Public/ShaderLang.h>
#include <glslang/Public/ResourceLimits.h>
#include <glslang/SPIRV/GlslangToSpv.h>

#include <mutex>

// Static libs from vcpkg. Linker picks up via vcpkg's MSBuild-integration
// lib-search-path injection. Debug variants have a "d" suffix.
#ifdef _DEBUG
#  pragma comment(lib, "glslangd.lib")
#  pragma comment(lib, "glslang-default-resource-limitsd.lib")
#  pragma comment(lib, "SPIRVd.lib")
#else
#  pragma comment(lib, "glslang.lib")
#  pragma comment(lib, "glslang-default-resource-limits.lib")
#  pragma comment(lib, "SPIRV.lib")
#endif

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
