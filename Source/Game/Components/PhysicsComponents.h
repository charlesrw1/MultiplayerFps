#pragma once

#include "Game/EntityComponent.h"

CLASS_H(CapsuleComponent, EntityComponent)
public:
	static const PropertyInfoList* get_props() = delete;
};
CLASS_H(BoxComponent, EntityComponent)
public:

};

