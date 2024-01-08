#ifndef MEDIA_H
#define MEDIA_H
#include <vector>
#include "Shader.h"
#include "Net.h"

class Model;
class Texture;
struct Media
{
	std::vector<Model*> gamemodels;
	Texture* testtex;

	Texture* blobshadow;
};

extern Media media;

#endif // !MEDIA_H
