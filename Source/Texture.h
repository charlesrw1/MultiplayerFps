#ifndef TEXTURE_H
#define TEXTURE_H
#include <string>
#include <cstdint>

struct Texture
{
	std::string name;
	int width = 0;
	int height = 0;
	int channels = 0;
	uint32_t gl_id = 0;
	uint32_t gl_flags = 0;
};

void FreeLoadedTextures();
Texture* FindOrLoadTexture(const char* filename);

#endif // !TEXTURE_H
