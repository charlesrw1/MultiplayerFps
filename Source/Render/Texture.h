#ifndef TEXTURE_H
#define TEXTURE_H
#include <string>
#include <cstdint>
#include "DrawTypedefs.h"
#include "glm/glm.hpp"
#include "Assets/IAsset.h"

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

	TEXFMT_BC4,	// grey scale encoding
	TEXFMT_BC5,	// normal map encoding
	TEXFMT_BC6,	// float rgb data
};

enum Texture_Type
{
	TEXTYPE_2D,
	TEXTYPE_2D_ARRAY,
	TEXTYPE_3D,
	TEXTYPE_CUBEMAP,
};

inline int get_mip_map_count(int width, int height)
{
	return floor(glm::log2((double)glm::max(width, height))) + 1;
}

CLASS_H(Texture, IAsset)
public:
	~Texture() {}

	void uninstall() override;
	void sweep_references() const override {}
	void post_load(ClassBase* userStruct) override;
	bool load_asset(ClassBase*& userStruct) override;
	void move_construct(IAsset* src) override;


	int width = 0;
	int height = 0;
	int channels = 0;
	Texture_Type type = Texture_Type::TEXTYPE_2D;
	Texture_Format format{};
	bool no_filtering = false;
	bool has_mips = false;
	bool is_float = false;

	texhandle gl_id = 0;

	bool is_resident = false;
	bindlesstexhandle bindless_handle = 0;

	// used for system textures
	void update_specs(texhandle handle, int w, int h, int channels, Texture_Format fmt) {
		this->gl_id = handle;
		this->width = w;
		this->height = h;
		this->channels = channels;
		this->format = fmt;
	}

	static Texture* install_system(const std::string& path);
};



#endif // !TEXTURE_H
