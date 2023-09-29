#ifndef PLAYERMOVE_H
#define PLAYERMOVE_H
#include "MoveCommand.h"
#include "Net.h"
#include "Physics.h"
class MeshBuilder;
class PlayerMovement
{
public:
	// caller sets these vars
	MeshBuilder* phys_debug = nullptr;
	//void* user_arg = nullptr;
	void(*trace_callback)(ColliderCastResult* out, PhysContainer obj, bool closest, bool double_sided) = nullptr;
	//void(*impact_func)(int, int) = nullptr;
	MoveCommand cmd;
	PlayerState in_state;
	float deltat;

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

#endif // !PLAYERMOVE_H
