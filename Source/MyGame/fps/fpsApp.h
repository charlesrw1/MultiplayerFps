#pragma once
#include "GameEnginePublic.h"
#include "../../Game/Entity.h"
#include "fpsObjects.h"

class fpsGameMgr {
public:
	void start_level(const std::string& name);
	void update();
	void stop();

	Entity* spawn_player();

	EntityPtr player;

	EntityPtr camera;
};

class fpsApp : public Application {
public:
	CLASS_BODY(fpsApp);

	static fpsApp* inst;

	uptr<fpsGameMgr> game;

	void start() override;
	void update() override;
};