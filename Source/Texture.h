#ifndef TEXTURE_H
#define TEXTURE_H
#include <string>
#include <cstdint>
#include <vector>
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

	bool is_float = false;
	uint32_t gl_id = 0;
};

enum Material_Alpha_Mode
{
	MALPHA_OPAQUE,
	MALPHA_ADDITIVE,
	MALPHA_BLEND,
	MALPHA_MASKED,
	MALPHA_SOFTMASKED,
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
	enum { BASE1, BASE2, NORMAL1, NORMAL2, AUX1, AUX2, SPECIAL, MAX_IMAGES };
	// base = rgb diffuse
	//		a alpha/mask
	// aux = r specular exponent
	//		g specular mask
	//		b emissive mask
	// normal = rgb normal map
	Texture* images[MAX_IMAGES];	

	// Phong material model
	glm::vec4 diffuse_tint = glm::vec4(1.f);	// with alpha
	glm::vec3 spec_tint = glm::vec3(0.1);
	float spec_exponent = 16.0;
	float spec_strength = 1.0;	// multiplies spec_tint or diffuse
	float emissive_boost = 1.0;
	float specular_mask_exponent = 1.0;	// mask = pow(mask, exponent), for tweaking
	bool backface = false;
	Material_Alpha_Mode alpha;

	glm::vec2 uv_scroll = glm::vec2(0.f);



	enum { A_NONE, A_ADD, A_BLEND, A_TEST };
	int alpha_type = A_NONE;
	bool emmisive = false;	// dont recieve lighting
	enum {S_DEFAULT, S_2WAYBLEND, S_WINDSWAY, S_WATER };
	int shader_type = S_DEFAULT;
	float uscroll = 0.0;
	float vscroll = 0.0;
	bool fresnel_transparency = false;

	uint64_t cached_render_key = -1;

	int physics = 0;	// physics of surface
	// Pbr material model

	bool is_translucent() const {
		return alpha_type == A_ADD || alpha_type == A_BLEND;
	}
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
	Texture* create_texture_from_memory(const char* name, const uint8_t* data, int data_len);

	Game_Shader fallback;
	std::vector<Game_Shader*> shaders;
	std::vector<Texture*> textures;

	Texture* load_texture(const std::string& path);
	void free_all();
};

extern Game_Material_Manager mats;

void FreeLoadedTextures();
Texture* FindOrLoadTexture(const char* filename);
Texture* CreateTextureFromImgFormat(uint8_t* data, int datalen, std::string name);
void FreeTexture(Texture* t);

#endif // !TEXTURE_H
