#ifndef PLAYERMOVE_H
#define PLAYERMOVE_H
#include "MoveCommand.h"
#include "Net.h"
#include "Physics.h"
class MeshBuilder;
class AnimationSet;
// Shared movement code
class PlayerMovement
{
public:
	static const int MAX_EVENTS = 4;

	// caller sets these vars
	MeshBuilder* phys_debug = nullptr;
	//void* user_arg = nullptr;
	void(*trace_callback)(GeomContact* out, PhysContainer obj, bool closest, bool double_sided, int ignore_ent) = nullptr;
	//void(*impact_func)(int, int) = nullptr;
	MoveCommand cmd;
	PlayerState in_state;
	float deltat;
	int ignore_ent;

	float max_ground_speed = 10;
	float max_air_speed = 2;

	int num_events = 0;
	EntityEvent triggered_events[MAX_EVENTS];
	int trig_event_parms[MAX_EVENTS];

	void Run();
	PlayerState* GetOutputState() {
		return &player;
	}
private:
	glm::vec2 inp_dir;
	float inp_len;
	glm::vec3 look_front;
	glm::vec3 look_side;
	PlayerState player;

	void TriggerEvent(EntityEvent type, int parm = 0);

	void ApplyFriction(float friction_val);
	void GroundMove();
	void AirMove();
	void FlyMove();
	void CheckJump();
	void CheckDuck();
	void CheckNans();
	void MoveAndSlide(glm::vec3 delta);
	void CheckGroundState();
};

// Shared weapon code
class WeaponController
{
public:
	static const int MAX_EVENTS = 2;

	// output events
	EntityEvent events[MAX_EVENTS];
	int event_parms[MAX_EVENTS];
	int num_events;

	PlayerState state;
	MoveCommand cmd;
	float deltat;
	float simtime;

	glm::vec3 shoot_vec;

	void Run();
private:
	void TriggerEvent(EntityEvent type, int parm = 0);
};



#endif // !PLAYERMOVE_H
