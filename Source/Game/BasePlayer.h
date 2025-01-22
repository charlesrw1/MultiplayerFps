#pragma once
#include "Entity.h"
#include "Types.h"	// for Move_Command
#include "Render/DrawPublic.h"

// represents the main controlling player
// this class is the interface for getting the view to render with and the sound listener position
CLASS_H(PlayerBase, Entity)
public:
	virtual void get_view(
		glm::mat4& viewMatrix,
		float& fov
	) = 0;

	View_Setup last_view_setup;
	static const PropertyInfoList* get_props() = delete;
};