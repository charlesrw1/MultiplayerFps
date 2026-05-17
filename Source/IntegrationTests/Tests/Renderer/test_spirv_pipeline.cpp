// Phase 3.1 toolchain spike: validate end-to-end GLSL -> SPIR-V -> reflection
// before any SDL3 GPU backend code lands. For each shader:
//   1. Load source via ShaderSourceLoader (#version 460 for Vulkan profile).
//   2. Compile to SPIR-V via glslang (SpirvCompile.cpp).
//   3. Run SPIRV-Cross reflection on the resulting blob.
//   4. Cross-check counts against IGraphicsShader::reflect() (the GL-side
//      reflection that B2 landed).
//
// If glslang rejects a shader, sys_print logs the full glslang diagnostic
// and the test fails on the require() — data we need before committing to
// the SDL3 backend in P3.2+.
//
// Lib note: spirv-cross is linked via #pragma comment(lib) below; glslang
// is linked from SpirvCompile.cpp inside CsRemake.lib.

#include "IntegrationTests/TestContext.h"
#include "IntegrationTests/TestRegistry.h"
#include "GameEnginePublic.h"
#include "Framework/Util.h"
#include "Render/IGraphicsDevice.h"
#include "Render/ShaderSourceLoader.h"
#include "Render/SpirvCompile.h"

#include <spirv_cross/spirv_cross.hpp>

#ifdef _DEBUG
#  pragma comment(lib, "spirv-cross-cored.lib")
#else
#  pragma comment(lib, "spirv-cross-core.lib")
#endif

namespace {

struct SpirvCounts
{
	int num_samplers = 0;          // combined sampler2D / samplerCube / etc.
	int num_storage_textures = 0;  // image2D / imageCube (writable)
	int num_storage_buffers = 0;   // SSBOs
	int num_uniform_buffers = 0;   // UBOs (incl. push-const blocks at binding 12+)
};

SpirvCounts reflect_spirv(const SpirvBlob& blob, const char* tag) {
	SpirvCounts c;
	try {
		spirv_cross::Compiler comp(blob.code.data(), blob.code.size());
		// Filter to statically-referenced resources only. GL's reflection
		// (GL_REFERENCED_BY_*_SHADER) reports the linker-active set; the
		// default get_shader_resources() reports every *declared* resource
		// including dead samplers like VfogScatteringC's probe_irradiance
		// (declared, never sampled). SDL3's SDL_GPUShaderCreateInfo wants
		// the active count, so the SDL3 backend must use the same filter.
		const auto active = comp.get_active_interface_variables();
		const auto res    = comp.get_shader_resources(active);
		c.num_samplers         = (int)res.sampled_images.size();
		c.num_storage_textures = (int)res.storage_images.size();
		c.num_storage_buffers  = (int)res.storage_buffers.size();
		c.num_uniform_buffers  = (int)res.uniform_buffers.size();
	} catch (const std::exception& e) {
		sys_print(Error, "spirv-cross threw on %s: %s\n", tag, e.what());
	}
	return c;
}

// Compile + reflect a single shader stage from its own source file.
// Returns true on success; logs glslang diagnostics on failure.
bool compile_and_reflect(const char* path, const std::string& defines,
						 SpirvStage stage, SpirvCounts& out) {
	const std::string formatted_defines = format_shader_defines(defines);
	const ShaderSource src = load_shader_source(path, formatted_defines, true, "#version 460\n");
	if (src.empty()) {
		sys_print(Error, "load_shader_source failed for %s\n", path);
		return false;
	}
	const SpirvBlob blob = compile_glsl_to_spirv(stage, src.source, path);
	if (!blob.ok()) {
		sys_print(Error, "glslang failed [%s]:\n%s\n", path, blob.error.c_str());
		return false;
	}
	out = reflect_spirv(blob, path);
	return true;
}

} // namespace

// ============================================================================
// Smoke — glslang init + a trivial fullscreen-quad VS round-trip. Cheapest
// possible compile; if this fails, none of the others can pass.
// ============================================================================
static TestTask test_spirv_smoke(TestContext& t) {
	eng->load_level("");
	co_await t.wait_ticks(1);

	spirv_compile_init();

	SpirvCounts vs{};
	const bool ok = compile_and_reflect("fullscreenquad.txt", "", SpirvStage::Vertex, vs);
	t.require(ok, "fullscreenquad VS round-trip");
}
GAME_TEST("renderer/spirv_smoke", 10.f, test_spirv_smoke);

