#ifndef TEXTURE_H
#define TEXTURE_H
#include <string>
#include <cstdint>
#include <vector>

struct Texture
{
	std::string name;
	int width = 0;
	int height = 0;
	int channels = 0;
	uint32_t gl_id = 0;
	uint32_t gl_flags = 0;
};

class Game_Shader
{
public:
	Game_Shader() {
		memset(images, 0, sizeof images);
	}
	std::string name;
	enum { BASE1, BASE2, AUX1, AUX2, SPECIAL, MAX_IMAGES };
	Texture* images[MAX_IMAGES];
	enum { A_NONE, A_ADD, A_BLEND, A_TEST };
	int alpha_type = A_NONE;
	bool backface = false;
	int physics = 0;	// physics of surface
};

class Game_Material_Manager
{
public:
	void init();
	void load_material_file_directory(const char* directory);
	void load_material_file(const char* path, bool overwrite);
	Game_Shader* find_for_name(const char* name);
	Game_Shader* create_temp_shader(const char* name);

	Game_Shader fallback;
	std::vector<Game_Shader*> shaders;
};

extern Game_Material_Manager mats;


void FreeLoadedTextures();
Texture* FindOrLoadTexture(const char* filename);
Texture* CreateTextureFromImgFormat(uint8_t* data, int datalen, std::string name);

#endif // !TEXTURE_H
