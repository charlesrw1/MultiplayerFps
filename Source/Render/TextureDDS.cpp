// TextureDDS.cpp — DDS file format parsing and GPU upload for DDS/BC textures.
// Handles standard DDS (BC1/BC3/BC4/BC5/RGB/RGBA) and DX10-extended formats
// (BC6, R11G11B10F, RG16F, cubemap arrays). Called from Texture::post_load.

#include "Texture.h"
#include "IGraphsDevice.h"
#include "Framework/Util.h"
#include <cstdint>

// ---------------------------------------------------------------------------
// DDS surface / pixel-format flag constants
// ---------------------------------------------------------------------------

// surface description flags
const unsigned long DDSF_CAPS        = 0x00000001l;
const unsigned long DDSF_HEIGHT      = 0x00000002l;
const unsigned long DDSF_WIDTH       = 0x00000004l;
const unsigned long DDSF_PITCH       = 0x00000008l;
const unsigned long DDSF_PIXELFORMAT = 0x00001000l;
const unsigned long DDSF_MIPMAPCOUNT = 0x00020000l;
const unsigned long DDSF_LINEARSIZE  = 0x00080000l;
const unsigned long DDSF_DEPTH       = 0x00800000l;

// pixel format flags
const unsigned long DDSF_ALPHAPIXELS = 0x00000001l;
const unsigned long DDSF_FOURCC     = 0x00000004l;
const unsigned long DDSF_RGB        = 0x00000040l;
const unsigned long DDSF_RGBA       = 0x00000041l;

// dwCaps1 flags
const unsigned long DDSF_COMPLEX = 0x00000008l;
const unsigned long DDSF_TEXTURE = 0x00001000l;
const unsigned long DDSF_MIPMAP  = 0x00400000l;

// ---------------------------------------------------------------------------
// DDS file structures
// ---------------------------------------------------------------------------