// ============================================================================
// MbTextured (vert+frag, TEXTURE_2D_VERSION) — cross-check vs
// test_shader_reflection_basic.
// ============================================================================
static TestTask test_spirv_mb_textured(TestContext& t) {
	eng->load_level("");
	co_await t.wait_ticks(1);

	spirv_compile_init();

	SpirvCounts vs{}, fs{};
	t.require(compile_and_reflect("MbTexturedV.txt", "TEXTURE_2D_VERSION", SpirvStage::Vertex, vs),
			  "MbTexturedV -> SPIR-V");
	t.require(compile_and_reflect("MbTexturedF.txt", "TEXTURE_2D_VERSION", SpirvStage::Fragment, fs),
			  "MbTexturedF -> SPIR-V");

	IGraphicsShader* shader = gfx().create_shader_vert_frag(
		"MbTexturedV.txt", "MbTexturedF.txt", "TEXTURE_2D_VERSION");
	t.require(shader != nullptr, "MbTextured GL shader compiled");
	auto gl = shader->reflect();
	shader->release();

	t.check(vs.num_uniform_buffers  == gl.vertex.num_uniform_buffers,   "VS UBO count matches GL");
	t.check(vs.num_samplers         == gl.vertex.num_samplers,          "VS sampler count matches GL");
	t.check(vs.num_storage_buffers  == gl.vertex.num_storage_buffers,   "VS SSBO count matches GL");
	t.check(vs.num_storage_textures == gl.vertex.num_storage_textures,  "VS image count matches GL");

	t.check(fs.num_uniform_buffers  == gl.fragment.num_uniform_buffers, "FS UBO count matches GL");
	t.check(fs.num_samplers         == gl.fragment.num_samplers,        "FS sampler count matches GL");
	t.check(fs.num_storage_buffers  == gl.fragment.num_storage_buffers, "FS SSBO count matches GL");
	t.check(fs.num_storage_textures == gl.fragment.num_storage_textures,"FS image count matches GL");
}
GAME_TEST("renderer/spirv_mb_textured", 15.f, test_spirv_mb_textured);

// ============================================================================
// Bloom downsample — cross-check vs test_shader_reflection_bloom.
// ============================================================================
static TestTask test_spirv_bloom(TestContext& t) {
	eng->load_level("");
	co_await t.wait_ticks(1);

	spirv_compile_init();

	SpirvCounts vs{}, fs{};
	t.require(compile_and_reflect("fullscreenquad.txt", "", SpirvStage::Vertex, vs),
			  "fullscreenquad VS -> SPIR-V");
	t.require(compile_and_reflect("BloomDownsampleF.txt", "", SpirvStage::Fragment, fs),
			  "BloomDownsampleF -> SPIR-V");

	IGraphicsShader* shader = gfx().create_shader_vert_frag(
		"fullscreenquad.txt", "BloomDownsampleF.txt");
	t.require(shader != nullptr, "Bloom GL shader compiled");
	auto gl = shader->reflect();
	shader->release();

	t.check(vs.num_uniform_buffers  == gl.vertex.num_uniform_buffers,   "VS UBO count matches GL");
	t.check(fs.num_uniform_buffers  == gl.fragment.num_uniform_buffers, "FS UBO count matches GL");
	t.check(fs.num_samplers         == gl.fragment.num_samplers,        "FS sampler count matches GL");
}
GAME_TEST("renderer/spirv_bloom", 15.f, test_spirv_bloom);

// ============================================================================
// CullCompute (MAINVIEW) — heavy compute, multi-SSBO + UBO + sampler.
// ============================================================================
static TestTask test_spirv_cull_compute(TestContext& t) {
	eng->load_level("");
	co_await t.wait_ticks(1);

	spirv_compile_init();

	SpirvCounts cs{};
	t.require(compile_and_reflect("CullCompute.txt", "MAINVIEW", SpirvStage::Compute, cs),
			  "CullCompute -> SPIR-V");

	IGraphicsShader* shader = gfx().create_shader_compute("CullCompute.txt", "MAINVIEW");
	t.require(shader != nullptr, "CullCompute GL shader compiled");
	auto gl = shader->reflect();
	shader->release();

	t.check(cs.num_samplers         == gl.compute.num_samplers,         "CS sampler count matches GL");
	t.check(cs.num_storage_textures == gl.compute.num_storage_textures, "CS image count matches GL");
	t.check(cs.num_uniform_buffers  == gl.compute.num_uniform_buffers,  "CS UBO count matches GL");
	t.check(cs.num_storage_buffers  == gl.compute.num_storage_buffers,  "CS SSBO count matches GL");
}
GAME_TEST("renderer/spirv_cull_compute", 15.f, test_spirv_cull_compute);

// ============================================================================
// VfogScatteringC — second compute exercise (shared VfogShared.txt include).
// ============================================================================
static TestTask test_spirv_vfog(TestContext& t) {
	eng->load_level("");
	co_await t.wait_ticks(1);

	spirv_compile_init();

	SpirvCounts cs{};
	t.require(compile_and_reflect("VfogScatteringC.txt", "", SpirvStage::Compute, cs),
			  "VfogScatteringC -> SPIR-V");

	IGraphicsShader* shader = gfx().create_shader_compute("VfogScatteringC.txt");
	t.require(shader != nullptr, "VfogScatteringC GL shader compiled");
	auto gl = shader->reflect();
	shader->release();

	t.check(cs.num_samplers         == gl.compute.num_samplers,         "CS sampler count matches GL");
	t.check(cs.num_storage_textures == gl.compute.num_storage_textures, "CS image count matches GL");
	t.check(cs.num_uniform_buffers  == gl.compute.num_uniform_buffers,  "CS UBO count matches GL");
	t.check(cs.num_storage_buffers  == gl.compute.num_storage_buffers,  "CS SSBO count matches GL");
}
GAME_TEST("renderer/spirv_vfog", 15.f, test_spirv_vfog);
