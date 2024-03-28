#pragma once

#include <cstdint>
#include "glm/glm.hpp"
#include "RenderObj.h"
#include "Animation.h"
#include "BaseComponents.h"

class Model;
class GeomContact;

enum Entity_Flags
{
	EF_DEAD = 1,
	EF_FORCED_ANIMATION = 2,
	EF_HIDDEN = 4,
	EF_HIDE_ITEM = 8,
	EF_BOUNCE = 16,
	EF_SLIDE = 32,
	EF_SOLID = 64,
	EF_FROZEN_VIEW = 128,
	EF_TELEPORTED = 256,
};

enum Player_Movement_State
{
	PMS_GROUND = 1,		// on ground, else in air
	PMS_CROUCHING = 2,	// crouching in air or on ground
	PMS_JUMPING = 4,	// first part of jump
};

enum Entity_Physics
{
	EPHYS_NONE,
	EPHYS_PLAYER,
	EPHYS_GRAVITY,
	EPHYS_PROJECTILE,
	EPHYS_MOVER,		// platforms, doors
};

enum class entityclass
{
	EMPTY,	// no/defaut logic
	PLAYER,
	THROWABLE,
	DOOR,
	NPC,

	BOMBZONE,
	SPAWNZONE,

	SPAWNPOINT,
};

typedef uint32_t entityhandle;

class Entity
{
public:
	virtual ~Entity();

	entityhandle selfid = 0;	// eng->ents[]
	entityclass class_ = entityclass::EMPTY;

	glm::vec3 position = glm::vec3(0);
	glm::vec3 rotation = glm::vec3(0);
	int model_index = 0;	// media.gamemodels[]

	glm::vec3 velocity = glm::vec3(0);
	glm::vec3 view_angles = glm::vec3(0.f);

	glm::vec3 esimated_accel = glm::vec3(0.f);

	short state = 0;	// For players: Player_Movement_State
	short flags = 0;	// Entity_Flags

	float timer = 0.f;	// multipurpose timer

	int owner_index = 0;
	int health = 100;
	Game_Inventory inv;

	int physics = EPHYS_NONE;
	glm::vec3 col_size;	// for characters, .x=radius,.y=height; for zones, it is an aabb
	float col_radius = 0.f;
	float col_height = 0;
	int ground_index = 0;

	// for interpolating entities
	glm::vec3 interp_pos;
	glm::vec3 interp_rot;
	float interp_remaining;
	float interp_time;

	int target_ent = -1;
	float in_air_time = 0.f;

	int force_angles = 0;	// 1=force, 2=add
	glm::vec3 diff_angles = glm::vec3(0.f);

	renderobj_handle render_handle = -1;
	Model* model = nullptr;

	Animator anim;

	unique_ptr<RenderInterpolationComponent> interp;

	void set_model(const char* model);

	void physics_update();
	void projectile_physics();
	void gravity_physics();
	void mover_physics();

	void damage(Entity* inflictor, glm::vec3 from, int amount);

	glm::mat4 get_world_transform();

	virtual void update() { }
	virtual void collide(Entity* other, const GeomContact& gc) {}

	virtual void update_visuals();
};