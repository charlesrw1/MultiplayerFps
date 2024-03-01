#include "Texture.h"
#include <vector>

#include "glm/glm.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "glad/glad.h"
#include "Util.h"
#include "Key_Value_File.h"

#include "Game_Engine.h"
#include <Windows.h>
#undef min
#undef max


static const char* const texture_folder_path = "./Data/Textures/";

Game_Material_Manager mats;

#define ENSURE(num) if(line.size() < num) { sys_print("bad material definition %s @ %d\n", matname.c_str(), mat.second.linenum); continue;}
void Game_Material_Manager::load_material_file(const char* path, bool overwrite)
{
	Key_Value_File file;
	bool good = file.open(path);
	if (!good) {
		sys_print("Couldn't open material file %s\n", path);
		return;
	}


	string key;
	string temp2;
	for (auto& mat : file.name_to_entry)
	{
		const std::string& matname = mat.first;

		// first check if it exists
		Game_Shader* gs = find_for_name(matname.c_str());
		if (gs && !overwrite) continue;
		else if (gs && overwrite) {
			*gs = Game_Shader();
			gs->name = (matname);
		}
		if (!gs) {
			gs = new Game_Shader;
			gs->name = matname;
			shaders.push_back(gs);
		}

		auto& dict = mat.second.dict;
		const char* str_get = "";

		if (*(str_get = dict.get_string("pbr_full")) != 0) {
			const char* path = string_format("%s_albedo.png", str_get);
			gs->images[Game_Shader::DIFFUSE] = FindOrLoadTexture(path);
			path = string_format("%s_normal-ogl.png", str_get);
			gs->images[Game_Shader::NORMAL] = FindOrLoadTexture(path);
			path = string_format("%s_ao.png", str_get);
			gs->images[Game_Shader::AO] = FindOrLoadTexture(path);
			path = string_format("%s_roughness.png", str_get);
			gs->images[Game_Shader::ROUGHNESS] = FindOrLoadTexture(path);
			path = string_format("%s_metallic.png", str_get);
			gs->images[Game_Shader::METAL] = FindOrLoadTexture(path);
		}

		// assume that ref_shaderX will be defined in the future, so create them if they haven't been
		if (*(str_get = dict.get_string("ref_shader1")) != 0) {
			gs->references[0] = find_for_name(str_get);
			if (!gs->references[0]) {
				Game_Shader* ref = new Game_Shader;
				ref->name = str_get;
				shaders.push_back(ref);
				gs->references[0] = ref;
			}
		}
		if (*(str_get = dict.get_string("ref_shader2")) != 0) {
			gs->references[1] = find_for_name(str_get);
			if (!gs->references[1]) {
				Game_Shader* ref = new Game_Shader;
				ref->name = str_get;
				shaders.push_back(ref);
				gs->references[1] = ref;
			}
		}

		if (*(str_get = dict.get_string("albedo")) != 0) {
			gs->images[Game_Shader::DIFFUSE] = FindOrLoadTexture(str_get);
		}
		if (*(str_get = dict.get_string("normal")) != 0) {
			gs->images[Game_Shader::NORMAL] = FindOrLoadTexture(str_get);
		}
		if (*(str_get = dict.get_string("ao")) != 0) {
			gs->images[Game_Shader::AO] = FindOrLoadTexture(str_get);
		}
		if (*(str_get = dict.get_string("rough")) != 0) {
			gs->images[Game_Shader::ROUGHNESS] = FindOrLoadTexture(str_get);
		}
		if (*(str_get = dict.get_string("metal")) != 0) {
			gs->images[Game_Shader::METAL] = FindOrLoadTexture(str_get);
		}
		if (*(str_get = dict.get_string("special")) != 0) {
			gs->images[Game_Shader::SPECIAL] = FindOrLoadTexture(str_get);
		}

		gs->diffuse_tint = glm::vec4(dict.get_vec3("tint", glm::vec3(1.f)),1.f);

		float default_metal = 0.f;
		if (gs->images[Game_Shader::METAL]) default_metal = 1.f;
		gs->metalness_mult = dict.get_float("metal_val", default_metal);
		gs->roughness_mult = dict.get_float("rough_val", 1.f);
		gs->roughness_remap_range = dict.get_vec2("rough_remap", glm::vec2(0, 1));

		if (*(str_get = dict.get_string("shader")) != 0) {
			if (strcmp(str_get,"blend2") == 0) gs->shader_type = Game_Shader::S_2WAYBLEND;
			else if (strcmp(str_get, "wind")==0) gs->shader_type = Game_Shader::S_WINDSWAY;
			else if (strcmp(str_get, "water")==0) {
				gs->alpha_type = Game_Shader::A_BLEND;
				gs->shader_type = Game_Shader::S_WATER;
			}
			else sys_print("unknown shader type %s\n", str_get);
		}
		if (*(str_get = dict.get_string("alpha")) != 0) {
			if (strcmp(str_get, "add") == 0) gs->alpha_type = Game_Shader::A_ADD;
			else if (strcmp(str_get, "blend") == 0) gs->alpha_type = Game_Shader::A_BLEND;
			else if (strcmp(str_get, "test") == 0) gs->alpha_type = Game_Shader::A_TEST;
		}

		if (*(str_get = dict.get_string("showbackface", "no")) == 0) {
			gs->backface = true;
		}
	}
}
#undef ENSURE;


void Game_Material_Manager::load_material_file_directory(const char* directory)
{
	std::string path = std::string(directory) + '*';

	char buffer[260];

	while (Files::iterate_files_in_dir(path.c_str(), buffer, 260)) {
		std::string path = directory;
		path += buffer;
		load_material_file(path.c_str(), true);
	}

	sys_print("Loaded material directory %s\n", directory);
}

