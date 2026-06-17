#pragma once
#include "Framework/InterfaceTypeInfo.h"
#include "../Game/Entity.h"
#include "../Game/EntityComponent.h"

/*
Objects of the fps game.

*/

class fpsIDamageable {
public:
	INTERFACE_BODY();
	REF virtual void deal_damage(int amount) {}
};
class fpsPlayer : public Component, public fpsIDamageable {
public:
	CLASS_BODY(fpsPlayer);
};
