#ifndef PLAYERMOVE_H
#define PLAYERMOVE_H
#include "MoveCommand.h"
#include "Net.h"
#include "Physics.h"
class MeshBuilder;
class Animation_Set;

// called by server+clients
void player_physics_update(Entity* player, Move_Command command);
void player_item_udpate(Entity* player, Move_Command command);
// server-side function
void player_update(Entity* player);	
void move_variables_menu();

#endif // !PLAYERMOVE_H
