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

#include "IGraphsDevice.h"


// TextureEditor.cpp
extern bool compile_texture_asset(const std::string& gamepath,IAssetLoadingInterface*,Color32& outColor);
#ifdef EDITOR_BUILD
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include"stb_image_write.h"
int write_png_wrapper(const char* filename, int w, int h, int comp, const void* data, int stride_in_bytes)
{
	return stbi_write_png(filename, w, h, comp, data, stride_in_bytes);
}
int write_hdr_wrapper(const char* filename, int w, int h, int comp, const float* data)
{
	return stbi_write_hdr(filename, w, h, comp, data);
}


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
#define DDS_RESOURCE_MISC_TEXTURECUBE 0x4
struct DDS_HEADER_DXT10
{
	DXGI_FORMAT     dxgiFormat;
	uint32_t        resourceDimension;
	uint32_t        miscFlag; // see D3D11_RESOURCE_MISC_FLAG
	uint32_t        arraySize;
	uint32_t        miscFlags2; // see DDS_MISC_FLAGS2
};


static IGraphicsTexture* make_from_data(Texture* output, int x, int y, void* data, GraphicsTextureFormat informat);
static bool load_dds_file(Texture* output, IGraphicsTexture*& out_ptr, uint8_t* buffer, int len)
{
	if (len < 4 + sizeof(ddsFileHeader_t)) return false;
	if (buffer[0] != 'D' || buffer[1] != 'D' || buffer[2] != 'S' || buffer[3] != ' ') return false;
	// BIG ENDIAN ISSUE:
	ddsFileHeader_t* header = (ddsFileHeader_t*)(buffer + 4);
	
	const uint32_t dxt1_fourcc = 'D' | ('X' << 8) | ('T' << 16) | ('1' << 24);	// aka bc1
	const uint32_t dxt5_fourcc = 'D' | ('X' << 8) | ('T' << 16) | ('5' << 24);	// aka bc3
	const uint32_t bc4u_fourcc = 'B' | ('C' << 8) | ('4' << 16) | ('U' << 24);
	const uint32_t bc5u_fourcc = 'B' | ('C' << 8) | ('5' << 16) | ('U' << 24);

	GraphicsTextureFormat fmt{};
	int input_width = header->Width;
	int input_height = header->Height;


	DDS_HEADER_DXT10* dx10 = nullptr;
	const uint32_t dx10FourCC = 'D' | ('X' << 8) | ('1' << 16) | ('0' << 24);
	using gtf = GraphicsTextureFormat;

	if (header->ddspf.Flags & DDSF_FOURCC) {
		if (header->ddspf.FourCC == dxt1_fourcc) {
			if (header->ddspf.Flags & DDSF_ALPHAPIXELS)
				fmt = gtf::bc1;
			else
				fmt = gtf::bc1;
		}
		else if (header->ddspf.FourCC == dxt5_fourcc) {
			fmt = gtf::bc3;
		}
		else if (header->ddspf.FourCC == bc4u_fourcc) {
			fmt = gtf::bc4;
		}
		else if (header->ddspf.FourCC == bc5u_fourcc) {
			fmt = gtf::bc5;
		}
		else if((header->ddspf.Flags & DDSF_FOURCC) &&
			header->ddspf.FourCC == dx10FourCC) {
			dx10 = (DDS_HEADER_DXT10*)(buffer + 4 + sizeof(ddsFileHeader_t));

			if (dx10->dxgiFormat == DXGI_FORMAT_BC1_UNORM_SRGB)
				fmt = gtf::bc1_srgb;
			else if (dx10->dxgiFormat == DXGI_FORMAT_BC1_UNORM)
				fmt = gtf::bc1;
			else
				ASSERT(0 && "UNHANDLED DDS FORMAT");

			}
		else
			ASSERT(0 && "bad fourcc");
	}
	else {
		if (header->ddspf.RGBBitCount == 24)
			fmt = gtf::rgb8;
		else if (header->ddspf.RGBBitCount == 32)
			fmt = gtf::rgba8;
		else if (header->ddspf.RGBBitCount == 16)
			fmt = gtf::rg8;
		else if (header->ddspf.RGBBitCount == 8)
			fmt = gtf::r8;
		else
			ASSERT(0 && "bad bit count in dds");
	}

	int numMipmaps = 1;
	if (header->Flags & DDSF_MIPMAPCOUNT) {
		numMipmaps = header->MipMapCount;
	}



	auto create_gpu_texture = [&]() {
		CreateTextureArgs args;
		args.width = input_width;
		args.height = input_height;
		args.num_mip_maps = numMipmaps;
		args.format = fmt;
		args.sampler_type = GraphicsSamplerType::AnisotropyDefault;
		return IGraphicsDevice::inst->create_texture(args);
	};
	out_ptr = create_gpu_texture();
	const bool compressed = out_ptr->is_compressed();

	//glCreateTextures(GL_TEXTURE_2D, 1, &output->gl_id);
	//glTextureStorage2D(output->gl_id, numMipmaps, internal_format, input_width, input_height);

	int ux = input_width;
	int uy = input_height;
	uint8_t* data_ptr = (buffer+4+sizeof(ddsFileHeader_t));
	if (dx10)
		data_ptr += sizeof(DDS_HEADER_DXT10);

	int compressed_stride = 0;
	if (compressed)
		compressed_stride = out_ptr->get_compressed_stride();
	//const int compressed_stride = (input_format == TEXFMT_RGBA8_DXT1 || input_format == TEXFMT_RGB8_DXT1) ? 8 : 16;
	for (int i = 0; i < numMipmaps; i++) {
		int size = 0;
		if (compressed) {
			size = ((ux + 3) / 4) * ((uy + 3) / 4) *
				compressed_stride;
		}
		else {
			size = ux*uy* int(header->ddspf.RGBBitCount / 8);
		}
		
		//if (compressed)
		//	glCompressedTextureSubImage2D(output->gl_id,i, 0, 0, ux, uy, internal_format, size, data_ptr);
		//else
		//	glTextureSubImage2D(output->gl_id, i, 0, 0, ux, uy, format, type, data_ptr);

		out_ptr->sub_image_upload(i, 0, 0, ux, uy, size, data_ptr);

		data_ptr += size;
		ux /= 2;
		uy /= 2;
		if (ux < 1)ux = 1;
		if (uy < 1)uy = 1;
	}

	//glTextureParameteri(output->gl_id, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	//glTextureParameteri(output->gl_id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	//glTextureParameteri(output->gl_id, GL_TEXTURE_MAX_ANISOTROPY, 16.f);

	return true;
	//return make_from_data(output, input_width, input_height, (buffer + 4 + sizeof(ddsFileHeader_t)), input_format);
}
bool dxgi_texture_format_to_gl(DXGI_FORMAT infmt, GraphicsTextureFormat* outfmt)
{
	switch (infmt)
	{
	case DXGI_FORMAT_BC6H_UF16:
		*outfmt = GraphicsTextureFormat::bc6;
		break;

	case DXGI_FORMAT_R11G11B10_FLOAT:
		*outfmt = GraphicsTextureFormat::r11f_g11f_b10f;
		break;

	case DXGI_FORMAT_R16G16_FLOAT:
		*outfmt = GraphicsTextureFormat::rg16f;
		break;

	default:
		return false;
	}
	return true;
}

