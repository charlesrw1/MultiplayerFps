#include "MaterialLocal.h"
#include "Framework/Files.h"
#include "AssetCompile/Someutils.h"
#include "DrawLocal.h"
#include "Render/Texture.h"
#include "Render/Model.h"
#include "glad/glad.h"
#include "IGraphicsDevice.h"
#include "OpenGlDeviceLocal.h"
#include "Assets/AssetDatabase.h"

#include <array>
#include <sstream>

// ---------------------------------------------------------------------------
// MaterialManagerLocal – dirty list management
// ---------------------------------------------------------------------------

void MaterialManagerLocal::add_to_dirty_list(MaterialInstance* mat) {
	ASSERT(mat);
	if (mat->impl)
		mat->impl->texture_id_hash = std::nullopt;
	dirty_list.insert(mat);
}

void MaterialManagerLocal::remove_from_dirty_list_if_it_is(MaterialInstance* mat) {
	ASSERT(mat);
	dirty_list.remove(mat);
}

void MaterialManagerLocal::free_material_instance(MaterialInstance* m) {
	ASSERT(m);
	if (!m->impl)
		return;
	remove_from_dirty_list_if_it_is(m);
	if (m->impl->gpu_buffer_offset != MaterialImpl::INVALID_MAPPING) {
		mat_offset_table->unregister_material(m);
	}
}

// ---------------------------------------------------------------------------
// MaterialManagerLocal – dynamic material management
// ---------------------------------------------------------------------------

MaterialInstance* MaterialManagerLocal::create_dynmaic_material_unsafier(const MaterialInstance* parent) {
	ASSERT(parent);
	std::shared_ptr<MaterialInstance> as_sptr = g_assets.find_sync_sptr<MaterialInstance>(parent->get_name());

	MaterialInstance* dynamic_mat = dynamic_mat_allocator.allocate_dynamic();
	ASSERT(dynamic_mat);

	dynamic_mat->impl = std::make_unique<MaterialImpl>();
	dynamic_mat->impl->self = dynamic_mat;
	dynamic_mat->impl->init_from(as_sptr);
	dynamic_mat->impl->is_dynamic_material = true;
	dynamic_mat->impl->post_load(dynamic_mat);

	return dynamic_mat;
}

void MaterialManagerLocal::free_dynamic_material(MaterialInstance* mat) {
	ASSERT(mat || !mat); // mat may be null (guard at call site)
	if (!mat)
		return;
	dynamic_mat_allocator.free_dynamic(mat);
}

// ---------------------------------------------------------------------------
// MaterialManagerLocal – reload / shader hooks
// ---------------------------------------------------------------------------

void MaterialManagerLocal::on_reloaded_material(MaterialInstance* mat) {
	ASSERT(mat);
	sys_print(Debug, "material reloaded from disk %s\n", mat->get_name().c_str());
	BuildSceneData_CpuFast::inst->rebuild_models();
}

void MaterialManagerLocal::on_reload_shader_invoke() {}

// ---------------------------------------------------------------------------
// MaterialManagerLocal – init
// ---------------------------------------------------------------------------

void MaterialManagerLocal::init() {
	ASSERT(!gpuMatBufferPtr); // should only be called once
	draw.on_reload_shaders.add(this, &MaterialManagerLocal::on_reload_shader_invoke);

	auto create_buffer = [&]() {
		CreateBufferArgs args;
		args.size = MATERIAL_SIZE * MAX_MATERIALS;
		args.flags = GraphicsBufferUseFlags(BUFFER_USE_DYNAMIC | BUFFER_USE_AS_STORAGE_READ);
		return gfx().create_buffer(args);
	};
	gpuMatBufferPtr = create_buffer();

	materialBufferSize = MATERIAL_SIZE * MAX_MATERIALS;
	mat_offset_table = std::make_unique<AllMaterialTable>(MAX_MATERIALS);

	fallback = g_assets.find_sync_sptr<MaterialInstance>("eng/fallback.mm");
	if (!fallback)
		Fatalf("couldnt load the fallback master material\n");

	defaultBillboard = g_assets.find_sync_sptr<MaterialInstance>("eng/billboardDefault.mm");
	if (!defaultBillboard)
		Fatalf("couldnt load the default billboard material\n");

	pp_editor_select_mat = g_assets.find_sync_sptr<MaterialInstance>("eng/defaultEditorSelect.mm");
	if (!pp_editor_select_mat)
		Fatalf("couldnt load the default editor select material\n");

	prewarm_manifest();
}

