#ifndef TEXTURE_H
#define TEXTURE_H
#include <string>
#include <cstdint>
#include <vector>
#include <unordered_map>
#include "DrawTypedefs.h"
#include "glm/glm.hpp"

enum Texture_Format
{
	TEXFMT_RGBA8,
	TEXFMT_RGB8,
	TEXFMT_RG8,
	TEXFMT_R8,
	TEXFMT_RGBA16F,
	TEXFMT_RGB16F,
	TEXFMT_RGBA8_DXT5,
	TEXFMT_RGB8_DXT1,	
	TEXFMT_RGBA8_DXT1,
};

enum Texture_Type
{
	TEXTYPE_2D,
	TEXTYPE_3D,
	TEXTYPE_CUBEMAP,
};

struct Texture
{
	std::string name;
	int width = 0;
	int height = 0;
	int channels = 0;
	Texture_Type type;
	Texture_Format format;
	bool couldnt_load = false;
	bool no_filtering = false;
	bool has_mips = false;
	bool is_loaded_in_memory = false;
	bool is_float = false;

	texhandle gl_id = 0;

	bool is_resident = false;
	bindlesstexhandle bindless_handle = 0;
};

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
	WATER
};

class Material
{
public:
	static const int MAX_REFERENCES = 2;

	Material() {
		memset(images, 0, sizeof images);
		memset(references, 0, sizeof references);
	}
	uint32_t material_id = 0;
	std::string name;

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

	int physics = 0;	// physics of surface

	bool texture_are_loading_in_memory = false;
	
	bool is_translucent() const {
		return blend == blend_state::ADD || blend == blend_state::BLEND || type == material_type::WATER;
	}
	bool is_alphatested() const {
		return alpha_tested;
	}

	uint32_t gpu_material_mapping = 0;
};

class Game_Material_Manager
{
public:
	void init();

	void load_material_file_directory(const char* directory);
	void load_material_file(const char* path, bool overwrite);

	Material* find_for_name(const char* name);
	Material* create_temp_shader(const char* name);

	// owner: caller manages lifetime if true
	// search_img_directory: prefixes the default image directory to file
	Texture* find_texture(const char* file, bool search_img_directory=true, bool owner=false);
	Texture* create_texture_from_memory(const char* name, const uint8_t* data, int data_len, bool flipy);

	Material fallback;
	Texture error_grid;
	std::unordered_map<std::string, Material> materials;
	std::unordered_map<std::string, Texture> textures;
	void free_all();
private:
	uint32_t cur_mat_id = 1;
	Material* find_and_make_if_dne(const char* name);

	void ensure_data_is_loaded(Material* mat);

	Texture* create_but_dont_load(const char* filename);
	bool load_texture(const std::string& path, Texture* t);
};

extern Game_Material_Manager mats;

void FreeLoadedTextures();
Texture* FindOrLoadTexture(const char* filename);
Texture* CreateTextureFromImgFormat(uint8_t* data, int datalen, std::string name, bool flipy);
void FreeTexture(Texture* t);

#endif // !TEXTURE_H
