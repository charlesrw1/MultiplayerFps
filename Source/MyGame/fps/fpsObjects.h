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

class fpsSpawnPoint : public Component {
public:
	CLASS_BODY(fpsSpawnPoint,spawnable);
	void editor_start() { editor_set_model("cylinder_nose.cmdl", true); }
};


class fpsPlayer : public Component, public fpsIDamageable {
public:
	CLASS_BODY(fpsPlayer);
	void start() override;
	void stop() override;

	void manualtick();

	EntityPtr camera;
};
