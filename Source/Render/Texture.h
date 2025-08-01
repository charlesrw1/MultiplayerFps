#ifndef TEXTURE_H
#define TEXTURE_H
#include <string>
#include <cstdint>
#include "DrawTypedefs.h"
#include "glm/glm.hpp"
#include "Assets/IAsset.h"
#include "Framework/Reflection2.h"
#include <memory>
#include <vector>

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


class IGraphicsTexture;
class Texture : public IAsset {
public:
	CLASS_BODY(Texture);
	Texture();
	~Texture();

	REF static Texture* load(const std::string& path);

	void uninstall() override;
	void post_load() override;
	bool load_asset(IAssetLoadingInterface* loading) override;
	void move_construct(IAsset* src) override;

	glm::ivec2 get_size() const;
	texhandle get_internal_render_handle() const;


	Texture_Type type = Texture_Type::TEXTYPE_2D;
	Texture_Format format{};
	bool no_filtering = false;
	bool has_mips = false;
	bool is_float = false;


#ifdef EDITOR_BUILD
	bool hasSimplifiedColor = false;
	Color32 simplifiedColor = COLOR_WHITE;
#endif

	// used for system textures
	void update_specs_ptr(IGraphicsTexture* ptr);

	static Texture* install_system(const std::string& path);
	static int get_mip_map_count(int width, int height){
		return glm::floor(glm::log2((double)glm::max(width, height))) + 1;
	}
	IGraphicsTexture* gpu_ptr = nullptr;
private:
	struct LoadData {
		std::vector<uint8_t> filedata;
		bool isDDSFile = false;
		int x{}, y{}, channels{};
		bool is_float = false;
		void* data = nullptr;
		bool wantsNearestFiltering = false;
	};
	std::unique_ptr<LoadData> loaddata;
};



#endif // !TEXTURE_H