void MaterialManagerLocal::prewarm_manifest() {
	// Workers read only the cache files named by this manifest. After they
	// finish, get_mat_shader executes glProgramBinary on this main thread and
	// installs the result in mat_shader_table.
	is_prewarming_manifest = true;
	auto file = FileSys::open_read_engine("Shaders/shader_prewarm.txt");
	if (!file) {
		sys_print(Warning, "shader prewarm manifest missing\n");
		is_prewarming_manifest = false;
		return;
	}
	std::string text(file->size(), '\0');
	file->read(text.data(), text.size());
	std::istringstream lines(text);
	std::string line;
	int count = 0;
	auto trim = [](std::string value) {
		const size_t first = value.find_first_not_of(" \t\r");
		if (first == std::string::npos) return std::string{};
		const size_t last = value.find_last_not_of(" \t\r");
		return value.substr(first, last - first + 1);
	};
	auto parse_flags = [](const std::string& value) {
		int flags = 0;
		std::istringstream tokens(value);
		std::string token;
		while (std::getline(tokens, token, ',')) {
			if (token == "ANIMATED") flags |= MSF_ANIMATED;
			else if (token == "EDITOR_ID") flags |= MSF_EDITOR_ID;
			else if (token == "DEPTH_ONLY") flags |= MSF_DEPTH_ONLY;
			else if (token == "DITHER") flags |= MSF_DITHER;
			else if (token == "NO_TAA") flags |= MSF_NO_TAA;
			else if (token == "MAT_WITH_INST") flags |= MSF_MATERIAL_IN_INSTANCE;
			else if (!token.empty()) sys_print(Warning, "shader prewarm: unknown flag %s\n", token.c_str());
		}
		return flags;
	};
	auto make_defines = [](int flags) {
		std::string defines;
		auto add = [&](const char* s) { if (!defines.empty()) defines += ','; defines += s; };
		if (flags & MSF_ANIMATED) add("ANIMATED"); if (flags & MSF_DITHER) add("DITHER");
		if (flags & MSF_EDITOR_ID) add("EDITOR_ID"); if (flags & MSF_DEPTH_ONLY) add("DEPTH_ONLY");
		if (flags & MSF_NO_TAA) add("NO_TAA"); if (flags & MSF_MATERIAL_IN_INSTANCE) add("MAT_WITH_INST");
		return defines;
	};
	std::vector<std::pair<std::shared_ptr<MaterialInstance>, int>> entries;
	while (std::getline(lines, line)) {
		const size_t comment = line.find('#');
		if (comment != std::string::npos) line.resize(comment);
		const size_t split = line.find('|');
		const std::string path = trim(line.substr(0, split));
		if (path.empty()) continue;
		auto material = g_assets.find_sync_sptr<MaterialInstance>(path);
		if (!material || !material->is_valid_to_use()) { sys_print(Warning, "shader prewarm: material not found %s\n", path.c_str()); continue; }
		const int flags = split == std::string::npos ? 0 : parse_flags(trim(line.substr(split + 1)));
		if (gfx().get_device_type() == GraphicsDeviceType::OpenGl) {
			std::string shader_path = FileSys::get_game_path() + std::string("/") + material->get_name();
			shader_path = strip_extension(shader_path) + "_shader.glsl";
			opengl_prefetch_program_binary_async(shader_path, make_defines(flags));
		}
		entries.emplace_back(std::move(material), flags);
	}
	if (gfx().get_device_type() == GraphicsDeviceType::OpenGl) opengl_wait_for_program_binary_prefetch();
	for (auto& [material, flags] : entries) { get_mat_shader(nullptr, material.get(), flags); ++count; }
	sys_print(Debug, "shader prewarm: loaded %d material permutations\n", count);
	is_prewarming_manifest = false;
}

// ---------------------------------------------------------------------------
// MaterialManagerLocal – pre_render_update (material buffer uploads)
// ---------------------------------------------------------------------------

void MaterialManagerLocal::flush_dirty_material(MaterialInstance* mat) {
	ASSERT(gpuMatBufferPtr);
	if (!mat)
		return;
	// Idempotent: if this material isn't dirty, nothing to do.
	if (!dirty_list.find(mat))
		return;

	if (!mat->impl) {
		sys_print(Error, "flush_dirty_material: material '%s' in dirty list with null impl — skipping\n",
				  mat->get_name().c_str());
		dirty_list.remove(mat);
		return;
	}

	if (mat->impl->masterImpl.get())
		mat_shader_table.recompile_for_material(mat->impl->masterImpl.get());

	if (mat->impl->gpu_buffer_offset == MaterialImpl::INVALID_MAPPING)
		mat_offset_table->register_material(mat);

	std::array<std::byte, MATERIAL_SIZE> data_to_upload = {};

	auto mm = mat->get_master_material();
	ASSERT(mm);
	auto& params = mat->impl->params;
	ASSERT(mm->param_defs.size() == mat->impl->params.size());
	for (int i = 0; i < (int)params.size(); i++) {
		auto& param = params[i];
		auto& def = mm->param_defs[i];
		if (param.type == MatParamType::Texture2D)
			mat->impl->texture_bindings.at(def.offset) = param.tex.get();
		else {
			if (param.type == MatParamType::Float) {
				ASSERT(def.offset >= 0 && def.offset < MATERIAL_SIZE - 4);
				memcpy(data_to_upload.data() + def.offset, &param.scalar, sizeof(float));
			} else if (param.type == MatParamType::FloatVec) {
				ASSERT(def.offset >= 0 && def.offset < MATERIAL_SIZE - (int)sizeof(glm::vec4));
				memcpy(data_to_upload.data() + def.offset, &param.vector, sizeof(glm::vec4));
			} else if (param.type == MatParamType::Vector) {
				ASSERT(def.offset >= 0 && def.offset < MATERIAL_SIZE - (int)sizeof(Color32));
				memcpy(data_to_upload.data() + def.offset, &param.color32, sizeof(Color32));
			} else
				ASSERT(0);
		}
	}
	mat->impl->texture_id_hash = binding_hasher.get_texture_hash_id_for_material(mat->impl.get());

	const int offset = mat->impl->gpu_buffer_offset * sizeof(uint);
	gpuMatBufferPtr->sub_upload(data_to_upload.data(), data_to_upload.size(), offset);

	// Remove last so on-demand callers (MaterialImpl::get_texture_id_hash) and the
	// per-frame pre_render_update don't double-process the same material.
	dirty_list.remove(mat);
}

void MaterialManagerLocal::pre_render_update() {
	ASSERT(gpuMatBufferPtr);
	// Snapshot the dirty list — flush_dirty_material mutates it during iteration.
	std::vector<MaterialInstance*> snapshot;
	for (auto mat : dirty_list) {
		if (mat)
			snapshot.push_back(mat);
	}
	for (auto* mat : snapshot)
		flush_dirty_material(mat);
	dirty_list.clear_all();
}
