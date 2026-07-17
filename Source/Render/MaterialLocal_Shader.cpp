#include "MaterialLocal.h"
#include "Framework/Files.h"
#include "AssetCompile/Someutils.h"
#include "DrawLocal.h"
#include "Render/Model.h"
#include "glad/glad.h"

// ---------------------------------------------------------------------------
// MaterialShaderTable
// ---------------------------------------------------------------------------

MaterialShaderTable::MaterialShaderTable() {}

void MaterialShaderTable::recompile_for_material(MasterMaterialImpl* mat) {
	ASSERT(mat);
	uint32_t id = mat->material_id;
	for (auto& pair : shader_key_to_program_handle) {
		uint32_t this_id = pair.first & ((1ul << 27ul) - 1ul);
		if (this_id == id) {
			draw.get_prog_man().recompile(pair.second);
		}
	}
	// do one default shader compile so we can approximately tell if the shader is invalid
	program_handle default_h = matman.get_mat_shader(nullptr, mat->self, 0);
	mat->is_compilied_shader_valid = !draw.get_prog_man().did_shader_fail(default_h);
	if (!mat->is_compilied_shader_valid)
		sys_print(Error, "recompile_for_material: material is invalid %s\n", mat->self->get_name().c_str());
}

program_handle MaterialShaderTable::lookup(shader_key key) {
	ASSERT(key.as_uint32() != 0 || true); // key can be 0 for default
	uint32_t key32 = key.as_uint32();
	auto find = shader_key_to_program_handle.find(key32);
	return find == shader_key_to_program_handle.end() ? -1 : find->second;
}

void MaterialShaderTable::insert(shader_key key, program_handle handle) {
	ASSERT(handle != -1);
	shader_key_to_program_handle.insert({key.as_uint32(), handle});
}

// ---------------------------------------------------------------------------
// MaterialManagerLocal – shader compilation
// ---------------------------------------------------------------------------

extern ConfigVar material_print_debug;

program_handle MaterialManagerLocal::compile_mat_shader(const MaterialInstance* mat, shader_key key) {
	ASSERT(mat);

	std::string name = FileSys::get_game_path() + ("/" + mat->get_name());
	name = strip_extension(name);
	name += "_shader.glsl";

	std::string params;
	if (key.has_flag(MSF_ANIMATED))
		params += "ANIMATED,";
	if (key.has_flag(MSF_DITHER))
		params += "DITHER,";
	if (key.has_flag(MSF_EDITOR_ID))
		params += "EDITOR_ID,";
	if (key.has_flag(MSF_DEPTH_ONLY))
		params += "DEPTH_ONLY,";
	if (key.has_flag(MSF_DEBUG))
		params += "DEBUG_SHADER,";
	if (key.has_flag(MSF_LIGHTMAPPED))
		params += "LIGHTMAPPED,";
	if (key.has_flag(MSF_IS_FORCED_FORWARD))
		params += "THUMBNAIL_FORWARD,";
	if (key.has_flag(MSF_NO_TAA))
		params += "NO_TAA,";
	if (key.has_flag(MSF_MATERIAL_IN_INSTANCE))
		params += "MAT_WITH_INST,";
	if (key.has_flag(MSF_COMPACT_INST))
		params += "COMPACT_INST,";
	if (!params.empty())
		params.pop_back();

	if (material_print_debug.get_bool())
		sys_print(Debug, "compiling shader: %s %s\n", mat->get_name().c_str(), params.c_str());

	const bool is_tesselation = mat->get_master_material()->usage == MaterialUsage::Terrain;
	program_handle handle = draw.get_prog_man().create_single_file(name, is_tesselation, params);
	ASSERT(handle != -1);

	mat_shader_table.insert(key, handle);
	return handle;
}

program_handle MaterialManagerLocal::get_mat_shader(const Model* mod, const MaterialInstance* mat, int flags) {
	ASSERT(mat);
	const MasterMaterialImpl* mm = mat->get_master_material();

	shader_key key;
	key.material_id = mm->material_id;
	key.msf_flags = flags;

	program_handle handle = mat_shader_table.lookup(key);
	if (handle != -1)
		return handle;
	return compile_mat_shader(mm->self, key);
}
