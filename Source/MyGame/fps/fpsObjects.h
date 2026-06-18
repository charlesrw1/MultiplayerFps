#pragma once
#include "Framework/InterfaceTypeInfo.h"
#include "../Game/Entity.h"
#include "../Game/EntityComponent.h"
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
};
class fpsPropPhysics : public Component {
public:
	CLASS_BODY(fpsPropPhysics, spawnable);
	void editor_start();
	void editor_on_change_property();

	void start();

	REF Model* model = nullptr;
};

class CharacterController;
class CameraComponent;

class fpsPlayer : public Component, public fpsIDamageable {
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

	std::unique_ptr<CharacterController> controller;
	glm::vec3 velocity{};
	float view_pitch = 0.f;
	float view_yaw = 0.f;
	bool on_ground = false;
};
