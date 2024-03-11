#ifndef TEXTURE_H
#define TEXTURE_H
#include <string>
#include <cstdint>
#include <vector>
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
	bool no_filtering = false;
	bool has_mips = false;
	bool is_loaded_in_memory = false;
	bool is_float = false;
	texhandle gl_id = 0;

	bool is_resident = false;
	bindlesstexhandle bindless_handle = 0;
};

enum Material_Alpha_Mode
{
	MALPHA_OPAQUE,
	MALPHA_ADDITIVE,
	MALPHA_BLEND,
	MALPHA_MASKED,
};
enum Material_Shader_Category
{
	MSHADER_STANDARD,
	MSHADER_WIND,
	MSHADER_MULTIBLEND,
	MSHADER_WATER,
};

class Game_Shader
{
public:
	Game_Shader() {
		memset(images, 0, sizeof images);
	}
	std::string name;
	enum { DIFFUSE, NORMAL, ROUGHNESS, METAL, AO, SPECIAL, MAX_IMAGES };
	Texture* images[MAX_IMAGES];	
	enum { MAX_REFERENCES = 2};
	Game_Shader* references[MAX_REFERENCES];

	// pbr parameters
	glm::vec4 diffuse_tint = glm::vec4(1.f);
	float roughness_mult = 1.f;
	float metalness_mult = 1.f;
	glm::vec2 roughness_remap_range = glm::vec2(0.f, 1.f);
	bool backface = false;

	enum { A_NONE, A_ADD, A_BLEND, A_TEST };
	int alpha_type = A_NONE;
	bool emmisive = false;	// dont recieve lighting
	enum {S_DEFAULT, S_2WAYBLEND, S_WINDSWAY, S_WATER };
	int shader_type = S_DEFAULT;

	int physics = 0;	// physics of surface

	bool texture_are_loading_in_memory = false;
	bool is_translucent() const {
		return alpha_type == A_ADD || alpha_type == A_BLEND;
	}

	int current_gpu_mapping = -1;
	int shader_hash = -1;
	int params_hash = -1;
};

class Game_Material_Manager
{
public:
	void init();
	void load_material_file_directory(const char* directory);
	void load_material_file(const char* path, bool overwrite);
	Game_Shader* find_for_name(const char* name);
	Game_Shader* create_temp_shader(const char* name);

	// @owner, caller manages lifetime if true
	// @search_img_directory, prefixes the default image directory to file
	Texture* find_texture(const char* file, bool search_img_directory=true, bool owner=false);
	Texture* create_texture_from_memory(const char* name, const uint8_t* data, int data_len, bool flipy);


	Game_Shader fallback;
	std::vector<Game_Shader*> shaders;
	std::vector<Texture*> textures;
	void free_all();
private:
	Texture* create_but_dont_load(const char* filename);
	bool load_texture(const std::string& path, Texture* t);
};

extern Game_Material_Manager mats;

void FreeLoadedTextures();
Texture* FindOrLoadTexture(const char* filename);
Texture* CreateTextureFromImgFormat(uint8_t* data, int datalen, std::string name, bool flipy);
void FreeTexture(Texture* t);

#endif // !TEXTURE_H
