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

#include "Game/BasePlayer.h"

using std::unique_ptr;
using std::vector;

class Entity;
class MeshBuilder;
class Animation_Set;

CLASS_H(SpawnLogic, ClassBase)
public:
	virtual bool can_spawn_this(PlayerBase* player) const = 0;
};

CLASS_H(SpawnIfOnRedTeam, SpawnLogic)
public:
	bool can_spawn_this(PlayerBase* player) const {
		return true;
	}
};


CLASS_H(PlayerGun, Entity)
public:
	MeshComponent* gunmesh = nullptr;

	Player* owner = nullptr;

	glm::vec3 lastoffset;
	glm::quat lastrot;
	glm::vec3 last_view_dir;

	glm::vec3 viewmodel_offsets = glm::vec3(0.f);
	glm::vec3 view_recoil = glm::vec3(0.f);			// local recoil to apply to view

	glm::vec3 vm_offset = glm::vec3(0.f, -2.9f, 0.f);
	glm::vec3 vm_scale = glm::vec3(1.f);
	float vm_reload_start = 0.f;
	float vm_reload_end = 0.f;
	float vm_recoil_start_time = 0.f;
	float vm_recoil_end_time = 0.f;
	glm::vec3 viewmodel_recoil_ofs = glm::vec3(0.f);
	glm::vec3 viewmodel_recoil_ang = glm::vec3(0.f);
};


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

class PlayerHUD;
class HealthComponent;
class InputUser;
class CharacterController;
CLASS_H(Player, PlayerBase)
public:

	Player();
	~Player() override;

	MeshComponent* player_mesh{};
	CapsuleComponent* player_capsule{};
	MeshComponent* viewmodel_mesh{};
	HealthComponent* health{};

	InputUser* inputPtr = nullptr;

	EntityPtr<Entity> someEntity;

	SpotLightComponent* spotlight{};

	AssetPtr<Model> a_second_model;

	MulticastDelegate<int> score_update_delegate;

	std::unique_ptr<PlayerHUD> hud;

	std::unique_ptr<CharacterController> ccontroller;
	
	static const PropertyInfoList* get_props() {
		START_PROPS(Player)
			REG_ASSET_PTR(a_second_model, PROP_DEFAULT),
			REG_ENTITY_PTR(someEntity, PROP_DEFAULT)
		END_PROPS(Player)
	};

	// PlayerBase overrides
	void set_input_command(Move_Command cmd) override; 	// called by game before calling update
	void get_view(glm::mat4& viewMatrix, float& fov) override;
	
	// Entity overrides
	void update() override;
	void start() override;

	void end() override;

	void on_jump_callback();

public:
	glm::vec3 calc_eye_position();

	void find_a_spawn_point();

	// current viewangles for player
	glm::vec3 view_angles = glm::vec3(0.f);

	float distTraveledSinceLastFootstep = 0.0;

	// how long has current state been active
	// how long in air? how long on ground?
	float state_time = 0.0;	
	bool is_crouching = false;
	Action_State action = Action_State::Idle;
	
	bool is_on_ground() const { return action != Action_State::Falling && action != Action_State::Jumped; }

	Move_Command cmd;

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

	// physics stuff


	glm::vec3 get_look_vec() {
		return AnglesToVector(view_angles.x, view_angles.y);
	}

};

#endif // !PLAYERMOVE_H
