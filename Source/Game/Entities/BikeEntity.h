#pragma once
#include "Game/EntityComponent.h"
#include "Framework/Reflection2.h"
#include <memory>
class Model;
class Texture;
class CharacterController;

class BikeCppUtils : public ClassBase
{
public:
	CLASS_BODY(BikeCppUtils);
	REF static void debug_pre_draw_bike(Entity* bike);
	REF static void debug_draw_bike(Entity* bike, float wheel_ofs0, float wheel_ofs1, float lifetime);
};
