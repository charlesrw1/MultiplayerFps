#include "Texture.h"
#include <vector>

#include "glm/glm.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "glad/glad.h"
#include "Framework/Util.h"

#include "Game_Engine.h"

#include "Framework/Files.h"
#include "Framework/DictParser.h"

#undef OPAQUE

static const char* const texture_folder_path = "./Data/Textures/";

TextureMan g_imgs;

void texture_format_to_gl(Texture_Format infmt, GLenum* format, GLenum* internal_format, GLenum* type, bool* compressed)
{
	if (infmt == TEXFMT_RGBA16F || infmt == TEXFMT_RGB16F)
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
		*internal_format = GL_RED;
		break;
	};


}

// from doom 3 source

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

int get_mip_map_count(int width, int height)
{
	return floor(log2(glm::max(width, height))) + 1;
}

static bool make_from_data(Texture* output, int x, int y, void* data, Texture_Format informat);
static bool load_dds_file(Texture* output, uint8_t* buffer, int len)
{
	if (len < 4 + sizeof(ddsFileHeader_t)) return false;
	if (buffer[0] != 'D' || buffer[1] != 'D' || buffer[2] != 'S' || buffer[3] != ' ') return false;
	// BIG ENDIAN ISSUE:
	ddsFileHeader_t* header = (ddsFileHeader_t*)(buffer + 4);
	
	uint32_t dxt1_fourcc = 'D' | ('X' << 8) | ('T' << 16) | ('1' << 24);
	uint32_t dxt5_fourcc = 'D' | ('X' << 8) | ('T' << 16) | ('5' << 24);

	Texture_Format input_format;
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
	for (int i = 0; i < numMipmaps; i++) {
		size_t size = 0;
		if (compressed) {
			size = ((ux + 3) / 4) * ((uy + 3) / 4) *
				(input_format == TEXFMT_RGBA8_DXT5 ? 16 : 8);
		}
		else {
			size = ux*uy* (header->ddspf.RGBBitCount / 8);
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


	//return make_from_data(output, input_width, input_height, (buffer + 4 + sizeof(ddsFileHeader_t)), input_format);
}

static bool make_from_data(Texture* output, int x, int y, void* data, Texture_Format informat)
{
	glCreateTextures(GL_TEXTURE_2D, 1, &output->gl_id);

	int x_real = x;
	int y_real = y;

	GLenum type;
	GLenum internal_format;
	GLenum format;
	bool compressed;
	texture_format_to_gl(informat, &format, &internal_format, &type, &compressed);

	size_t size = 0;
	if (compressed) {
		size = ((x + 3) / 4) * ((y + 3) / 4) *
			(informat == TEXFMT_RGBA8_DXT5 ? 16 : 8);
	}

	glTextureStorage2D(output->gl_id, get_mip_map_count(x,y), internal_format, x, y);
	assert(x == x_real && y == y_real);
	if (compressed)
		glCompressedTextureSubImage2D(output->gl_id, 0, 0, 0, x, y, internal_format, size, data);
	else
		glTextureSubImage2D(output->gl_id, 0, 0, 0, x, y, format, type, data);
	glTextureParameteri(output->gl_id, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTextureParameteri(output->gl_id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTextureParameteri(output->gl_id, GL_TEXTURE_MAX_ANISOTROPY, 16.f);
	glGenerateTextureMipmap(output->gl_id);

	glCheckError();

	output->width = x;
	output->height = y;
	output->format = informat;

	return true;
}

using std::string;
using std::vector;


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


bool TextureMan::load_texture(const std::string& path, Texture* t)
{
	int x, y, channels;
	stbi_set_flip_vertically_on_load(false);
	void* data = nullptr;
	bool good = false;
	bool is_float = false;

	std::string path_to_use = path.substr(0, path.rfind('.')) + ".dds";

	auto file = FileSys::open_read(path_to_use.c_str());

	if (!file) {
		path_to_use = path;
		file = FileSys::open_read(path_to_use.c_str());
	}

	// check archive


	if (!file) {
		sys_print("!!! Couldn't load texture: %s", path.c_str());
		return false;
	}

	std::vector<uint8_t> filedata;
	filedata.resize(file->size());
	file->read(filedata.data(), filedata.size());

	if (path_to_use.find(".dds")!=std::string::npos) {
		load_dds_file(t, filedata.data(), filedata.size());
		return true;
	}
	else if (path_to_use.find(".hdr") != std::string::npos) {
		stbi_set_flip_vertically_on_load(true);
		data = stbi_loadf_from_memory(filedata.data(), filedata.size(), &x, &y, &channels, 0);
		is_float = true;
	}
	else {
		stbi_set_flip_vertically_on_load(false);
		data = stbi_load_from_memory(filedata.data(), filedata.size(), &x, &y, &channels, 0);
		printf("%d %d\n", x, y);
		is_float = false;
	}
	file->close();


	if (data == nullptr) {
		sys_print("Couldn't load texture: %s", path.c_str());
		return false;
	}

	good = make_from_data(t, x, y, data, to_format(channels, is_float));
	stbi_image_free(data);

	if (!good) {
		return false;
	}
	t->is_loaded = true;

	//t->bindless_handle = glGetTextureHandleARB(t->gl_id);
	//glMakeTextureHandleResidentARB(t->bindless_handle);

	return true;
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

Texture* TextureMan::find_texture(const char* file, bool search_img_directory, bool owner)
{
	std::string path;
	path.reserve(256);
	if(search_img_directory)
		path += texture_folder_path;
	path += file;

	Texture* t = nullptr;
	if (!owner) {
		auto find = textures.find(file);
		if (find != textures.end())
			return find->second;
		t = new Texture;
		textures[file] = t;
		t->path = path;
	}
	else {
		t = new Texture;
		t->path = path;
	}

	bool good = load_texture(path, t);
	return t;
}


Texture* TextureMan::create_unloaded_ptr(const char* file)
{
	std::string path;
	path.reserve(256);
	path += texture_folder_path;
	path += file;

	auto find = textures.find(file);
	if (find != textures.end())
		return find->second;

	Texture* t = new Texture;
	textures[file] = t;
	t->path = path;
	t->is_loaded = false;
	return t;
}

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
