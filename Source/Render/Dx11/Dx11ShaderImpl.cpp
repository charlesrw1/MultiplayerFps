// Dx11ShaderImpl — GLSL -> SPIR-V -> HLSL -> DXBC compile pipeline + a disk
// cache (keyed like OpenGlShaderImpl's program-binary cache, but storing DXBC
// blobs + resource-binding tables instead of a GL program binary).
#include "Dx11Local.h"
#include "Framework/Util.h"
#include "Framework/StringUtils.h"
#include "Framework/BinaryReadWrite.h"
#include "Render/ShaderSourceLoader.h"
#include "Render/SpirvCompile.h"

namespace {

constexpr const char* DX11_SHADER_PATH = "Shaders\\";

// ---------------------------------------------------------------------------
// One compiled stage: DXBC bytecode + the resource-binding table produced by
// spirv_to_hlsl (consumed by D3 to map engine binding slots -> HLSL
// registers).
// ---------------------------------------------------------------------------
struct StageResult
{
	std::vector<uint8_t> dxbc;
	std::vector<HlslResourceBinding> bindings;
	bool ok = false;
};

StageResult compile_stage(const std::string& path, const std::string& defines_directive,
						   bool path_is_relative, SpirvStage stage,
						   const char* target_profile, const std::string& debug_name) {
	StageResult out;
	ShaderSource src = load_shader_source(path, defines_directive, path_is_relative, "#version 460\n");
	if (src.empty()) {
		sys_print(Error, "Dx11: shader source load failed: %s\n", debug_name.c_str());
		return out;
	}
	SpirvBlob spirv = compile_glsl_to_spirv(stage, src.source, debug_name);
	if (!spirv.ok()) {
		sys_print(Error, "Dx11: glslang failed [%s]:\n%s\n", debug_name.c_str(), spirv.error.c_str());
		return out;
	}
	HlslBlob hlsl = spirv_to_hlsl(spirv, debug_name);
	if (!hlsl.ok()) {
		sys_print(Error, "Dx11: spirv-cross failed [%s]:\n%s\n", debug_name.c_str(), hlsl.error.c_str());
		return out;
	}
	DxbcBlob dxbc = compile_hlsl_to_dxbc(hlsl.source, target_profile, debug_name);
	if (!dxbc.ok()) {
		sys_print(Error, "Dx11: D3DCompile failed [%s]:\n%s\n", debug_name.c_str(), dxbc.error.c_str());
		return out;
	}
	out.dxbc = std::move(dxbc.code);
	out.bindings = std::move(hlsl.bindings);
	out.ok = true;
	return out;
}

// ---------------------------------------------------------------------------
// Disk cache. Keyed on source paths + defines (same convention as
// OpenGlShaderImpl's make_cache_filename), but with a distinct extension so
// DX11 and GL caches for the same shader don't collide in FileSys::SHADER_CACHE.
// ---------------------------------------------------------------------------
std::string make_cache_filename(std::span<const std::string> inputs) {
	std::string concatenated;
	for (auto& s : inputs)
		concatenated += s;
	return StringUtils::alphanumeric_hash(concatenated) + ".dx11bin";
}

void write_stage(FileWriter& w, const StageResult& stage) {
	w.write_int32((uint32_t)stage.dxbc.size());
	w.write_bytes_ptr(stage.dxbc.data(), stage.dxbc.size());
	w.write_int32((uint32_t)stage.bindings.size());
	for (auto& b : stage.bindings) {
		w.write_string(b.name);
		w.write_int32(b.spirv_binding);
		w.write_int32(b.register_index);
		w.write_int32((uint32_t)b.kind);
	}
}

void read_stage(BinaryReader& r, StageResult& stage) {
	uint32_t dxbc_len = r.read_int32();
	stage.dxbc.resize(dxbc_len);
	r.read_bytes_ptr(stage.dxbc.data(), dxbc_len);
	uint32_t num_bindings = r.read_int32();
	stage.bindings.resize(num_bindings);
	for (auto& b : stage.bindings) {
		r.read_string(b.name);
		b.spirv_binding = r.read_int32();
		b.register_index = r.read_int32();
		b.kind = (HlslRegisterKind)r.read_int32();
	}
	stage.ok = true;
}

// Returns false if the cache is absent/stale. On success fills `stages`.
bool try_load_cache(const std::string& cache_filename, std::span<const std::string> engine_relative_sources,
					 std::span<StageResult> stages) {
	auto cache_file = FileSys::open_read(cache_filename.c_str(), FileSys::SHADER_CACHE);
	if (!cache_file)
		return false;
	for (auto& src : engine_relative_sources) {
		auto src_file = FileSys::open_read_engine(src.c_str());
		if (!src_file || src_file->get_timestamp() > cache_file->get_timestamp())
			return false;
	}
	BinaryReader reader(cache_file.get());
	for (auto& stage : stages)
		read_stage(reader, stage);
	return true;
}

void save_cache(const std::string& cache_filename, std::span<const StageResult> stages) {
	FileWriter writer;
	for (auto& stage : stages)
		write_stage(writer, stage);
	auto out_file = FileSys::open_write(cache_filename.c_str(), FileSys::SHADER_CACHE);
	if (out_file)
		out_file->write(writer.get_buffer(), writer.get_size());
	else
		sys_print(Error, "Dx11: shader-cache write open failed: %s\n", cache_filename.c_str());
}

// ---------------------------------------------------------------------------
// Dx11ShaderImpl::reflect() — see Dx11Local.h for the class definition (shared
// with D3's set_pipeline / flush_binds).
// ---------------------------------------------------------------------------
void count_bindings(const std::vector<HlslResourceBinding>& bindings, IGraphicsShader::PerStageCounts& out) {
	for (auto& b : bindings) {
		switch (b.kind) {
		case HlslRegisterKind::CBV: out.num_uniform_buffers++; break;
		case HlslRegisterKind::Sampler: out.num_samplers++; break;
		case HlslRegisterKind::UAV: (b.is_image ? out.num_storage_textures : out.num_storage_buffers)++; break;
		case HlslRegisterKind::SRV: {
			// Sampled images get a paired Sampler entry at the same
			// register_index (assign_registers keeps both entries of a
			// combined image+sampler in sync). spirv_binding alone isn't
			// unique here - unrelated resources from different descriptor
			// sets can share the same original SPIR-V binding number.
			bool has_sampler = false;
			for (auto& s : bindings)
				if (s.kind == HlslRegisterKind::Sampler && s.register_index == b.register_index)
					has_sampler = true;
			if (!has_sampler)
				(b.is_image ? out.num_storage_textures : out.num_storage_buffers)++;
			break;
		}
		}
	}
}

Dx11ShaderImpl* make_vert_frag_shader(const StageResult& vert, const StageResult& frag) {
	Dx11ShaderImpl* shader = new Dx11ShaderImpl();
	HRESULT hr = g_dx11_device->CreateVertexShader(vert.dxbc.data(), vert.dxbc.size(), nullptr, shader->vs.GetAddressOf());
	ASSERT(SUCCEEDED(hr) && "Dx11: CreateVertexShader failed");
	hr = g_dx11_device->CreatePixelShader(frag.dxbc.data(), frag.dxbc.size(), nullptr, shader->ps.GetAddressOf());
	ASSERT(SUCCEEDED(hr) && "Dx11: CreatePixelShader failed");
	shader->vs_bytecode = vert.dxbc;
	shader->vs_bindings = vert.bindings;
	shader->ps_bindings = frag.bindings;
	return shader;
}

} // namespace

