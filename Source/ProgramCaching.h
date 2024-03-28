#pragma once

#include <cstdint>
#include <vector>
#include "DrawTypedefs.h"
#include "Shader.h"

class Program_Manager
{
public:
	program_handle create_raster(const char* frag, const char* vert, const std::string& defines = {});
	program_handle create_raster_geo(const char* frag, const char* vert, const char* geo = nullptr, const std::string& defines = {});
	program_handle create_compute(const char* compute, const std::string& defines = {});
	Shader get_obj(program_handle handle) {
		return programs[handle].shader_obj;
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

struct shader_key
{
	shader_key() {
		shader_type = 0;
		depth_only = 0;
		alpha_tested = 0;
		normal_mapped = 0;
		animated = 0;
		vertex_colors = 0;
	}
	uint32_t shader_type : 27;
	uint32_t animated : 1;
	uint32_t alpha_tested : 1;
	uint32_t normal_mapped : 1;
	uint32_t vertex_colors : 1;
	uint32_t depth_only : 1;

	uint32_t as_uint32() const {
		return *((uint32_t*)this);
	}
};
static_assert(sizeof(shader_key) == 4, "shader key needs 4 bytes");

class Material_Shader_Table
{
public:
	Material_Shader_Table();
	struct material_shader_internal {
		shader_key key;
		program_handle handle = -1;
	};

	program_handle lookup(shader_key key);
	void insert(shader_key key, program_handle handle);

	std::vector<material_shader_internal> shader_hash_map;
};