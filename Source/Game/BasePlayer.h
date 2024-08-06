#pragma once
#include "Entity.h"
#include "Types.h"	// for Move_Command

// represents the main controlling player
// this class is the interface for getting the view to render with and the sound listener position
CLASS_H(PlayerBase, Entity)
public:
	virtual void get_view(
		glm::vec3& pos,
		glm::vec3& front,
		float& fov
	) = 0;
	virtual void set_input_command(Move_Command cmd) = 0;
};