#ifndef TEXTURE_H
#define TEXTURE_H
#include <string>
#include <cstdint>
#include <unordered_map>
#include "DrawTypedefs.h"
#include "glm/glm.hpp"
#include "IAsset.h"

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

CLASS_H(Texture, IAsset)
public:
	~Texture() {}

	int width = 0;
	int height = 0;
	int channels = 0;
	Texture_Type type;
	Texture_Format format;
	bool no_filtering = false;
	bool has_mips = false;
	bool is_float = false;

	texhandle gl_id = 0;

	bool is_resident = false;
	bindlesstexhandle bindless_handle = 0;

	friend class TextureMan;
};

class TextureMan : public IAssetLoader
{
public:
	IAsset* load_asset(const std::string& path) override {
		return find_texture(path.c_str());
	}

	// owner: caller manages lifetime if true
	// search_img_directory: prefixes the default image directory to file
	Texture* find_texture(const char* file, bool search_img_directory = true, bool owner = false);
	Texture* create_texture_from_memory(const char* name, const uint8_t* data, int data_len, bool flipy);
private:

	Texture* create_unloaded_ptr(const char* filename);
	bool load_texture(const std::string& path, Texture* t);

	std::unordered_map<std::string, Texture*> textures;
	friend class MaterialMan;
};

extern TextureMan g_imgs;


#endif // !TEXTURE_H
