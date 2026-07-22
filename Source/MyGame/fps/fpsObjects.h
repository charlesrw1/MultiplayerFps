#pragma once
#include "Framework/InterfaceTypeInfo.h"
#include "../Game/Entity.h"
#include "../Game/Components/LightComponents.h"

#include "../Game/EntityPtr.h"
#include "../Game/EntityComponent.h"
#include "../Game/GameplayStatic.h"
#include <glm/glm.hpp>

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
class SpringPogoController;
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

// snapshot of the active controller's state, for the third-person debug camera to visualize
struct fpsPogoDebugInfo {
	bool enabled = false;	// is the spring pogo controller the active movement controller
	bool grounded = false;
	glm::vec3 feet_pos{};
	glm::vec3 ground_point{};
	float ride_height = 0.f;
	float ground_dist = -1.f;
	float compression = 0.f;
	float capsule_height = 0.f;
	float capsule_radius = 0.f;
};

class fpsPlayer : public Component {
public:
	CLASS_BODY(fpsPlayer);
	fpsPlayer();
	~fpsPlayer() override;
	void start() override;
	void stop() override;

	void manualtick();

	REF const std::vector<Component*> get_blah_components() const {
		return get_owner()->get_components();
	}

	fpsPogoDebugInfo get_pogo_debug_info();

	// current look direction/eye, same values used by the fps camera - lets an external
	// (debug) camera mirror where the player is looking without stealing mouse capture.
	glm::vec3 get_eye_position();
	glm::vec3 get_view_forward();

	EntityPtr camera;

private:
	void update_look();
	void update_movement();
	void update_camera();

	std::unique_ptr<fpsInventoryLogic> inventory;
	std::unique_ptr<CharacterController> controller;
	std::unique_ptr<SpringPogoController> pogo_controller;
	glm::vec3 velocity{};
	float view_pitch = 0.f;
	float view_yaw = 0.f;
	bool on_ground = false;
};
