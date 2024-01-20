#include "Texture.h"
#include <vector>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "glad/glad.h"

static const char* const texture_folder_path = "Data\\Textures\\";

static std::vector<Texture*> textures;

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


// handles case of using root file system and package file system
class Archive
{
	struct File
	{
		string name;
		int offset, len;
	};
	vector<File> files;
};
class File_System
{
	


	vector<string> networked_strings;	// for model and sound names
};

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
