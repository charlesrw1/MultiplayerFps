#pragma once
#include "Framework/InterfaceTypeInfo.h"
#include "../Game/Entity.h"
#include "../Game/Components/LightComponents.h"

#include "../Game/EntityPtr.h"
#include "../Game/EntityComponent.h"
#include "../Game/GameplayStatic.h"

#include <memory>

class fpsIDamageable {
public:
	INTERFACE_BODY();
	REF virtual void deal_damage(int amount) {}
};


class fpsSpawnPoint : public Component {
public:
	CLASS_BODY(fpsSpawnPoint,spawnable);
	void editor_start() { editor_set_model("cylinder_nose.cmdl", true); }

	REFLECT(type = EntityTarget);
	std::string other;
};
class fpsPropPhysics : public Component {
public:
	CLASS_BODY(fpsPropPhysics, spawnable);
	void editor_start();
	void editor_on_change_property();

	void start();

	REF Model* model = nullptr;
};

class fpsFlickeringLightScript : public Component {
public:
	CLASS_BODY(fpsFlickeringLightScript,spawnable);
	fpsFlickeringLightScript() { set_call_init_in_editor(true); }
	void start();
	void update();
	float evaluate_intsensity();

	REF float min_intensity = 0.0;
	REF float max_intensity = 1.0;
	REF float radius = 5.0;
	REF Color32 color = Color32();
	REF float frequency = 1.f;
	REF float offset = 0.f;
	REF int octaves = 1;
};



class CharacterController;
class CameraComponent;


// items
// swap to
// drop (or no)
// holster
// update

enum class fpsItem {
	empty,
	pistol,
	flamethrower,
	rocket_launcher,
	grenade,
};
enum class fpsItemState {
	deploying,
	idle,
	holstering,
};

struct fpsFlamethrowerData {
};
class fpsPlayer;
class fpsInventoryLogic
{
public:
	void start();
	void update();
	void stop();
	void update_viewmodel();

	fpsPlayer* player = nullptr;
	fpsItem current_item = fpsItem::empty;
	fpsItem next_item = fpsItem::empty;
	fpsItemState state = fpsItemState::idle;

	float state_timer = 0.0;
	float next_attack_time = 0.0;

	fpsFlamethrowerData flamedata;

};

class fpsPlayer : public Component {
public:
	CLASS_BODY(fpsPlayer);
	void start() override;
	void stop() override;

	void manualtick();

	EntityPtr camera;

private:
	void update_look();
	void update_movement();
	void update_camera();

	std::unique_ptr<fpsInventoryLogic> inventory;
	std::unique_ptr<CharacterController> controller;
	glm::vec3 velocity{};
	float view_pitch = 0.f;
	float view_yaw = 0.f;
	bool on_ground = false;
};
