#pragma once

// an empty entity, but used as a marker for a camera position with a sprite
#include "Game/Entity.h"
CLASS_H(CameraPoint, Entity)
public:
	CameraPoint();

	static const PropertyInfoList* get_props() = delete;
};