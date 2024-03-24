#ifndef PLAYERMOVE_H
#define PLAYERMOVE_H
#include "Types.h"
#include "Game_Engine.h"
#include <memory>
using std::unique_ptr;
using std::vector;

class Entity;
class MeshBuilder;
class Animation_Set;


struct LagCompState
{
	int startmatrix = 0;
	int matrixcount = 0;
	glm::vec3 pos = glm::vec3(0.0);
	glm::vec3 rot = glm::vec3(0.0);
	int tick = 0;
};

// uses 10*60*48 + 36*60 + 4 = ~31kb per lag compped
struct LagCompensationComponent
{
	LagCompensationComponent() : 
		hitbox_matricies(10 * 60, glm::mat4x3(1)), 
		state_hist(60), 
		matrixhead(0) {}
	vector<glm::mat4x3> hitbox_matricies;
	vector<LagCompState> state_hist;
	int matrixhead;
};



class Player;
struct ViewmodelComponent
{
	ViewmodelComponent(Player* player); 
	~ViewmodelComponent();

	Model* model;
	Animator animator;
	Player* player;

	void update();
	void update_visuals();

	renderobj_handle viewmodel_handle;

	glm::vec3 lastoffset;
	glm::quat lastrot;
	glm::vec3 last_view_dir;
};

class Player : public Entity
{
public:
	Player() :
		buffered_cmds(8),
		num_buffered_cmds(0) {
	}

	void init() {
		bool is_local_player = &eng->local_player() == this;

		if (is_local_player) {
			viewmodel.reset( new ViewmodelComponent(this) );
		}
	}

	void update() override;

	void move_update(Move_Command command);
	void postc_move(Move_Command command);

	virtual void update_visuals() override;

	unique_ptr<ViewmodelComponent> viewmodel;
	unique_ptr<LagCompensationComponent> lagcomp;
	vector<Move_Command> buffered_cmds;
	int num_buffered_cmds = 0;
};

// called by server+clients
void player_physics_update(Entity* player, Move_Command command);	// physics movement code
void player_post_physics(Entity* player, Move_Command command, bool is_local);		// item and animation code
// server-side function
void player_update(Entity* player);	

void move_variables_menu();

#endif // !PLAYERMOVE_H