// use for bc6 cubemap array, bc6 irrad/depth textures
bool load_dds_file_specialized_format(IGraphicsTexture*& out_ptr, uint8_t* buffer, int len)
{
	if (len < 4 + sizeof(ddsFileHeader_t)) return false;
	if (buffer[0] != 'D' || buffer[1] != 'D' || buffer[2] != 'S' || buffer[3] != ' ') return false;
	// BIG ENDIAN ISSUE:
	ddsFileHeader_t* header = (ddsFileHeader_t*)(buffer + 4);


	GraphicsTextureFormat input_format = GraphicsTextureFormat::rg16f;
	int input_width = header->Width;
	int input_height = header->Height;

	int cubemap_array_size = 0;
	uint32_t dx10FourCC = 'D' | ('X' << 8) | ('1' << 16) | ('0' << 24);
	DDS_HEADER_DXT10 dx10 = {};
	if ((header->ddspf.Flags & DDSF_FOURCC) &&
		header->ddspf.FourCC == dx10FourCC)
	{
		dx10 = *(DDS_HEADER_DXT10*)(buffer + 4 + sizeof(ddsFileHeader_t));
	}
	else {
		return false;
	}

	if (dx10.miscFlag & DDS_RESOURCE_MISC_TEXTURECUBE) {
		cubemap_array_size = dx10.arraySize;
	}

	int numMipmaps = 1;
	if (header->Flags & DDSF_MIPMAPCOUNT) {
		numMipmaps = header->MipMapCount;
	}


	GraphicsTextureFormat type{};
	
	const bool res = dxgi_texture_format_to_gl(dx10.dxgiFormat, &type);
	if (!res)
		return false;

	auto create_gpu_texture = [&]() {
		CreateTextureArgs args;
		args.width = input_width;
		args.height = input_height;
		args.num_mip_maps = numMipmaps;
		args.format = type;
		args.float_input_is_16f = true;
		if (cubemap_array_size > 0) {
			args.type = GraphicsTextureType::tCubemapArray;
			args.depth_3d = cubemap_array_size*6;
			args.sampler_type = GraphicsSamplerType::CubemapDefault;
		}
		else {
			args.sampler_type = GraphicsSamplerType::LinearDefault;
		}
		return IGraphicsDevice::inst->create_texture(args);
	};
	out_ptr = create_gpu_texture();

	//glCreateTextures(GL_TEXTURE_2D, 1, &output->gl_id);
	//glTextureStorage2D(output->gl_id, numMipmaps, internal_format, input_width, input_height);

	uint8_t* data_ptr = (buffer + 4 + sizeof(ddsFileHeader_t) + sizeof(DDS_HEADER_DXT10));

	const bool compressed = out_ptr->is_compressed();

	const int compressed_stride = 16;//for bc6 fixme
	const int bytes_per_pixel = 4;	// fixme, hardcoded for rg16f and r11g11b10
	int num_layers = 1;
	if (cubemap_array_size > 0)
		num_layers = cubemap_array_size * 6;
	for (int layer = 0; layer < num_layers; layer++) {
		int ux = input_width;
		int uy = input_height;
		for (int i = 0; i < numMipmaps; i++) {
			int size = 0;
			if (compressed) {
				size = ((ux + 3) / 4) * ((uy + 3) / 4) *
					compressed_stride;
			}
			else {
				size = ux * uy * bytes_per_pixel;
			}

			//if (compressed)
			//	glCompressedTextureSubImage2D(output->gl_id,i, 0, 0, ux, uy, internal_format, size, data_ptr);
			//else
			//	glTextureSubImage2D(output->gl_id, i, 0, 0, ux, uy, format, type, data_ptr);

			if (num_layers == 1)
				out_ptr->sub_image_upload(i, 0, 0, ux, uy, size, data_ptr);
			else
				out_ptr->sub_image_upload_3d(layer, i, 0, 0, ux, uy, size, data_ptr);

			data_ptr += size;
			ux /= 2;
			uy /= 2;
			if (ux < 1)ux = 1;
			if (uy < 1)uy = 1;
		}
	}

	//glTextureParameteri(output->gl_id, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	//glTextureParameteri(output->gl_id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	//glTextureParameteri(output->gl_id, GL_TEXTURE_MAX_ANISOTROPY, 16.f);

	return true;
	//return make_from_data(output, input_width, input_height, (buffer + 4 + sizeof(ddsFileHeader_t)), input_format);
}


