#pragma once
#include "Entity.h"
#include "Framework/ClassTypePtr.h"

CLASS_H(WorldSettings, Entity)
public:
	WorldSettings();

	static const PropertyInfoList* get_props() = delete;
};