#include "Texture.h"
#include <vector>


#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "glad/glad.h"
#include "Util.h"
#include "Key_Value_File.h"

#include <Windows.h>

static const char* const texture_folder_path = "Data\\Textures\\";

Game_Material_Manager mats;

#define ENSURE(num) if(line.size() < num) { sys_print("bad material definition %s @ %d\n", mat.name.c_str(), mat.linenum); continue;}
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
			else if (key == "aux1") {
				ENSURE(2);
				gs->images[Game_Shader::AUX1] = FindOrLoadTexture(line.at(1).c_str());
			}
			else if (key == "normal1") {
				ENSURE(2);
				gs->images[Game_Shader::NORMAL1] = FindOrLoadTexture(line.at(1).c_str());
			}
			else if (key == "image2") {
				ENSURE(2);
				gs->images[Game_Shader::BASE2] = FindOrLoadTexture(line.at(1).c_str());
			}
			else if (key == "aux2") {
				ENSURE(2);
				gs->images[Game_Shader::AUX2] = FindOrLoadTexture(line.at(1).c_str());
			}
			else if (key == "normal2") {
				ENSURE(2);
				gs->images[Game_Shader::NORMAL2] = FindOrLoadTexture(line.at(1).c_str());
			}
			else if (key == "special") {
				ENSURE(2);
				gs->images[Game_Shader::SPECIAL] = FindOrLoadTexture(line.at(1).c_str());
			}
			else if (key == "spec_exp") {
				ENSURE(2);
				gs->spec_exponent = std::atof(line.at(1).c_str());
			}
			else if (key == "spec_str") {
				ENSURE(2);
				gs->spec_strength = std::atof(line.at(1).c_str());
			}
			else if (key == "spec_tint") {
				ENSURE(4);
				gs->spec_tint.x = std::atof(line.at(1).c_str());
				gs->spec_tint.y = std::atof(line.at(2).c_str());
				gs->spec_tint.z = std::atof(line.at(3).c_str());
			}
			else if (key == "tint") {
				ENSURE(4);
				gs->diffuse_tint.x = std::atof(line.at(1).c_str());
				gs->diffuse_tint.y = std::atof(line.at(2).c_str());
				gs->diffuse_tint.z = std::atof(line.at(3).c_str());
			}
			else if (key == "physics") {
				// TODO
			}
			else if (key == "shader") {
				ENSURE(2);
				string& t = line.at(1);
				if (t == "blend2") gs->shader_type = Game_Shader::S_2WAYBLEND;
				else if (t == "wind") gs->shader_type = Game_Shader::S_WINDSWAY;
				else if (t == "water") {
					gs->alpha_type = Game_Shader::A_BLEND;
					gs->shader_type = Game_Shader::S_WATER;
				}
				else sys_print("unknown shader type %s\n", t.c_str());
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
			else if (key == "fresnel_transparency") {
				gs->fresnel_transparency = true;
			}
			else if (key == "uscroll") {
				ENSURE(2);
				gs->uscroll = std::atof(line.at(1).c_str());
			}
			else if (key == "vscroll") {
				ENSURE(2);
				gs->vscroll = std::atof(line.at(1).c_str());
			}
			else if (key == "emmisive") {
				gs->emmisive = true;	
			}
			else {
				sys_print("unknown material key %s for %s @ %d", key.c_str(), mat.name.c_str(), mat.linenum);
			}
		}
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

	if (path_to_use.find(".dds") != std::string::npos) {
		Texture* t = new Texture;
		std::vector<char> data;
		std::ifstream infile(path_to_use, std::ios::binary);
		infile.seekg(0,std::ios::end);
		size_t len = infile.tellg();
		data.resize(len);
		infile.seekg(0);
		infile.read(data.data(), len);
		load_dds_file(t, (uint8_t*)data.data(), len);
		t->name = path_to_use;
		return t;
	}
	else if (path_to_use.find(".hdr") != std::string::npos) {
		data = stbi_loadf(path_to_use.c_str(), &x, &y, &channels, 0);
		is_float = true;
	}
	else {
		data = stbi_load(path_to_use.c_str(), &x, &y, &channels, 0);
		is_float = false;
	}


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