static IGraphicsTexture* make_from_data(Texture* output, int x, int y, void* data, GraphicsTextureFormat informat, bool nearest_filtered = false)
{
	//glCreateTextures(GL_TEXTURE_2D, 1, &output->gl_id);

	int x_real = x;
	int y_real = y;
	const int num_mip_maps = Texture::get_mip_map_count(x, y);
	auto create_gpu_texture = [&]() {
		CreateTextureArgs args;
		args.width = x;
		args.height = y;
		args.num_mip_maps = num_mip_maps;
		args.format = informat;
		args.type = GraphicsTextureType::t2D;
		if (nearest_filtered)
			args.sampler_type = GraphicsSamplerType::NearestDefault;
		else
			args.sampler_type = GraphicsSamplerType::AnisotropyDefault;
		return IGraphicsDevice::inst->create_texture(args);
	};
	IGraphicsTexture* ptr = create_gpu_texture();
	ASSERT(!ptr->is_compressed());	// compressed takes dds path

	const int size = 0;
	

	//glTextureStorage2D(output->gl_id, num_mip_maps, internal_format, x, y);
	assert(x == x_real && y == y_real);
	//if (compressed)
	//	glCompressedTextureSubImage2D(output->gl_id, 0, 0, 0, x, y, internal_format, size, data);
	//else
	//	glTextureSubImage2D(output->gl_id, 0, 0, 0, x, y, format, type, data);

	ptr->sub_image_upload(0, 0, 0, x, y, size, data);
	if (!nearest_filtered) {
		// fixme
		glGenerateTextureMipmap(ptr->get_internal_handle());
	}
	//if (!nearest_filtered) {
	//
	//	glTextureParameteri(output->gl_id, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	//	glTextureParameteri(output->gl_id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	//	glTextureParameteri(output->gl_id, GL_TEXTURE_MAX_ANISOTROPY, 16.f);
	//	glGenerateTextureMipmap(output->gl_id);
	//}
	//else {
	//	glTextureParameteri(output->gl_id, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	//	glTextureParameteri(output->gl_id, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	//}

	glCheckError();

	//output->width = x;
	//output->height = y;
	//output->format = informat;

	return ptr;
}