Game_Shader* Game_Material_Manager::create_temp_shader(const char* name)
{
	Game_Shader* gs = find_for_name(name);
	if (!gs) {
		gs = new Game_Shader;
		shaders.push_back(gs);
	}
	else
		*gs = Game_Shader();
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

	glGenTextures(1, &output->gl_id);
	glBindTexture(GL_TEXTURE_2D, output->gl_id);

	GLenum type;
	GLenum internal_format;
	GLenum format;
	bool compressed;
	texture_format_to_gl(input_format, &format, &internal_format, &type, &compressed);

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
			glCompressedTexImage2D(GL_TEXTURE_2D, i, internal_format, ux, uy, 0, size, data_ptr);
		else
			glTexImage2D(GL_TEXTURE_2D, i,  internal_format, ux, uy, 0, format, type, data_ptr);

		data_ptr += size;
		ux /= 2;
		uy /= 2;
		if (ux < 1)ux = 1;
		if (uy < 1)uy = 1;
	}

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 16.f);
	glBindTexture(GL_TEXTURE_2D, 0);

	output->width = input_width;
	output->height = input_height;
	output->format = input_format;


	//return make_from_data(output, input_width, input_height, (buffer + 4 + sizeof(ddsFileHeader_t)), input_format);
}

static bool make_from_data(Texture* output, int x, int y, void* data, Texture_Format informat)
{
	glGenTextures(1, &output->gl_id);
	glBindTexture(GL_TEXTURE_2D, output->gl_id);

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

	if (compressed)
		glCompressedTexImage2D(GL_TEXTURE_2D, 0, internal_format, x, y, 0, size, data);
	else
		glTexImage2D(GL_TEXTURE_2D, 0, internal_format, x, y, 0, format, type, data);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 16.f);
	glGenerateMipmap(GL_TEXTURE_2D);

	glBindTexture(GL_TEXTURE_2D, 0);

	glCheckError();

	output->width = x;
	output->height = y;
	output->format = informat;

	return true;
}

using std::string;
using std::vector;

void FreeTexture(Texture* t)
{
	sys_print("Freeing texture: %s\n", t->name.c_str());
	glDeleteTextures(1, &t->gl_id);
}

void FreeLoadedTextures()
{
	mats.free_all();
}
void Game_Material_Manager::free_all()
{
	for (int i = 0; i < textures.size(); i++) {
		FreeTexture(textures[i]);
		delete textures[i];
	}
	textures.clear();
}

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


Texture* Game_Material_Manager::load_texture(const std::string& path)
{
	int x, y, channels;
	stbi_set_flip_vertically_on_load(false);
	void* data = nullptr;
	bool good = false;
	bool is_float = false;

	std::string path_to_use = path.substr(0, path.rfind('.')) + ".dds";

	if (INVALID_FILE_ATTRIBUTES == GetFileAttributesA(path_to_use.c_str()) && GetLastError() == ERROR_FILE_NOT_FOUND) {
		path_to_use = path;
	}

	// check archive
	File_Buffer* f = Files::open(path_to_use.c_str());
	if (!f) {
		sys_print("Couldn't load texture: %s", path.c_str());
		return nullptr;
	}

	if (path_to_use.find(".dds")!=std::string::npos) {
		Texture* t = new Texture;
		load_dds_file(t, (uint8_t*)f->buffer, f->length);
		t->name = path_to_use;
		return t;
	}
	else if (path_to_use.find(".hdr") != std::string::npos) {
		stbi_set_flip_vertically_on_load(true);
		data = stbi_loadf_from_memory((uint8_t*)f->buffer, f->length, &x, &y, &channels, 0);
		is_float = true;
	}
	else {
		data = stbi_load_from_memory((uint8_t*)f->buffer, f->length, &x, &y, &channels, 0);
		is_float = false;
	}
	Files::close(f);


	if (data == nullptr) {
		sys_print("Couldn't load texture: %s", path.c_str());
		return nullptr;
	}

	Texture* t = new Texture;
	good = make_from_data(t, x, y, data, to_format(channels, is_float));
	stbi_image_free(data);
	t->name = path;

	if (!good) {
		delete t;
		return nullptr;
	}

	return t;
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

Texture* Game_Material_Manager::find_texture(const char* file, bool search_img_directory, bool owner)
{
	std::string path;
	path.reserve(256);
	if(search_img_directory)
		path += texture_folder_path;
	path += file;

	if (!owner) {
		for (int i = 0; i < textures.size(); i++) {
			if (textures[i]->name == path) {
				return textures[i];
			}
		}
	}
	Texture* t = load_texture(path);
	if (!owner && t)
		textures.push_back(t);
	return t;
}

Texture* FindOrLoadTexture(const char* filename)
{
	return mats.find_texture(filename, true, false);
}

Texture* Game_Material_Manager::create_texture_from_memory(const char* name, const uint8_t* data, int data_len)
{
	for (int i = 0; i < textures.size(); i++) {
		if (textures[i]->name == name) {
			return textures[i];
		}
	}

	int width, height, channels;
	uint8_t* imgdat = stbi_load_from_memory(data, data_len, &width, &height, &channels, 0);
	if (!imgdat) {
		printf("Couldn't load from memory: %s", name);
		return nullptr;
	}

	Texture* t = new Texture;
	bool good = make_from_data(t, width, height, (void*)imgdat, to_format(channels, false));
	stbi_image_free((void*)imgdat);
	t->name = name;

	textures.push_back(t);
	return t;
}
Texture* CreateTextureFromImgFormat(uint8_t* inpdata, int datalen, std::string name)
{
	return mats.create_texture_from_memory(name.c_str(), inpdata, datalen);
}
