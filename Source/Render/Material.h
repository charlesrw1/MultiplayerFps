#pragma once
#include <unordered_map>
#include "glm/glm.hpp"
#include "IAsset.h"

enum class material_texture : uint8_t
{
	DIFFUSE,
	NORMAL,
	ROUGHNESS,
	METAL,
	AO,
	SPECIAL,

	COUNT
};

enum class blend_state : uint8_t
{
	OPAQUE,
	BLEND,
	ADD
};

enum class material_type : uint8_t
{
	DEFAULT,
	TWOWAYBLEND,
	WINDSWAY,
	WATER,
	UNLIT,
	OUTLINE_HULL,
	SELECTION_PULSE,

	CUSTOM,	// custom vertex+fragment shader
};

enum class billboard_setting : uint8_t
{
	NONE,
	FACE_CAMERA,	// standard billboard
	ROTATE_AXIS,	// rotate around the provided axis in model transform
	SCREENSPACE,	// faces camera and keeps constant screen space size
};

class Texture;
class Material : public IAsset
{
public:
	static const int MAX_REFERENCES = 2;
	static const uint32_t INVALID_MAPPING = uint32_t(-1);

	Material() {
		memset(images, 0, sizeof images);
		memset(references, 0, sizeof references);
	}
	uint32_t material_id = 0;
	Texture* images[(int)material_texture::COUNT];

	Texture*& get_image(material_texture texture) {
		return images[(int)texture];
	}

	Material* references[MAX_REFERENCES];
	// pbr parameters
	glm::vec4 diffuse_tint = glm::vec4(1.f);
	float roughness_mult = 1.f;
	float metalness_mult = 0.f;
	glm::vec2 roughness_remap_range = glm::vec2(0.f, 1.f);
	bool emmisive = false;	// dont recieve lighting
	material_type type = material_type::DEFAULT;
	blend_state blend = blend_state::OPAQUE;
	bool alpha_tested = false;
	bool backface = false;
	billboard_setting billboard = billboard_setting::NONE;

	bool is_translucent() const {
		return blend == blend_state::ADD || blend == blend_state::BLEND || type == material_type::WATER;
	}
	bool is_alphatested() const {
		return alpha_tested;
	}

	// where this material maps to in the gpu buffer
	uint32_t gpu_material_mapping = INVALID_MAPPING;

	friend class MaterialMan;
};

namespace gpu {
	struct Material_Data;
}
class MaterialMan
{
public:
	void init();

	Material* find_for_name(const char* name);
	Material* create_temp_shader(const char* name);

	Material* unlit;
	Material* outline_hull;
	Material* fallback = nullptr;

	void reload_all();
	bool update_gpu_material_buffer(gpu::Material_Data* buffer, size_t buffer_size, size_t* num_materials);
private:

	Material* find_existing(const std::string& name);

	Material* load_material_file(const char* path, bool overwrite);
	std::unordered_map<std::string, Material*> materials;

	uint32_t cur_mat_id = 1;
	bool referenced_materials_dirty = false;

	Material* find_and_make_if_dne(const char* name);

	void ensure_data_is_loaded(Material* mat);
};

extern MaterialMan mats;