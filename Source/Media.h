#ifndef MEDIA_H
#define MEDIA_H
#include <vector>
#include "Shader.h"

enum GameModels
{
	Mod_PlayerCT,
	Mod_GunM16,
	Mod_Grenade_HE,
	Mod_Grenade_Smoke,

	Mod_Door1,
	Mod_Door2,

	Mod_NUMMODELS
};

class Model;
class Texture;
struct Media
{
	std::vector<Model*> gamemodels;
	Texture* testtex;

	Shader simple;
	Shader textured;
	Shader animated;
};

extern Media media;

#endif // !MEDIA_H
