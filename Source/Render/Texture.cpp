#include "Texture.h"
#include <vector>

#include "glm/glm.hpp"


#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "glad/glad.h"
#include "Framework/Util.h"

#include "Framework/Files.h"
#include "Framework/DictParser.h"
#include "Assets/AssetRegistry.h"
#include "AssetCompile/Someutils.h"
#include "Assets/AssetDatabase.h"
#include "Framework/Config.h"
#undef APIENTRY
#define TINYEXR_IMPLEMENTATION

#include "tinyexr.h"
#undef APIENTRY

// TextureEditor.cpp
extern bool compile_texture_asset(const std::string& gamepath,IAssetLoadingInterface*,Color32& outColor);

#ifdef EDITOR_BUILD
//extern IEditorTool* g_texture_editor_tool;
class TextureAssetMetadata : public AssetMetadata
{
public:
	TextureAssetMetadata() {
		extensions.push_back("dds");
		extensions.push_back("hdr");
	}


	// Inherited via AssetMetadata
	virtual Color32 get_browser_color() const  override
	{
		return { 227, 39, 39 };
	}

	virtual std::string get_type_name() const  override
	{
		return "Texture";
	}

	virtual void fill_extra_assets(std::vector<std::string>& filepaths) const override
	{
		filepaths.push_back("_white");
		filepaths.push_back("_black");
		filepaths.push_back("_flat_normal");
	}


	virtual const ClassTypeInfo* get_asset_class_type() const { return &Texture::StaticType; }
};

REGISTER_ASSETMETADATA_MACRO(TextureAssetMetadata);
#endif

void texture_format_to_gl(Texture_Format infmt, GLenum* format, GLenum* internal_format, GLenum* type, bool* compressed)
{
	if (infmt == TEXFMT_RGBA16F || infmt == TEXFMT_RGB16F || infmt == TEXFMT_BC6)
		*type = GL_FLOAT;
	else
		*type = GL_UNSIGNED_BYTE;

	*compressed = false;

	switch (infmt)
	{
	case TEXFMT_RGBA8:
		*format = GL_RGBA;
		*internal_format = GL_RGBA8;
		break;
	case TEXFMT_RGBA8_DXT1:
		*format = GL_RGBA;
		*internal_format = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
		*compressed = true;
		break;
	case TEXFMT_RGBA8_DXT5:
		*format = GL_RGBA;
		*internal_format = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
		*compressed = true;
		break;
	case TEXFMT_RGBA16F:
		*format = GL_RGBA;
		*internal_format = GL_RGBA16F;
		break;
	case TEXFMT_RGB8:
		*format = GL_RGB;
		*internal_format = GL_RGB8;
		break;
	case TEXFMT_RGB8_DXT1:
		*format = GL_RGB;
		*internal_format = GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
		*compressed = true;
		break;
	case TEXFMT_RGB16F:
		*format = GL_RGB;
		*internal_format = GL_RGB16F;
		break;
	case TEXFMT_RG8:
		*format = GL_RG;
		*internal_format = GL_RG8;
		break;
	case TEXFMT_R8:
		*format = GL_RED;
		*internal_format = GL_R8;
		break;
	case TEXFMT_BC4:
		*format = GL_RED;
		*internal_format = GL_COMPRESSED_RED_RGTC1;
		*compressed = true;
		break;
	case TEXFMT_BC5:
		*format = GL_RG;
		*internal_format = GL_COMPRESSED_RG_RGTC2;
		*compressed = true;
		break;
	default:
		ASSERT(0);
	};


}

// surface description flags
const unsigned long DDSF_CAPS = 0x00000001l;
const unsigned long DDSF_HEIGHT = 0x00000002l;
const unsigned long DDSF_WIDTH = 0x00000004l;
const unsigned long DDSF_PITCH = 0x00000008l;
const unsigned long DDSF_PIXELFORMAT = 0x00001000l;
const unsigned long DDSF_MIPMAPCOUNT = 0x00020000l;
const unsigned long DDSF_LINEARSIZE = 0x00080000l;
const unsigned long DDSF_DEPTH = 0x00800000l;

// pixel format flags
const unsigned long DDSF_ALPHAPIXELS = 0x00000001l;
const unsigned long DDSF_FOURCC = 0x00000004l;
const unsigned long DDSF_RGB = 0x00000040l;
const unsigned long DDSF_RGBA = 0x00000041l;