using std::string;
using std::vector;


enum class TextureMipLevel
{
	Highest,
	Medium,
	Low,
};

GraphicsTextureFormat to_format(int n, bool isfloat)
{
	using gtf = GraphicsTextureFormat;
	if (isfloat) {
		if (n == 4) return gtf::rgba16f;
		else if (n == 3) return gtf::rgb16f;
	}
	else {
		if (n == 1) return gtf::r8;
		if (n == 2) return gtf::rg8;
		if (n == 3)return gtf::rgb8;
		if (n == 4)return gtf::rgba8;
	}
	ASSERT(0 && "unknown to format");
	return gtf::rgb8;
}

#include "GameEnginePublic.h"
void Texture::move_construct(IAsset* _src)
{
	assert(!eng->get_is_in_overlapped_period());
	Texture* src = (Texture*)_src;
	uninstall();
	assert(!gpu_ptr);
	gpu_ptr = src->gpu_ptr;
	//format = src->format;
	loaddata = std::move(src->loaddata);
	src->gpu_ptr = nullptr;	// dont uninstall it since were just stealing it
#ifdef EDITOR_BUILD
	simplifiedColor = src->simplifiedColor;
	hasSimplifiedColor = src->hasSimplifiedColor;
#endif
}
glm::ivec2 Texture::get_size() const {
	if (gpu_ptr) {
		return gpu_ptr->get_size();
	}
	return {};
}
texhandle Texture::get_internal_render_handle() const
{
	if (gpu_ptr)
		return gpu_ptr->get_internal_handle();
	return 0;
}
ConfigVar disable_texture_loads("disable_texture_loads", "0", CVAR_BOOL | CVAR_DEV, "");