IGraphicsShader::Reflection Dx11ShaderImpl::reflect() {
	Reflection out{};
	count_bindings(vs_bindings, out.vertex);
	count_bindings(ps_bindings, out.fragment);
	count_bindings(cs_bindings, out.compute);
	return out;
}

IGraphicsShader* dx11_create_shader_vert_frag(const std::string& vert_path, const std::string& frag_path,
											   const std::string& defines) {
	const std::string defines_directive = format_shader_defines(defines);
	const std::string sources[] = {vert_path, frag_path, defines};
	const std::string cache_filename = make_cache_filename(sources);
	const std::string engine_relative[] = {std::string(DX11_SHADER_PATH) + vert_path,
											std::string(DX11_SHADER_PATH) + frag_path};

	StageResult vert{}, frag{};
	StageResult cached_stages[] = {vert, frag};
	if (try_load_cache(cache_filename, engine_relative, cached_stages))
		return make_vert_frag_shader(cached_stages[0], cached_stages[1]);

	vert = compile_stage(vert_path, defines_directive, true, SpirvStage::Vertex, "vs_5_0", vert_path);
	if (!vert.ok)
		return nullptr;
	frag = compile_stage(frag_path, defines_directive, true, SpirvStage::Fragment, "ps_5_0", frag_path);
	if (!frag.ok)
		return nullptr;

	StageResult stages[] = {vert, frag};
	save_cache(cache_filename, stages);
	return make_vert_frag_shader(vert, frag);
}

IGraphicsShader* dx11_create_shader_compute(const std::string& compute_path, const std::string& defines) {
	const std::string defines_directive = format_shader_defines(defines);
	StageResult comp = compile_stage(compute_path, defines_directive, true, SpirvStage::Compute, "cs_5_0", compute_path);
	if (!comp.ok)
		return nullptr;

	Dx11ShaderImpl* shader = new Dx11ShaderImpl();
	HRESULT hr = g_dx11_device->CreateComputeShader(comp.dxbc.data(), comp.dxbc.size(), nullptr, shader->cs.GetAddressOf());
	ASSERT(SUCCEEDED(hr) && "Dx11: CreateComputeShader failed");
	shader->cs_bindings = comp.bindings;
	return shader;
}

IGraphicsShader* dx11_create_shader_single_file(const std::string& shared_path, const std::string& defines) {
	const std::string defines_directive = format_shader_defines(defines);
	const std::string sources[] = {shared_path, defines};
	const std::string cache_filename = make_cache_filename(sources);
	const std::string engine_relative[] = {shared_path};

	StageResult vert{}, frag{};
	StageResult cached_stages[] = {vert, frag};
	if (try_load_cache(cache_filename, engine_relative, cached_stages))
		return make_vert_frag_shader(cached_stages[0], cached_stages[1]);

	vert = compile_stage(shared_path, defines_directive + "\n#define _VERTEX_SHADER\n#line 0\n", false,
						  SpirvStage::Vertex, "vs_5_0", shared_path);
	if (!vert.ok)
		return nullptr;
	frag = compile_stage(shared_path, defines_directive + "\n#define _FRAGMENT_SHADER\n#line 0\n", false,
						  SpirvStage::Fragment, "ps_5_0", shared_path);
	if (!frag.ok)
		return nullptr;

	StageResult stages[] = {vert, frag};
	save_cache(cache_filename, stages);
	return make_vert_frag_shader(vert, frag);
}