// our extended flags
const unsigned long DDSF_ID_INDEXCOLOR = 0x10000000l;
const unsigned long DDSF_ID_MONOCHROME = 0x20000000l;

// dwCaps1 flags
const unsigned long DDSF_COMPLEX = 0x00000008l;
const unsigned long DDSF_TEXTURE = 0x00001000l;
const unsigned long DDSF_MIPMAP = 0x00400000l;


typedef struct {
	unsigned long Size;
	unsigned long Flags;
	unsigned long FourCC;
	unsigned long RGBBitCount;
	unsigned long RBitMask;
	unsigned long GBitMask;
	unsigned long BBitMask;
	unsigned long ABitMask;
} ddsFilePixelFormat_t;

typedef struct
{
	unsigned long Size;
	unsigned long Flags;
	unsigned long Height;
	unsigned long Width;
	unsigned long PitchOrLinearSize;
	unsigned long Depth;
	unsigned long MipMapCount;
	unsigned long Reserved1[11];
	ddsFilePixelFormat_t ddspf;
	unsigned long Caps1;
	unsigned long Caps2;
	unsigned long Reserved2[3];
} ddsFileHeader_t;


enum DXGI_FORMAT {
	DXGI_FORMAT_UNKNOWN = 0,
	DXGI_FORMAT_R32G32B32A32_TYPELESS = 1,
	DXGI_FORMAT_R32G32B32A32_FLOAT = 2,
	DXGI_FORMAT_R32G32B32A32_UINT = 3,
	DXGI_FORMAT_R32G32B32A32_SINT = 4,
	DXGI_FORMAT_R32G32B32_TYPELESS = 5,
	DXGI_FORMAT_R32G32B32_FLOAT = 6,
	DXGI_FORMAT_R32G32B32_UINT = 7,
	DXGI_FORMAT_R32G32B32_SINT = 8,
	DXGI_FORMAT_R16G16B16A16_TYPELESS = 9,
	DXGI_FORMAT_R16G16B16A16_FLOAT = 10,
	DXGI_FORMAT_R16G16B16A16_UNORM = 11,
	DXGI_FORMAT_R16G16B16A16_UINT = 12,
	DXGI_FORMAT_R16G16B16A16_SNORM = 13,
	DXGI_FORMAT_R16G16B16A16_SINT = 14,
	DXGI_FORMAT_R32G32_TYPELESS = 15,
	DXGI_FORMAT_R32G32_FLOAT = 16,
	DXGI_FORMAT_R32G32_UINT = 17,
	DXGI_FORMAT_R32G32_SINT = 18,
	DXGI_FORMAT_R32G8X24_TYPELESS = 19,
	DXGI_FORMAT_D32_FLOAT_S8X24_UINT = 20,
	DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS = 21,
	DXGI_FORMAT_X32_TYPELESS_G8X24_UINT = 22,
	DXGI_FORMAT_R10G10B10A2_TYPELESS = 23,
	DXGI_FORMAT_R10G10B10A2_UNORM = 24,
	DXGI_FORMAT_R10G10B10A2_UINT = 25,
	DXGI_FORMAT_R11G11B10_FLOAT = 26,
	DXGI_FORMAT_R8G8B8A8_TYPELESS = 27,
	DXGI_FORMAT_R8G8B8A8_UNORM = 28,
	DXGI_FORMAT_R8G8B8A8_UNORM_SRGB = 29,
	DXGI_FORMAT_R8G8B8A8_UINT = 30,
	DXGI_FORMAT_R8G8B8A8_SNORM = 31,
	DXGI_FORMAT_R8G8B8A8_SINT = 32,
	DXGI_FORMAT_R16G16_TYPELESS = 33,
	DXGI_FORMAT_R16G16_FLOAT = 34,
	DXGI_FORMAT_R16G16_UNORM = 35,
	DXGI_FORMAT_R16G16_UINT = 36,
	DXGI_FORMAT_R16G16_SNORM = 37,
	DXGI_FORMAT_R16G16_SINT = 38,
	DXGI_FORMAT_R32_TYPELESS = 39,
	DXGI_FORMAT_D32_FLOAT = 40,
	DXGI_FORMAT_R32_FLOAT = 41,
	DXGI_FORMAT_R32_UINT = 42,
	DXGI_FORMAT_R32_SINT = 43,
	DXGI_FORMAT_R24G8_TYPELESS = 44,
	DXGI_FORMAT_D24_UNORM_S8_UINT = 45,
	DXGI_FORMAT_R24_UNORM_X8_TYPELESS = 46,
	DXGI_FORMAT_X24_TYPELESS_G8_UINT = 47,
	DXGI_FORMAT_R8G8_TYPELESS = 48,
	DXGI_FORMAT_R8G8_UNORM = 49,
	DXGI_FORMAT_R8G8_UINT = 50,
	DXGI_FORMAT_R8G8_SNORM = 51,
	DXGI_FORMAT_R8G8_SINT = 52,
	DXGI_FORMAT_R16_TYPELESS = 53,
	DXGI_FORMAT_R16_FLOAT = 54,
	DXGI_FORMAT_D16_UNORM = 55,
	DXGI_FORMAT_R16_UNORM = 56,
	DXGI_FORMAT_R16_UINT = 57,
	DXGI_FORMAT_R16_SNORM = 58,
	DXGI_FORMAT_R16_SINT = 59,
	DXGI_FORMAT_R8_TYPELESS = 60,
	DXGI_FORMAT_R8_UNORM = 61,
	DXGI_FORMAT_R8_UINT = 62,
	DXGI_FORMAT_R8_SNORM = 63,
	DXGI_FORMAT_R8_SINT = 64,
	DXGI_FORMAT_A8_UNORM = 65,
	DXGI_FORMAT_R1_UNORM = 66,
	DXGI_FORMAT_R9G9B9E5_SHAREDEXP = 67,
	DXGI_FORMAT_R8G8_B8G8_UNORM = 68,
	DXGI_FORMAT_G8R8_G8B8_UNORM = 69,
	DXGI_FORMAT_BC1_TYPELESS = 70,
	DXGI_FORMAT_BC1_UNORM = 71,
	DXGI_FORMAT_BC1_UNORM_SRGB = 72,
	DXGI_FORMAT_BC2_TYPELESS = 73,
	DXGI_FORMAT_BC2_UNORM = 74,
	DXGI_FORMAT_BC2_UNORM_SRGB = 75,
	DXGI_FORMAT_BC3_TYPELESS = 76,
	DXGI_FORMAT_BC3_UNORM = 77,
	DXGI_FORMAT_BC3_UNORM_SRGB = 78,
	DXGI_FORMAT_BC4_TYPELESS = 79,
	DXGI_FORMAT_BC4_UNORM = 80,
	DXGI_FORMAT_BC4_SNORM = 81,
	DXGI_FORMAT_BC5_TYPELESS = 82,
	DXGI_FORMAT_BC5_UNORM = 83,
	DXGI_FORMAT_BC5_SNORM = 84,
	DXGI_FORMAT_B5G6R5_UNORM = 85,
	DXGI_FORMAT_B5G5R5A1_UNORM = 86,
	DXGI_FORMAT_B8G8R8A8_UNORM = 87,
	DXGI_FORMAT_B8G8R8X8_UNORM = 88,
	DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM = 89,
	DXGI_FORMAT_B8G8R8A8_TYPELESS = 90,
	DXGI_FORMAT_B8G8R8A8_UNORM_SRGB = 91,
	DXGI_FORMAT_B8G8R8X8_TYPELESS = 92,
	DXGI_FORMAT_B8G8R8X8_UNORM_SRGB = 93,
	DXGI_FORMAT_BC6H_TYPELESS = 94,
	DXGI_FORMAT_BC6H_UF16 = 95,
	DXGI_FORMAT_BC6H_SF16 = 96,
	DXGI_FORMAT_BC7_TYPELESS = 97,
	DXGI_FORMAT_BC7_UNORM = 98,
	DXGI_FORMAT_BC7_UNORM_SRGB = 99,
	DXGI_FORMAT_AYUV = 100,
	DXGI_FORMAT_Y410 = 101,
	DXGI_FORMAT_Y416 = 102,
	DXGI_FORMAT_NV12 = 103,
	DXGI_FORMAT_P010 = 104,
	DXGI_FORMAT_P016 = 105,
	DXGI_FORMAT_420_OPAQUE = 106,
	DXGI_FORMAT_YUY2 = 107,
	DXGI_FORMAT_Y210 = 108,
	DXGI_FORMAT_Y216 = 109,
	DXGI_FORMAT_NV11 = 110,
	DXGI_FORMAT_AI44 = 111,
	DXGI_FORMAT_IA44 = 112,
	DXGI_FORMAT_P8 = 113,
	DXGI_FORMAT_A8P8 = 114,
	DXGI_FORMAT_B4G4R4A4_UNORM = 115,
	DXGI_FORMAT_P208 = 130,
	DXGI_FORMAT_V208 = 131,
	DXGI_FORMAT_V408 = 132,
	DXGI_FORMAT_SAMPLER_FEEDBACK_MIN_MIP_OPAQUE,
	DXGI_FORMAT_SAMPLER_FEEDBACK_MIP_REGION_USED_OPAQUE,
	DXGI_FORMAT_FORCE_UINT = 0xffffffff
};
struct DDS_HEADER_DXT10
{
	DXGI_FORMAT     dxgiFormat;
	uint32_t        resourceDimension;
	uint32_t        miscFlag; // see D3D11_RESOURCE_MISC_FLAG
	uint32_t        arraySize;
	uint32_t        miscFlags2; // see DDS_MISC_FLAGS2
};

