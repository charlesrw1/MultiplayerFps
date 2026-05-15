#include "MaterialLocal.h"
#include "Framework/Files.h"
#include "DrawLocal.h"
#include "Render/Texture.h"
#include "Render/Model.h"
#include "glad/glad.h"
#include "IGraphsDevice.h"
#include "Assets/AssetDatabase.h"

#include <array>

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
		return IGraphicsDevice::inst->create_buffer(args);
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
}

// ---------------------------------------------------------------------------
// MaterialManagerLocal – pre_render_update (material buffer uploads)
// ---------------------------------------------------------------------------

void MaterialManagerLocal::pre_render_update() {
	ASSERT(gpuMatBufferPtr);
	for (auto mat : dirty_list) {
		if (!mat)
			continue;

		if (mat->impl->masterImpl.get())
			mat_shader_table.recompile_for_material(mat->impl->masterImpl.get());

		auto check_buffer_offset = [&]() {
			const int gpu_buffer_offset = mat->impl->gpu_buffer_offset;
			if (gpu_buffer_offset == MaterialImpl::INVALID_MAPPING) {
				mat_offset_table->register_material(mat);
			}
		};
		check_buffer_offset();

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
	}

	dirty_list.clear_all();
}
