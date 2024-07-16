#ifndef PLAYERMOVE_H
#define PLAYERMOVE_H
#include "Types.h"
#include "Game_Engine.h"
#include <memory>

#include "PlayerAnimDriver.h"

using std::unique_ptr;
using std::vector;

class Entity;
class MeshBuilder;
class Animation_Set;



CLASS_H(PlayerGun, Entity)
public:
	MeshComponent gunmesh;

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

CLASS_H(Player, Entity)
public:

	Player() {
		root_component = &player_capsule;
		player_mesh.attach_to_parent(root_component.get(), {});
		viewmodel_mesh.attach_to_parent(&player_mesh, NAME("Prop_Gun"));
	}

	MeshComponent player_mesh;

	CapsuleComponent player_capsule;

	MeshComponent viewmodel_mesh;

	AssetPtr<Model> a_second_model;

	EntityPtr<Entity> gun_entity;
	
	static const PropertyInfoList* get_props() {
		START_PROPS(Player)
			REG_ENTITY_PTR(gun_entity,PROP_DEFAULT),
			REG_COMPONENT(player_capsule,PROP_DEFAULT,"Root"),
			REG_COMPONENT(player_mesh,PROP_DEFAULT,""),
			REG_COMPONENT(viewmodel_mesh, PROP_DEFAULT, ""),
			REG_ASSET_PTR(a_second_model, PROP_DEFAULT)
		END_PROPS(Player)
	};

public:

	// Entity overrides
	void update() override;

	// called by game before calling update
	void set_input_command(Move_Command cmd);

	glm::vec3 calc_eye_position();
	void get_view(glm::vec3& origin, glm::vec3& angles, float& fov);

	void find_a_spawn_point();

	// current viewangles for player
	glm::vec3 view_angles = glm::vec3(0.f);

	// how long has current state been active
	// how long in air? how long on ground?
	float state_time = 0.0;	
	bool is_crouching = false;
	Action_State action = Action_State::Idle;
	
	bool is_on_ground() const { return action != Action_State::Falling && action != Action_State::Jumped; }

	Move_Command cmd;

	virtual glm::vec3 get_velocity() const  override {
		return velocity;
	}

	glm::vec3 velocity = glm::vec3(0.f);

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
	void move();
	bool check_perch();
	void ground_move();

	void get_crouch_state(bool& is_crouching);
	Action_State update_state(const float grnd_speed, bool& dont_add_grav);
	Action_State get_ground_state_based_on_speed(float speed) const;
	void slide_move();

	void item_update();
	void change_to_item(int next);

	glm::vec3 get_look_vec() {
		return AnglesToVector(view_angles.x, view_angles.y);
	}

};

#endif // !PLAYERMOVE_H