static void make_from_data(Texture* output, int x, int y, void* data, Texture_Format informat);
static bool load_dds_file(Texture* output, uint8_t* buffer, int len)
{
	if (len < 4 + sizeof(ddsFileHeader_t)) return false;
	if (buffer[0] != 'D' || buffer[1] != 'D' || buffer[2] != 'S' || buffer[3] != ' ') return false;
	// BIG ENDIAN ISSUE:
	ddsFileHeader_t* header = (ddsFileHeader_t*)(buffer + 4);
	
	uint32_t dxt1_fourcc = 'D' | ('X' << 8) | ('T' << 16) | ('1' << 24);	// aka bc1
	uint32_t dxt5_fourcc = 'D' | ('X' << 8) | ('T' << 16) | ('5' << 24);	// aka bc3
	uint32_t bc4u_fourcc = 'B' | ('C' << 8) | ('4' << 16) | ('U' << 24);
	uint32_t bc5u_fourcc = 'B' | ('C' << 8) | ('5' << 16) | ('U' << 24);

	Texture_Format input_format = TEXFMT_RGB8;
	int input_width = header->Width;
	int input_height = header->Height;

	if (header->ddspf.Flags & DDSF_FOURCC) {
		if (header->ddspf.FourCC == dxt1_fourcc) {
			if (header->ddspf.Flags & DDSF_ALPHAPIXELS)
				input_format = TEXFMT_RGBA8_DXT1;
			else
				input_format = TEXFMT_RGB8_DXT1;
		}
		else if (header->ddspf.FourCC == dxt5_fourcc) {
			input_format = TEXFMT_RGBA8_DXT5;
		}
		else if (header->ddspf.FourCC == bc4u_fourcc) {
			input_format = TEXFMT_BC4;
		}
		else if (header->ddspf.FourCC == bc5u_fourcc) {
			input_format = TEXFMT_BC5;
		}
		else
			ASSERT(0 && "bad fourcc");
	}
	else {
		if (header->ddspf.RGBBitCount == 24)
			input_format = TEXFMT_RGB8;
		else if (header->ddspf.RGBBitCount == 32)
			input_format = TEXFMT_RGBA8;
		else if (header->ddspf.RGBBitCount == 16)
			input_format = TEXFMT_RG8;
		else if (header->ddspf.RGBBitCount == 8)
			input_format = TEXFMT_R8;
		else
			ASSERT(0 && "bad bit count in dds");
	}

	int numMipmaps = 1;
	if (header->Flags & DDSF_MIPMAPCOUNT) {
		numMipmaps = header->MipMapCount;
	}


	GLenum type;
	GLenum internal_format;
	GLenum format;
	bool compressed;
	texture_format_to_gl(input_format, &format, &internal_format, &type, &compressed);

	glCreateTextures(GL_TEXTURE_2D, 1, &output->gl_id);
	glTextureStorage2D(output->gl_id, numMipmaps, internal_format, input_width, input_height);

	int ux = input_width;
	int uy = input_height;
	uint8_t* data_ptr = (buffer+4+sizeof(ddsFileHeader_t));

	const int compressed_stride = (input_format == TEXFMT_RGBA8_DXT1 || input_format == TEXFMT_RGB8_DXT1) ? 8 : 16;
	for (int i = 0; i < numMipmaps; i++) {
		int size = 0;
		if (compressed) {
			size = ((ux + 3) / 4) * ((uy + 3) / 4) *
				compressed_stride;
		}
		else {
			size = ux*uy* int(header->ddspf.RGBBitCount / 8);
		}
		
		if (compressed)
			glCompressedTextureSubImage2D(output->gl_id,i, 0, 0, ux, uy, internal_format, size, data_ptr);
		else
			glTextureSubImage2D(output->gl_id, i, 0, 0, ux, uy, format, type, data_ptr);

		data_ptr += size;
		ux /= 2;
		uy /= 2;
		if (ux < 1)ux = 1;
		if (uy < 1)uy = 1;
	}

	glTextureParameteri(output->gl_id, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTextureParameteri(output->gl_id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTextureParameteri(output->gl_id, GL_TEXTURE_MAX_ANISOTROPY, 16.f);

	output->width = input_width;
	output->height = input_height;
	output->format = input_format;

	return true;
	//return make_from_data(output, input_width, input_height, (buffer + 4 + sizeof(ddsFileHeader_t)), input_format);
}

static void make_from_data(Texture* output, int x, int y, void* data, Texture_Format informat, bool nearest_filtered = false)
{
	glCreateTextures(GL_TEXTURE_2D, 1, &output->gl_id);

	int x_real = x;
	int y_real = y;

	GLenum type{};
	GLenum internal_format{};
	GLenum format{};
	bool compressed{};
	texture_format_to_gl(informat, &format, &internal_format, &type, &compressed);

	int size = 0;
	if (compressed) {
		size = ((x + 3) / 4) * ((y + 3) / 4) *
			(informat == TEXFMT_RGBA8_DXT5 ? 16 : 8);
	}

	glTextureStorage2D(output->gl_id, Texture::get_mip_map_count(x, y), internal_format, x, y);
	assert(x == x_real && y == y_real);
	if (compressed)
		glCompressedTextureSubImage2D(output->gl_id, 0, 0, 0, x, y, internal_format, size, data);
	else
		glTextureSubImage2D(output->gl_id, 0, 0, 0, x, y, format, type, data);

	if (!nearest_filtered) {

		glTextureParameteri(output->gl_id, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTextureParameteri(output->gl_id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTextureParameteri(output->gl_id, GL_TEXTURE_MAX_ANISOTROPY, 16.f);
		glGenerateTextureMipmap(output->gl_id);
	}
	else {
		glTextureParameteri(output->gl_id, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTextureParameteri(output->gl_id, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}

	glCheckError();

	output->width = x;
	output->height = y;
	output->format = informat;
}

using std::string;
using std::vector;


enum class TextureMipLevel
{
	Highest,
	Medium,
	Low,
};

Texture_Format to_format(int n, bool isfloat)
{
	if (isfloat) {
		if (n == 4) return TEXFMT_RGBA16F;
		else if (n == 3) return TEXFMT_RGB16F;
	}
	else {
		if (n >= 1 && n <= 4) return Texture_Format(TEXFMT_R8 - (n - 1));
	}
	return TEXFMT_RGB8;
}

#include "GameEnginePublic.h"
void Texture::move_construct(IAsset* _src)
{
	assert(!eng->get_is_in_overlapped_period());
	Texture* src = (Texture*)_src;
	uninstall();
	width = src->width;
	height = src->height;
	gl_id = src->gl_id;
	channels = src->channels;
	type = src->type;
	format = src->format;
	loaddata = std::move(src->loaddata);
	src->gl_id = 0;	// dont uninstall it since were just stealing it
#ifdef EDITOR_BUILD
	simplifiedColor = src->simplifiedColor;
	hasSimplifiedColor = src->hasSimplifiedColor;
#endif
}
void Texture::post_load() {
	if (did_load_fail())
		return;

	auto user = loaddata.get();

	int& x = user->x;
	int& y = user->y;
	auto& data = user->data;
	auto& filedata = user->filedata;
	auto& is_float = user->is_float;

	if (user->isDDSFile)
		load_dds_file(this, filedata.data(), filedata.size());
	else
		make_from_data(this, x, y, data, to_format(channels, is_float), user->wantsNearestFiltering);

	if (data)
		stbi_image_free(data);

	type = Texture_Type::TEXTYPE_2D;

	loaddata.reset();
}

extern ConfigVar developer_mode;

bool Texture::load_asset(IAssetLoadingInterface* loading) {
	const auto& path = get_name();
	assert(path != "_white" && path != "_black");	// quick assert here, default textures should be initialized before anything else
#ifdef EDITOR_BUILD
	if (developer_mode.get_bool()) {
		// this will check if a compile is needed
		bool good = compile_texture_asset(path,loading,this->simplifiedColor);
		if (good)
			this->hasSimplifiedColor = true;
	}
#endif

	auto file = FileSys::open_read_game(path);

	if (!file) {
		return false;
	}

	loaddata = std::make_unique<LoadData>();
	auto user = loaddata.get();
	int& x = user->x;
	int& y = user->y;
	auto& data = user->data;
	auto& filedata = user->filedata;
	auto& is_float = user->is_float;

	if (path.find("/_nearest")!=std::string::npos)
		user->wantsNearestFiltering = true;	// hack moment


	user->filedata.resize(file->size());
	file->read(user->filedata.data(), user->filedata.size());

	if (path.find(".dds") != std::string::npos) {
		user->isDDSFile = true;
		return true;
	}
	else if (path.find(".hdr") != std::string::npos) {
		data = stbi_loadf_from_memory(filedata.data(), filedata.size(), &x, &y, &channels, 0);
		filedata = {};
		is_float = true;
	}
	else if (path.find(".exr") != std::string::npos) {
		float* out_data = nullptr;
		const char* err = nullptr;
		int res = LoadEXRFromMemory(&out_data, &x, &y, filedata.data(), filedata.size(), &err);
		if (res != TINYEXR_SUCCESS) {
			sys_print(Error, "Texture::load_asset: couldnt load .exr: %s\n", err ? err : "<unknown>");
			return false;
		}
		is_float = true;
		channels = 4;
		filedata = {};
		data = out_data;
	}
	else {
		data = stbi_load_from_memory(filedata.data(), filedata.size(), &x, &y, &channels, 0);
		filedata = {};
		is_float = false;
	}
	file->close();

	if (data == nullptr) {
		loaddata.reset();
		return false;
	}

	return true;
}

void Texture::uninstall()
{
	if (gl_id != 0) {
		glDeleteTextures(1, &gl_id);
		width = height = 0;
		gl_id = 0;
	}
}
Texture* Texture::install_system(const std::string& path)
{
	Texture* t = new Texture;
	g_assets.install_system_asset(t, path);
	return t;
}
Texture::Texture() {}
Texture::~Texture() {
	//assert(is_this_globally_referenced()||gl_id == 0);
}
#include "Assets/AssetDatabase.h"
Texture* Texture::load(const std::string& path)
{
	return g_assets.find_sync<Texture>(path).get();
}


void benchmark_run()
{
#if 0
	const char* dds_file = "Data\\Textures\\Cttexturenavy.dds";
	const char* png_file = "Data\\Textures\\Cttexturenavy.png";

	const int RUNS = 500;

	Texture tex;
	std::vector<char> data;
	double start = GetTime();
	{
	for (int i = 0; i < RUNS; i++) {
		int x, y, channels;
		void*  data = stbi_load(png_file, &x, &y, &channels, 0);
		make_from_data(&tex, x, y, data, to_format(channels,false));
		stbi_image_free(data);
	}
	}
	double end = GetTime();
	printf("PNG: %f\n", float(end - start));
	start = GetTime();
		for (int i = 0; i < RUNS; i++) {
			std::ifstream infile(dds_file, std::ios::binary);
			infile.seekg(0, std::ios::end);
			size_t len = infile.tellg();
			data.resize(len);
			infile.seekg(0);
			infile.read(data.data(), len);
			load_dds_file(&tex, (uint8_t*)data.data(), len);
		}
	end = GetTime();
	printf("DDS: %f\n", float(end - start));
#endif

}

#if 0
Texture* TextureMan::create_texture_from_memory(const char* name, const uint8_t* data, int data_len, bool flipy)
{
	auto find = textures.find(name);
	if (find != textures.end())
		return find->second;
	Texture* t = new Texture;
	textures[name] = t;
	t->path = name;

	int width, height, channels;
	stbi_set_flip_vertically_on_load(flipy);
	uint8_t* imgdat = stbi_load_from_memory(data, data_len, &width, &height, &channels, 0);
	if (!imgdat) {
		printf("Couldn't load from memory: %s", name);
		return nullptr;
	}

	bool good = make_from_data(t, width, height, (void*)imgdat, to_format(channels, false));
	stbi_image_free((void*)imgdat);

	return t;
}
#endif
