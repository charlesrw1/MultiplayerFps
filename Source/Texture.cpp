#include "Texture.h"
#include <vector>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "glad/glad.h"
#include "Util.h"
#include "Key_Value_File.h"

#include <Windows.h>

static const char* const texture_folder_path = "Data\\Textures\\";

static std::vector<Texture*> textures;
Game_Material_Manager mats;

#define ENSURE(num) if(line.size() < num) { sys_print("bad material %s", line); continue;}
void Game_Material_Manager::load_material_file(const char* path, bool overwrite)
{
	Key_Value_File file;
	bool good = file.open(path);
	if (!good) {
		sys_print("Couldn't open material file %s\n", path);
		return;
	}

	for (auto& mat : file.entries)
	{
		// first check if it exists
		Game_Shader* gs = find_for_name(mat.name.c_str());
		if (gs && !overwrite) continue;
		else if (gs && overwrite) {
			*gs = Game_Shader();
			gs->name = std::move(mat.name);
		}
		if (!gs) {
			gs = new Game_Shader;
			gs->name = std::move(mat.name);
			shaders.push_back(gs);
		}

		for (auto& line : mat.tokenized_lines)
		{
			ASSERT(!line.empty());
			string& key = line.at(0);
			if (key == "image1") {
				ENSURE(2);
				gs->images[Game_Shader::BASE1] = FindOrLoadTexture(line.at(1).c_str());
			}
			else if (key == "image1aux") {
				ENSURE(2);
				gs->images[Game_Shader::AUX1] = FindOrLoadTexture(line.at(1).c_str());
			}
			else if (key == "image2") {
				ENSURE(2);
				gs->images[Game_Shader::BASE2] = FindOrLoadTexture(line.at(1).c_str());
			}
			else if (key == "image2aux") {
				ENSURE(2);
				gs->images[Game_Shader::AUX2] = FindOrLoadTexture(line.at(1).c_str());
			}
			else if (key == "image3") {
				ENSURE(2);
				gs->images[Game_Shader::SPECIAL] = FindOrLoadTexture(line.at(1).c_str());
			}
			else if (key == "physics") {
				// TODO
			}
			else if (key == "shader") {
				// TODO
			}
			else if (key == "alpha") {
				ENSURE(2);
				string& t = line.at(1);
				if (t == "add") gs->alpha_type = Game_Shader::A_ADD;
				else if (t == "blend") gs->alpha_type = Game_Shader::A_BLEND;
				else if (t == "test") gs->alpha_type = Game_Shader::A_TEST;
			}
			else if (key == "showbackface") {
				gs->backface = true;
			}
		}

		sys_print("loaded material %s\n", gs->name.c_str());
	}
}
#undef ENSURE;


bool file_system_get_files(const char* directory, std::vector<string>& out)
{
	WIN32_FIND_DATAA findData;
	HANDLE hFind = FindFirstFileA(directory, &findData);
	if (hFind == INVALID_HANDLE_VALUE)
		return false;
	while (FindNextFileA(hFind, &findData) != 0) {
		if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY || findData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN)
			continue;
		out.push_back(std::string(findData.cFileName));
	}

	FindClose(hFind);

	return true;
}

void Game_Material_Manager::load_material_file_directory(const char* directory)
{
	std::vector<std::string> files;
	std::string path = std::string(directory) + '*';
	bool good = file_system_get_files(path.c_str(), files);
	if (!good) {
		sys_print("Couldn't open material directory %s\n", directory);
		return;
	}
	for (auto& file : files) {
		std::string path = directory + file;
		load_material_file(path.c_str(), true);
	}

	sys_print("Loaded material directory %s\n", directory);
}

Game_Shader* Game_Material_Manager::create_temp_shader(const char* name)
{
	ASSERT(find_for_name(name)==nullptr);
	Game_Shader* gs = new Game_Shader;
	shaders.push_back(gs);
	gs->name = name;
	return gs;
}
Game_Shader* Game_Material_Manager::find_for_name(const char* name)
{
	for (auto s : shaders)
		if (s->name == name)
			return s;
	return nullptr;
}

void Game_Material_Manager::init()
{
	sys_print("Material Manager initialized\n");
}


static Texture* MakeFromData(int x, int y, int channels, uint8_t* data)
{
	Texture* output = new Texture;

	glGenTextures(1, &output->gl_id);
	glBindTexture(GL_TEXTURE_2D, output->gl_id);

	GLenum internal_format;
	GLenum format;

	if (channels == 4) {
		internal_format = GL_RGBA;
		format = GL_RGBA;
	}
	else if (channels == 3) {
		internal_format = GL_RGB;
		format = GL_RGB;
	}
	else {
		printf("Unsupported channel count\n");
		delete output;
		return nullptr;
	}
	glTexImage2D(GL_TEXTURE_2D, 0, internal_format, x, y, 0, format, GL_UNSIGNED_BYTE, data);

	glGenerateMipmap(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, 0);

	output->width = x;
	output->height = y;
	output->channels = channels;

	return output;
}

using std::string;
using std::vector;


static Texture* AddTexture(std::string& path)
{
	int width, height, channels;

	stbi_set_flip_vertically_on_load(false);
	uint8_t* data = stbi_load(path.c_str(), &width, &height, &channels, 0);
	if (!data) {
		printf("Couldn't find image: %s\n", path.c_str());
		return nullptr;
	}

	Texture* output = MakeFromData(width, height, channels, data);
	output->name = std::move(path);

	stbi_image_free(data);
	textures.push_back(output);
	return output;
}

static void FreeTexture(Texture* t)
{
	printf("Freeing texture: %s\n", t->name.c_str());
	glDeleteTextures(1, &t->gl_id);
}

void FreeLoadedTextures()
{
	for (int i = 0; i < textures.size(); i++) {
		FreeTexture(textures[i]);
		delete textures[i];
	}
	textures.clear();
}

Texture* FindOrLoadTexture(const char* filename)
{
	std::string path;
	path.reserve(256);
	path += texture_folder_path;
	path += filename;

	for (int i = 0; i < textures.size(); i++) {
		if (textures[i]->name == path) {
			return textures[i];
		}
	}
	return AddTexture(path);
}

Texture* CreateTextureFromImgFormat(uint8_t* inpdata, int datalen, std::string name)
{
	for (int i = 0; i < textures.size(); i++) {
		if (textures[i]->name == name) {
			return textures[i];
		}
	}
	
	int width, height, channels;
	uint8_t* data = stbi_load_from_memory(inpdata, datalen, &width, &height, &channels, 0);
	if (!data) {
		printf("Couldn't load from memory: %s", name.c_str());
		return nullptr;
	}

	Texture* t = MakeFromData(width, height, channels, data);
	stbi_image_free(data);
	t->name = std::move(name);
	textures.push_back(t);
	return t;
}
