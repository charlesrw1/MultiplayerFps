// GLSL -> SPIR-V via glslang. Output drives the SDL3 GPU Vulkan backend in
// Phase 3 and the test_spirv_pipeline.cpp reflection cross-check in P3.1.

#include "SpirvCompile.h"
#include "Framework/Util.h"

#include <glslang/Public/ShaderLang.h>
#include <glslang/Public/ResourceLimits.h>
#include <glslang/SPIRV/GlslangToSpv.h>

#include <spirv_cross/spirv_hlsl.hpp>

#include <set>

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

bool spirv_is_initialized() {
	std::lock_guard<std::mutex> lk(g_init_mutex);
	return g_initialized;
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
	// gl_DrawID (SPIR-V BuiltIn DrawIndex) has no SM5.0/D3D11 equivalent and
	// spirv-cross's HLSL backend rejects it outright ("Unsupported builtin in
	// HLSL: 4426"). The engine's indirect-loop draws (r_indirect_loop=1, see
	// Dx11Device::multi_draw_elements_indirect) issue one DrawIndexedInstancedIndirect
	// per element with that element's StartInstanceLocation == its draw index
	// (the same convention used by the GL MDI path), so gl_BaseInstance is an
	// exact substitute here.
	shader.setPreamble(
		"#define gl_VertexID gl_VertexIndex\n"
		"#define gl_InstanceID gl_InstanceIndex\n"
		"#define gl_DrawID gl_BaseInstance\n"
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

// One resource pending a register assignment, before SPIRV-Cross compiles to
// HLSL. `cbv`/`srv`/`uav`/`sampler` mark which independent HLSL register
// space(s) this resource occupies (a combined image+sampler occupies both
// srv and sampler, at the same index).
struct PendingResource
{
	spirv_cross::ID id;
	std::string name;
	uint32_t orig_binding = 0;
	bool cbv = false, srv = false, uav = false, sampler = false;
	bool is_image = false;
};

// GLSL/SPIR-V binding decorations share one namespace across all resource
// types, but HLSL register spaces (b/t/u/s) are independent and SM5.0 caps
// cbuffers at b0-b13. The raw SPIR-V binding can therefore (a) collide with
// another resource in the same HLSL space (X4500 "overlapping register
// semantics", e.g. a sampled image and a read-only SSBO both at binding 1 ->
// both want t1), or (b) exceed b13 for cbuffers/push-constant blocks (X4567,
// e.g. push constants conventionally live at binding 16 -> b16).
//
// Fix: re-decorate each resource with a register index that's free within
// its space(s) (preferring the original binding if it already fits), before
// calling compile(). `HlslResourceBinding::spirv_binding` keeps the original
// SPIR-V binding (engine-side bind tables are keyed by it);
// `register_index` is the post-remap HLSL register.
void assign_registers(spirv_cross::CompilerHLSL& comp, std::vector<PendingResource>& pending) {
	std::set<uint32_t> cbv_used, srv_used, uav_used, sampler_used;
	auto fits = [&](const PendingResource& p, uint32_t idx) {
		if (p.cbv && (idx >= 14 || cbv_used.count(idx)))
			return false;
		if (p.srv && srv_used.count(idx))
			return false;
		if (p.uav && uav_used.count(idx))
			return false;
		if (p.sampler && sampler_used.count(idx))
			return false;
		return true;
	};
	for (auto& p : pending) {
		uint32_t idx = p.orig_binding;
		if (!fits(p, idx)) {
			idx = 0;
			while (!fits(p, idx))
				idx++;
		}
		if (p.cbv)     cbv_used.insert(idx);
		if (p.srv)     srv_used.insert(idx);
		if (p.uav)     uav_used.insert(idx);
		if (p.sampler) sampler_used.insert(idx);
		if (idx != p.orig_binding)
			comp.set_decoration(p.id, spv::DecorationBinding, idx);
	}
}

void add_pending(std::vector<PendingResource>& out, const spirv_cross::Compiler& comp,
				  const spirv_cross::Resource& res, bool cbv, bool srv, bool uav, bool sampler,
				  bool is_image = false) {
	if (!comp.has_decoration(res.id, spv::DecorationBinding))
		return;
	PendingResource p;
	p.id = res.id;
	p.name = res.name;
	p.orig_binding = comp.get_decoration(res.id, spv::DecorationBinding);
	p.cbv = cbv; p.srv = srv; p.uav = uav; p.sampler = sampler;
	p.is_image = is_image;
	out.push_back(std::move(p));
}

HlslRegisterKind primary_kind(const PendingResource& p) {
	if (p.cbv) return HlslRegisterKind::CBV;
	if (p.uav) return HlslRegisterKind::UAV;
	if (p.srv) return HlslRegisterKind::SRV;
	return HlslRegisterKind::Sampler;
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

		const auto active = comp.get_active_interface_variables();
		const auto resources = comp.get_shader_resources(active);

		std::vector<PendingResource> pending;
		// Uniform buffers are handled from the *unfiltered* resource list:
		// glslang's OpEntryPoint interface list doesn't reliably include
		// Uniform-storage-class blocks, so an in-use UBO at binding>=14 (e.g.
		// the binding=16 fragment push-constants convention) can be missing
		// from `active` and skip the b0-b13 remap below, producing X4567
		// ("manual bind to slot 16 failed"). Remapping an unused UBO's
		// decoration is harmless - compile() simply won't emit it.
		for (const auto& r : comp.get_shader_resources().uniform_buffers)
			add_pending(pending, comp, r, /*cbv*/true, false, false, false);
		for (const auto& r : resources.storage_buffers) {
			const bool read_only = comp.has_decoration(r.id, spv::DecorationNonWritable);
			add_pending(pending, comp, r, false, /*srv*/read_only, /*uav*/!read_only, false);
		}
		for (const auto& r : resources.storage_images) {
			const bool read_only = comp.has_decoration(r.id, spv::DecorationNonWritable);
			add_pending(pending, comp, r, false, /*srv*/read_only, /*uav*/!read_only, false, /*is_image*/true);
		}
		for (const auto& r : resources.separate_images)
			add_pending(pending, comp, r, false, /*srv*/true, false, false);
		for (const auto& r : resources.separate_samplers)
			add_pending(pending, comp, r, false, false, false, /*sampler*/true);
		for (const auto& r : resources.sampled_images)
			// Combined image+sampler: HLSL backend splits into a Texture (SRV)
			// and a SamplerState (Sampler), both at the same register index.
			add_pending(pending, comp, r, false, /*srv*/true, false, /*sampler*/true);

		assign_registers(comp, pending);

		out.source = comp.compile();

		// FXC's default loop-unroll heuristic chokes on loops with a
		// dynamic (uniform-buffer-driven) trip count that also contain
		// gradient instructions (texture .Sample), e.g. light-list loops in
		// LightAccumulationFullScreen.txt: "X3511: unable to unroll loop...".
		// GLSL/SPIR-V has no equivalent annotation, so force [loop] (real
		// control flow, no unrolling) on every "for (" SPIRV-Cross emits.
		{
			const std::string from = "for (";
			const std::string to = "[loop] for (";
			size_t pos = 0;
			while ((pos = out.source.find(from, pos)) != std::string::npos) {
				out.source.replace(pos, from.size(), to);
				pos += to.size();
			}
		}

		for (const auto& p : pending) {
			HlslResourceBinding b;
			b.name = p.name;
			b.spirv_binding = p.orig_binding;
			b.register_index = comp.get_decoration(p.id, spv::DecorationBinding);
			b.kind = primary_kind(p);
			b.is_image = p.is_image;
			out.bindings.push_back(b);
			if (p.cbv && p.sampler) ASSERT(false && "cbv+sampler resource unexpected");
			// Sampled images need a second binding entry for the Sampler register.
			if (p.srv && p.sampler) {
				HlslResourceBinding sb = b;
				sb.kind = HlslRegisterKind::Sampler;
				out.bindings.push_back(sb);
			}
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