typedef struct
{
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

enum DXGI_FORMAT
{
	DXGI_FORMAT_UNKNOWN                     = 0,
	DXGI_FORMAT_R32G32B32A32_TYPELESS       = 1,
	DXGI_FORMAT_R32G32B32A32_FLOAT          = 2,
	DXGI_FORMAT_R32G32B32A32_UINT           = 3,
	DXGI_FORMAT_R32G32B32A32_SINT           = 4,
	DXGI_FORMAT_R32G32B32_TYPELESS          = 5,
	DXGI_FORMAT_R32G32B32_FLOAT             = 6,
	DXGI_FORMAT_R32G32B32_UINT              = 7,
	DXGI_FORMAT_R32G32B32_SINT              = 8,
	DXGI_FORMAT_R16G16B16A16_TYPELESS       = 9,
	DXGI_FORMAT_R16G16B16A16_FLOAT          = 10,
	DXGI_FORMAT_R16G16B16A16_UNORM          = 11,
	DXGI_FORMAT_R16G16B16A16_UINT           = 12,
	DXGI_FORMAT_R16G16B16A16_SNORM          = 13,
	DXGI_FORMAT_R16G16B16A16_SINT           = 14,
	DXGI_FORMAT_R32G32_TYPELESS             = 15,
	DXGI_FORMAT_R32G32_FLOAT                = 16,
	DXGI_FORMAT_R32G32_UINT                 = 17,
	DXGI_FORMAT_R32G32_SINT                 = 18,
	DXGI_FORMAT_R32G8X24_TYPELESS           = 19,
	DXGI_FORMAT_D32_FLOAT_S8X24_UINT        = 20,
	DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS    = 21,
	DXGI_FORMAT_X32_TYPELESS_G8X24_UINT     = 22,
	DXGI_FORMAT_R10G10B10A2_TYPELESS        = 23,
	DXGI_FORMAT_R10G10B10A2_UNORM           = 24,
	DXGI_FORMAT_R10G10B10A2_UINT            = 25,
	DXGI_FORMAT_R11G11B10_FLOAT             = 26,
	DXGI_FORMAT_R8G8B8A8_TYPELESS           = 27,
	DXGI_FORMAT_R8G8B8A8_UNORM             = 28,
	DXGI_FORMAT_R8G8B8A8_UNORM_SRGB        = 29,
	DXGI_FORMAT_R8G8B8A8_UINT              = 30,
	DXGI_FORMAT_R8G8B8A8_SNORM             = 31,
	DXGI_FORMAT_R8G8B8A8_SINT              = 32,
	DXGI_FORMAT_R16G16_TYPELESS            = 33,
	DXGI_FORMAT_R16G16_FLOAT               = 34,
	DXGI_FORMAT_R16G16_UNORM               = 35,
	DXGI_FORMAT_R16G16_UINT                = 36,
	DXGI_FORMAT_R16G16_SNORM               = 37,
	DXGI_FORMAT_R16G16_SINT                = 38,
	DXGI_FORMAT_R32_TYPELESS               = 39,
	DXGI_FORMAT_D32_FLOAT                  = 40,
	DXGI_FORMAT_R32_FLOAT                  = 41,
	DXGI_FORMAT_R32_UINT                   = 42,
	DXGI_FORMAT_R32_SINT                   = 43,
	DXGI_FORMAT_R24G8_TYPELESS             = 44,
	DXGI_FORMAT_D24_UNORM_S8_UINT          = 45,
	DXGI_FORMAT_R24_UNORM_X8_TYPELESS      = 46,
	DXGI_FORMAT_X24_TYPELESS_G8_UINT       = 47,
	DXGI_FORMAT_R8G8_TYPELESS              = 48,
	DXGI_FORMAT_R8G8_UNORM                 = 49,
	DXGI_FORMAT_R8G8_UINT                  = 50,
	DXGI_FORMAT_R8G8_SNORM                 = 51,
	DXGI_FORMAT_R8G8_SINT                  = 52,
	DXGI_FORMAT_R16_TYPELESS               = 53,
	DXGI_FORMAT_R16_FLOAT                  = 54,
	DXGI_FORMAT_D16_UNORM                  = 55,
	DXGI_FORMAT_R16_UNORM                  = 56,
	DXGI_FORMAT_R16_UINT                   = 57,
	DXGI_FORMAT_R16_SNORM                  = 58,
	DXGI_FORMAT_R16_SINT                   = 59,
	DXGI_FORMAT_R8_TYPELESS                = 60,
	DXGI_FORMAT_R8_UNORM                   = 61,
	DXGI_FORMAT_R8_UINT                    = 62,
	DXGI_FORMAT_R8_SNORM                   = 63,
	DXGI_FORMAT_R8_SINT                    = 64,
	DXGI_FORMAT_A8_UNORM                   = 65,
	DXGI_FORMAT_R1_UNORM                   = 66,
	DXGI_FORMAT_R9G9B9E5_SHAREDEXP         = 67,
	DXGI_FORMAT_R8G8_B8G8_UNORM           = 68,
	DXGI_FORMAT_G8R8_G8B8_UNORM           = 69,
	DXGI_FORMAT_BC1_TYPELESS               = 70,
	DXGI_FORMAT_BC1_UNORM                  = 71,
	DXGI_FORMAT_BC1_UNORM_SRGB             = 72,
	DXGI_FORMAT_BC2_TYPELESS               = 73,
	DXGI_FORMAT_BC2_UNORM                  = 74,
	DXGI_FORMAT_BC2_UNORM_SRGB             = 75,
	DXGI_FORMAT_BC3_TYPELESS               = 76,
	DXGI_FORMAT_BC3_UNORM                  = 77,
	DXGI_FORMAT_BC3_UNORM_SRGB             = 78,
	DXGI_FORMAT_BC4_TYPELESS               = 79,
	DXGI_FORMAT_BC4_UNORM                  = 80,
	DXGI_FORMAT_BC4_SNORM                  = 81,
	DXGI_FORMAT_BC5_TYPELESS               = 82,
	DXGI_FORMAT_BC5_UNORM                  = 83,
	DXGI_FORMAT_BC5_SNORM                  = 84,
	DXGI_FORMAT_B5G6R5_UNORM               = 85,
	DXGI_FORMAT_B5G5R5A1_UNORM             = 86,
	DXGI_FORMAT_B8G8R8A8_UNORM             = 87,
	DXGI_FORMAT_B8G8R8X8_UNORM             = 88,
	DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM = 89,
	DXGI_FORMAT_B8G8R8A8_TYPELESS          = 90,
	DXGI_FORMAT_B8G8R8A8_UNORM_SRGB        = 91,
	DXGI_FORMAT_B8G8R8X8_TYPELESS          = 92,
	DXGI_FORMAT_B8G8R8X8_UNORM_SRGB        = 93,
	DXGI_FORMAT_BC6H_TYPELESS              = 94,
	DXGI_FORMAT_BC6H_UF16                  = 95,
	DXGI_FORMAT_BC6H_SF16                  = 96,
	DXGI_FORMAT_BC7_TYPELESS               = 97,
	DXGI_FORMAT_BC7_UNORM                  = 98,
	DXGI_FORMAT_BC7_UNORM_SRGB             = 99,
	DXGI_FORMAT_AYUV                       = 100,
	DXGI_FORMAT_Y410                       = 101,
	DXGI_FORMAT_Y416                       = 102,
	DXGI_FORMAT_NV12                       = 103,
	DXGI_FORMAT_P010                       = 104,
	DXGI_FORMAT_P016                       = 105,
	DXGI_FORMAT_420_OPAQUE                 = 106,
	DXGI_FORMAT_YUY2                       = 107,
	DXGI_FORMAT_Y210                       = 108,
	DXGI_FORMAT_Y216                       = 109,
	DXGI_FORMAT_NV11                       = 110,
	DXGI_FORMAT_AI44                       = 111,
	DXGI_FORMAT_IA44                       = 112,
	DXGI_FORMAT_P8                         = 113,
	DXGI_FORMAT_A8P8                       = 114,
	DXGI_FORMAT_B4G4R4A4_UNORM             = 115,
	DXGI_FORMAT_P208                       = 130,
	DXGI_FORMAT_V208                       = 131,
	DXGI_FORMAT_V408                       = 132,
	DXGI_FORMAT_SAMPLER_FEEDBACK_MIN_MIP_OPAQUE,
	DXGI_FORMAT_SAMPLER_FEEDBACK_MIP_REGION_USED_OPAQUE,
	DXGI_FORMAT_FORCE_UINT                 = 0xffffffff
};
#define DDS_RESOURCE_MISC_TEXTURECUBE 0x4
struct DDS_HEADER_DXT10
{
	DXGI_FORMAT dxgiFormat;
	uint32_t    resourceDimension;
	uint32_t    miscFlag;   // see D3D11_RESOURCE_MISC_FLAG
	uint32_t    arraySize;
	uint32_t    miscFlags2; // see DDS_MISC_FLAGS2
};

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

bool load_dds_file(Texture* output, IGraphicsTexture*& out_ptr, uint8_t* buffer, int len) {
	ASSERT(buffer && len > 0);
	if (len < 4 + (int)sizeof(ddsFileHeader_t))
		return false;
	if (buffer[0] != 'D' || buffer[1] != 'D' || buffer[2] != 'S' || buffer[3] != ' ')
		return false;
	// BIG ENDIAN ISSUE:
	ddsFileHeader_t* header = (ddsFileHeader_t*)(buffer + 4);

	const uint32_t dxt1_fourcc = 'D' | ('X' << 8) | ('T' << 16) | ('1' << 24); // aka bc1
	const uint32_t dxt5_fourcc = 'D' | ('X' << 8) | ('T' << 16) | ('5' << 24); // aka bc3
	const uint32_t bc4u_fourcc = 'B' | ('C' << 8) | ('4' << 16) | ('U' << 24);
	const uint32_t bc5u_fourcc = 'B' | ('C' << 8) | ('5' << 16) | ('U' << 24);

	GraphicsTextureFormat fmt{};
	int input_width  = header->Width;
	int input_height = header->Height;

	DDS_HEADER_DXT10* dx10 = nullptr;
	const uint32_t dx10FourCC = 'D' | ('X' << 8) | ('1' << 16) | ('0' << 24);
	using gtf = GraphicsTextureFormat;

	if (header->ddspf.Flags & DDSF_FOURCC) {
		if (header->ddspf.FourCC == dxt1_fourcc) {
			fmt = gtf::bc1; // DDSF_ALPHAPIXELS branch same result
		} else if (header->ddspf.FourCC == dxt5_fourcc) {
			fmt = gtf::bc3;
		} else if (header->ddspf.FourCC == bc4u_fourcc) {
			fmt = gtf::bc4;
		} else if (header->ddspf.FourCC == bc5u_fourcc) {
			fmt = gtf::bc5;
		} else if ((header->ddspf.Flags & DDSF_FOURCC) && header->ddspf.FourCC == dx10FourCC) {
			dx10 = (DDS_HEADER_DXT10*)(buffer + 4 + sizeof(ddsFileHeader_t));

			if (dx10->dxgiFormat == DXGI_FORMAT_BC1_UNORM_SRGB)
				fmt = gtf::bc1_srgb;
			else if (dx10->dxgiFormat == DXGI_FORMAT_BC1_UNORM)
				fmt = gtf::bc1;
			else
				ASSERT(0 && "UNHANDLED DDS FORMAT");
		} else {
			ASSERT(0 && "bad fourcc");
		}
	} else {
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
		args.width        = input_width;
		args.height       = input_height;
		args.num_mip_maps = numMipmaps;
		args.format       = fmt;
		args.sampler_type = GraphicsSamplerType::AnisotropyDefault;
		return IGraphicsDevice::inst->create_texture(args);
	};
	out_ptr = create_gpu_texture();
	const bool compressed = out_ptr->is_compressed();

	int ux = input_width;
	int uy = input_height;
	uint8_t* data_ptr = (buffer + 4 + sizeof(ddsFileHeader_t));
	if (dx10)
		data_ptr += sizeof(DDS_HEADER_DXT10);

	int compressed_stride = 0;
	if (compressed)
		compressed_stride = out_ptr->get_compressed_stride();
	for (int i = 0; i < numMipmaps; i++) {
		int size = 0;
		if (compressed) {
			size = ((ux + 3) / 4) * ((uy + 3) / 4) * compressed_stride;
		} else {
			size = ux * uy * int(header->ddspf.RGBBitCount / 8);
		}

		out_ptr->sub_image_upload(i, 0, 0, ux, uy, size, data_ptr);

		data_ptr += size;
		ux /= 2;
		uy /= 2;
		if (ux < 1) ux = 1;
		if (uy < 1) uy = 1;
	}

	return true;
}

bool dxgi_texture_format_to_gl(DXGI_FORMAT infmt, GraphicsTextureFormat* outfmt) {
	ASSERT(outfmt);
	switch (infmt) {
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
bool load_dds_file_specialized_format(IGraphicsTexture*& out_ptr, uint8_t* buffer, int len) {
	ASSERT(buffer && len > 0);
	if (len < 4 + (int)sizeof(ddsFileHeader_t))
		return false;
	if (buffer[0] != 'D' || buffer[1] != 'D' || buffer[2] != 'S' || buffer[3] != ' ')
		return false;
	// BIG ENDIAN ISSUE:
	ddsFileHeader_t* header = (ddsFileHeader_t*)(buffer + 4);

	int input_width  = header->Width;
	int input_height = header->Height;

	int cubemap_array_size = 0;
	uint32_t dx10FourCC = 'D' | ('X' << 8) | ('1' << 16) | ('0' << 24);
	DDS_HEADER_DXT10 dx10 = {};
	if ((header->ddspf.Flags & DDSF_FOURCC) && header->ddspf.FourCC == dx10FourCC) {
		dx10 = *(DDS_HEADER_DXT10*)(buffer + 4 + sizeof(ddsFileHeader_t));
	} else {
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
		args.width              = input_width;
		args.height             = input_height;
		args.num_mip_maps       = numMipmaps;
		args.format             = type;
		args.float_input_is_16f = true;
		if (cubemap_array_size > 0) {
			args.type         = GraphicsTextureType::tCubemapArray;
			args.depth_3d     = cubemap_array_size * 6;
			args.sampler_type = GraphicsSamplerType::CubemapDefault;
		} else {
			args.sampler_type = GraphicsSamplerType::LinearNoMipmaps;
			if (numMipmaps > 1)
				sys_print(Warning, "specilized format has more than 1 mipmap\n");
		}
		return IGraphicsDevice::inst->create_texture(args);
	};
	out_ptr = create_gpu_texture();

	uint8_t* data_ptr = (buffer + 4 + sizeof(ddsFileHeader_t) + sizeof(DDS_HEADER_DXT10));

	const bool compressed        = out_ptr->is_compressed();
	const int  compressed_stride = 16; // for bc6 fixme
	const int  bytes_per_pixel   = 4;  // fixme, hardcoded for rg16f and r11g11b10
	int num_layers = 1;
	if (cubemap_array_size > 0)
		num_layers = cubemap_array_size * 6;
	for (int layer = 0; layer < num_layers; layer++) {
		int ux = input_width;
		int uy = input_height;
		for (int i = 0; i < numMipmaps; i++) {
			int size = 0;
			if (compressed) {
				size = ((ux + 3) / 4) * ((uy + 3) / 4) * compressed_stride;
			} else {
				size = ux * uy * bytes_per_pixel;
			}

			if (num_layers == 1)
				out_ptr->sub_image_upload(i, 0, 0, ux, uy, size, data_ptr);
			else
				out_ptr->sub_image_upload_3d(layer, i, 0, 0, ux, uy, size, data_ptr);

			data_ptr += size;
			ux /= 2;
			uy /= 2;
			if (ux < 1) ux = 1;
			if (uy < 1) uy = 1;
		}
	}

	return true;
}
