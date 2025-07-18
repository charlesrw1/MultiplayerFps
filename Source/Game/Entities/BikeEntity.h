#pragma once
#include "Game/EntityComponent.h"
#include "Framework/Reflection2.h"
#include <memory>
class Model;
class Texture;
class CharacterController;
class BikeEntity : public Component {
public:
	CLASS_BODY(BikeEntity);

	BikeEntity();
	void update() final;
	void start() final;

	float turn_strength = 0.0;	// -1,1 r,l
	float forward_strength = 0.0;	// 0,1
	glm::vec3 bike_direction{};
private:
	//REF Model* model_to_swap = nullptr;
	//REF Texture* texture_to_use = nullptr;
	//REF PrefabAsset* prefab = nullptr;

	std::unique_ptr<CharacterController> ccontrol = nullptr;
	float current_turn = 0.0; // -1,1
	float forward_speed = 0.0;
	float current_roll = 0.0;
};

class BikeCppUtils : public ClassBase {
public:
	CLASS_BODY(BikeCppUtils);
	REF static void debug_pre_draw_bike(Entity* bike);
	REF static void debug_draw_bike(Entity* bike, float wheel_ofs0, float wheel_ofs1,float lifetime);
};