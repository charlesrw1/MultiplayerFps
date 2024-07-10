#pragma once


#include "Render/MaterialPublic.h"

#include <vector>
#include <unordered_set>

#include "Framework/Handle.h"
#include "Shader.h"
using program_handle_NEW = handle<Shader>;

class ProgramManagerNEW
{
public:
	program_handle_NEW create_raster(const char* frag, const char* vert, const std::string& defines = {});
	program_handle_NEW create_raster_geo(const char* frag, const char* vert, const char* geo = nullptr, const std::string& defines = {});
	program_handle_NEW create_compute(const char* compute, const std::string& defines = {});
	Shader get_obj(program_handle_NEW handle) {
		return programs[handle.id].shader_obj;
	}
	void recompile_all();

	struct program_def {
		std::string defines;
		const char* frag = nullptr;
		const char* vert = nullptr;
		const char* geo = nullptr;
		bool is_compute = false;
		bool compile_failed = false;
		Shader shader_obj;
	};
	std::vector<program_def> programs;
private:
	void recompile(program_def& def);
};

struct shader_key_NEW
{
	shader_key_NEW() {
		shader_type = 0;
		depth_only = 0;
		animated = 0;
		debug = 0;
		particle = 0;
		forward_lit = 0;
		postprocess = 0;
	}
	uint32_t shader_type : 26;
	uint32_t animated : 1;
	uint32_t depth_only : 1;
	uint32_t debug : 1;
	uint32_t particle : 1;
	uint32_t postprocess : 1;
	uint32_t forward_lit : 1;

	uint32_t as_uint32() const {
		return *((uint32_t*)this);
	}
};
static_assert(sizeof(shader_key_NEW) == 4, "shader key needs 4 bytes");

class MaterialShaderTableNEW
{
public:
	MaterialShaderTableNEW();
	struct material_shader_internal {
		shader_key_NEW key;
		program_handle_NEW handle = { -1 };
	};

	program_handle_NEW lookup(shader_key_NEW key);
	void insert(shader_key_NEW key, program_handle_NEW handle);

	std::vector<material_shader_internal> shader_hash_map;
};


class MaterialTypeLocal : public MaterialType
{
public:
	// shader details

	// get shader for runtime

};

class MaterialManagerLocal : public MaterialManagerPublic
{
public:
	// public interface
	void pre_render_update() override;
	const MaterialInstance* find_material_instance(const char* mat_inst_name) override;
	DynamicMatView* create_dynmaic_material(const MaterialInstance* material) override;
	void free_dynamic_material(DynamicMatView*& mat) override;
	const MaterialType* find_master_material(const char* master_name) override;

	MaterialParameterBuffer* find_parameter_buffer(const char* name) override;

	// local interface
	void add_to_changed_queue(DynamicMatView* mat);

private:
	MaterialShaderTableNEW shader_table;


	bufferhandle gpuMaterialBuffer = 0;
	uint32_t materialBufferSize = 0;

	// hardcoded values, these should be dynamic in the future
	static const uint32_t MATERIAL_SIZE = 64;	// 64 bytes
	static const uint32_t MAX_MAXTERIALS = MATERIAL_SIZE * 1024;

	// bitmap allocator for materials
	std::vector<uint64_t> materialBitmapAllocator;

	std::unordered_map<std::string, MaterialTypeLocal*> master_materials;
	std::unordered_map<std::string, MaterialInstance*> static_materials;
	std::unordered_set<DynamicMatView*> dynamic_materials;

	std::unordered_map<std::string, MaterialParameterBuffer*> parameter_buffers;

	std::vector<DynamicMatView*> changed_queue;

};