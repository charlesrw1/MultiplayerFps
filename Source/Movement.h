#ifndef PLAYERMOVE_H
#define PLAYERMOVE_H
#include "MoveCommand.h"
#include "Net.h"
#include "Physics.h"
class MeshBuilder;
class AnimationSet;

// called by server+clients
void player_physics_update(Entity* player, Move_Command command);
void player_item_udpate(Entity* player, Move_Command command);
// server-side function
void player_update(Entity* player);	

class PlayerMovement
{
public:
	static const int MAX_EVENTS = 4;

	// client/server callbacks
	PhysicsWorld* phys = nullptr;
	void(*fire_weapon)(int entindex, bool altfire) = nullptr;
	void(*play_sound)(glm::vec3 org, int snd_idx) = nullptr;
	void(*set_viewmodel_animation)(const char* str) = nullptr;

	// caller sets these vars
	MeshBuilder* phys_debug = nullptr;
	Move_Command cmd;
	float deltat;
	int entindex;
	float simtime;
	PlayerState player;
	bool isclient = false;	// for debugging

	float max_ground_speed = 10;
	float max_air_speed = 2;

	// output vars
	glm::vec3 view_recoil_add;

	void Run();
private:
	glm::vec2 inp_dir;
	float inp_len;
	glm::vec3 look_front;
	glm::vec3 look_side;

	// movement code
	void ApplyFriction(float friction_val);
	void GroundMove();
	void AirMove();
	void FlyMove();
	void CheckJump();
	void CheckDuck();
	void CheckNans();
	void MoveAndSlide(glm::vec3 delta);
	void CheckGroundState();

	// item code
	void RunItemCode();
};

#endif // !PLAYERMOVE_H