void Texture::post_load() {
	if (did_load_fail())
		return;

	if (disable_texture_loads.get_bool()) {
		const uint8_t missing_tex[] = { 230,0,255,255,  0,0,0,255,
										0,0,0,255,	230,0,255,255 };
		auto create_defeault = [](IGraphicsTexture*& handle, const uint8_t* data) -> void {
			CreateTextureArgs args;
			args.num_mip_maps = 1;
			args.width = 2;
			args.height = 2;
			args.format = GraphicsTextureFormat::rgba8;
			args.sampler_type = GraphicsSamplerType::LinearDefault;


			handle = IGraphicsDevice::inst->create_texture(args);
			handle->sub_image_upload(0, 0, 0, 2, 2, sizeof(uint8_t) * 4 * 4, data);
		};
		create_defeault(gpu_ptr, missing_tex);

	//	format = Texture_Format::TEXFMT_RGBA8;

		return;
	}

	auto user = loaddata.get();

	int& x = user->x;
	int& y = user->y;
	auto& data = user->data;
	auto& filedata = user->filedata;

	if (user->isDDSFile)
		load_dds_file(this,gpu_ptr, filedata.data(), filedata.size());
	else
		gpu_ptr=make_from_data(this, x, y, data, to_format(user->channels, user->is_float), user->wantsNearestFiltering);

	if (data)
		stbi_image_free(data);


	loaddata.reset();
}

extern ConfigVar developer_mode;

bool Texture::load_asset(IAssetLoadingInterface* loading) {
	const auto& path = get_name();
	assert(path != "_white" && path != "_black");	// quick assert here, default textures should be initialized before anything else
	if (disable_texture_loads.get_bool())
		return true;

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
	auto& channels = user->channels;

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
	safe_release(gpu_ptr);
}
void Texture::update_specs_ptr(IGraphicsTexture* ptr) {
	this->gpu_ptr = ptr;
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
#include <array>



void texture_loading_benchmark()
{
	ASSERT(0);
	std::vector<std::byte> filedata;
	filedata.reserve(10'000'000);
	double start = GetTime();
	double last = 0.0;
	auto print_time = [&](const char* msg) {
		double now = GetTime();
		last = now - start;
		//printf("%s: %f\n", msg, float(now - start));
		start = now;
	};

	const char* ssd_path = "work_prop/WorkLight02_Base_Color.dds";
	const char* hdd_path = "E:/Users/charl/Downloads/WorkLight02_Base_Color.dds";
	auto run_benchmark = [&](int max_mips) {
		printf("mips: %d\n", max_mips);
		IFilePtr ptr = FileSys::open_read(hdd_path,FileSys::FULL_SYSTEM);
		print_time("	open file");

		//ptr->read(filedata.data(), filedata.size());
		print_time("	read file");
		Texture dummy;
	//	load_dds_file_file(&dummy, dummy.gpu_ptr, ptr.get(), max_mips);
		print_time("	load to opengl");
		return dummy.gpu_ptr;
	};

	for (int i = 12; i >= 1; i-=1) {
		run_benchmark(i);
	}

	auto try_copy = [&]() {
		IGraphicsTexture* src = run_benchmark(5);
		const glm::ivec2 size = src->get_size();
		const int mips = src->get_num_mips();
		texhandle new_t{};
		print_time("copying_texture...");
		glCreateTextures(GL_TEXTURE_2D, 1, &new_t);
		glTextureStorage2D(new_t, mips, GL_COMPRESSED_RGB_S3TC_DXT1_EXT, size.x, size.y);
		print_time("	glTextureStorage2D...");

		for (int mip = 0; mip < mips; ++mip) {
			int mipWidth = std::max(1, size.x >> mip);
			int mipHeight = std::max(1, size.y >> mip);

			glCopyImageSubData(
				src->get_internal_handle(), GL_TEXTURE_2D, mip, 0, 0, 0,
				new_t, GL_TEXTURE_2D, mip, 0, 0, 0,
				mipWidth, mipHeight, 1
			);
		}
		print_time("	copying done.");
	};
	try_copy();
	printf("%f\n", float(last));
	__debugbreak();
}
#include "MaterialLocal.h"

struct StreamTextureData {
	ddsFileHeader_t cached_header{};
	int num_mips_allocated = 0;
	int num_mips_loaded = 0;
	float mip_lod_factor = 0.0;
	
	int wanted_mip_level = -1;
};

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
