#pragma once
#include "GameEnginePublic.h"
#include "../../Game/Entity.h"
#include "fpsObjects.h"
#include "fpsDebugCamera.h"

class fpsLuaBridge : public ClassBase {
public:
	CLASS_BODY(fpsLuaBridge);
	REF virtual void start_level_script() {}
	REF virtual void tick() {}
};
class fpsGameMgr {
public:
	void start_level(const std::string& name);
	void update();
	void stop();

	Entity* spawn_player();

	EntityPtr player;
	fpsDebugCamera debug_camera;
};

class fpsApp : public Application {
public:
	CLASS_BODY(fpsApp);

	static fpsApp* inst;

	uptr<fpsGameMgr> game;

	void start() override;
	void update() override;
	void stop() override;
	void on_imgui() override;
};