#pragma once
#include "GameEnginePublic.h"
#include "../../Game/Entity.h"
#include "fpsObjects.h"
#include "fpsDebugCamera.h"
#include <Assets/ScriptableObject.h>
class fpsLuaBridge : public ClassBase {
public:
	CLASS_BODY(fpsLuaBridge,scriptable);
	REF virtual void init() {}
	REF virtual void start_level_script() {}
	REF virtual void update() {}
	REF virtual void imgui_tick() {}
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
	uptr<fpsLuaBridge> lua;

	REF void change_level(const std::string& next_level) { 
		game->stop();
		game->start_level(next_level);
	}
	REF Entity* get_player() const {
		return game->player.get();
	}

	void start() override;
	void update() override;
	void stop() override;
	void on_imgui() override;
};