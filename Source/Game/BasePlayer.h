#pragma once
#include "Entity.h"
#include "Types.h"	// for Move_Command

// represents the main controlling player
// this class is the interface for getting the view to render with and the sound listener position
CLASS_H(PlayerBase, Entity)
public:
	virtual void get_view(
		glm::mat4& viewMatrix,
		float& fov
	) = 0;
	virtual void set_input_command(Move_Command cmd) = 0;


	static const PropertyInfoList* get_props() = delete;
};