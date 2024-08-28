#pragma once

// an empty entity, but used as a marker for a camera position

#include "Game/Entity.h"
CLASS_H(CameraPoint, Entity)
public:
	CameraPoint();

	static const PropertyInfoList* get_props() {
		START_PROPS(CameraPoint)
			REG_FLOAT(fov, PROP_DEFAULT, "70.0")
		END_PROPS(CameraPoint)
	}
	float fov = 70.f;
};