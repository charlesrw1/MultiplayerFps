#ifndef PLAYERPHYSICS_H
#define PLAYERPHYSICS_H
#include "glm/glm.hpp"
#include "Net.h"
#include "Physics.h"

enum EntType
{
	Ent_Player,
	Ent_Dummy,
	Ent_Free = 0xff,
};

// State that is transmitted to clients
struct EntityState
{
	int type = Ent_Free;
	glm::vec3 position = glm::vec3(0.f);
	glm::vec3 angles = glm::vec3(0.f);	// for players, these are view angles
	bool ducking = false;
};
// State specific to the client's player that is transmitted
struct PlayerState
{
	glm::vec3 position;
	glm::vec3 velocity;
	glm::vec3 angles;
	bool on_ground = false;
	bool ducking = false;
};

extern const Capsule DEFAULT_COLLIDER;
extern const Capsule CROUCH_COLLIDER;

class Level;
class PlayerPhysics
{
public:
	void Run(void(*trace_capsule)(ColliderCastResult*, const Capsule&, bool), PlayerState* p, MoveCommand cmd, float dt);

private:
	void DoStandardPhysics();
	void ApplyFriction(float friction_val);
	void GroundMove();
	void AirMove();
	void FlyMove();
	void CheckJump();
	void CheckDuck();
	void CheckNans();
	void MoveAndSlide(glm::vec3 delta);
	void CheckGroundState();

	float deltat;
	PlayerState* player;
	MoveCommand inp;
	glm::vec2 inp_dir;
	float inp_len;
	glm::vec3 look_front;
	glm::vec3 look_side;

	int num_touch = 0;
	int touch_ents[16];	// index into client or server entities for all touches that occured
	// trace function callbacks
	void(*trace)(ColliderCastResult*, const Capsule&, bool) = nullptr;
};
#endif