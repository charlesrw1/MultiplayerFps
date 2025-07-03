#ifndef PLAYERMOVE_H
#define PLAYERMOVE_H
#include "Types.h"
#include "GameEnginePublic.h"
#include <memory>

#include "PlayerAnimDriver.h"
#include "Game/Entity.h"
#include "Game/Components/MeshComponent.h"
#include "Game/Components/LightComponents.h"
#include "Game/Components/PhysicsComponents.h"
#include "Framework/MulticastDelegate.h"

using std::unique_ptr;
using std::vector;

class Entity;
class MeshBuilder;
class Animation_Set;


extern void find_spawn_position(Entity* ent);

enum class Action_State
{
	Idle,
	Moving,
	Jumped,
	Falling,
};

struct PlayerFlags
{
	enum Enum
	{
		FrozenView = 1,
	};
};

class HealthComponent;
class InputUser;
class CharacterController;
class BikeEntity;


struct HitResult {
	STRUCT_BODY();
	REF obj<Entity> what;
	REF glm::vec3 pos;
	REF bool hit = false;
};

class GameplayStatic : public ClassBase {
public:
	CLASS_BODY(GameplayStatic);
	REF static Entity* find_entity(string name) {
		return nullptr;
	}
	REF static Entity* spawn_prefab(PrefabAsset* prefab) {
		return nullptr;
	}
	REF static Entity* spawn_entity() {
		return nullptr;
	}
	REF static void change_level() {
		return;
	}
	REF static HitResult cast_ray() {
		return HitResult();
	}
};
//
/// <summary>
/// 
/// </summary>
class LuaInput : public ClassBase {
public:
	CLASS_BODY(LuaInput);
	REF static bool is_key_down(int key) {
		return false;
	}
	REF static bool was_key_pressed(int key) {
		return false;
	}
	REF static bool was_key_released(int key) {
		return false;
	}
	REF static bool is_con_button_down(int con_button) {
		return false;
	}
	REF static float get_con_axis(int con_axis) {
		return 0.0;
	}
};

/// <summary>
/// 
/// </summary>
class Player : public Component {
public:
	CLASS_BODY(Player);

	Player();
	~Player() override;//

	REF virtual void do_something() {}

	MeshComponent* player_mesh{};
	CapsuleComponent* player_capsule{};
	MeshComponent* viewmodel_mesh{};
	HealthComponent* health{};
	SpotLightComponent* spotlight{};

	BikeEntity* bike = nullptr;

	MulticastDelegate<int> score_update_delegate;

	std::unique_ptr<CharacterController> ccontroller;


	// PlayerBase overrides
	void get_view(glm::mat4& viewMatrix, float& fov);
	
	// Entity overrides
	void update() final;
	void start() final;
	void end() final;

	void on_jump_callback();
public:
	glm::vec3 calc_eye_position();

	void find_a_spawn_point();

	void on_foot_update();

	// current viewangles for player
	glm::vec3 view_angles = glm::vec3(0.f);
	glm::quat view_quat{};
	glm::vec3 view_pos{};

	float distTraveledSinceLastFootstep = 0.0;

	// how long has current state been active
	// how long in air? how long on ground?
	float state_time = 0.0;	
	bool is_crouching = false;
	Action_State action = Action_State::Idle;
	
	bool is_on_ground() const { return action != Action_State::Falling && action != Action_State::Jumped; }

	glm::vec3 velocity{};

	bool has_flag(PlayerFlags::Enum flag) const {
		return flags & flag;
	}
	void set_flag(PlayerFlags::Enum flag, bool val) {
		if (val)
			flags = PlayerFlags::Enum(flags | flag);
		else
			flags = PlayerFlags::Enum(flags & (~flag));
	}

	PlayerFlags::Enum flags = {};
private:
	float wall_jump_cooldown = 0.0;
	// physics stuff


	glm::vec3 get_look_vec() {
		return AnglesToVector(view_angles.x, view_angles.y);
	}

};

#endif // !PLAYERMOVE_H
