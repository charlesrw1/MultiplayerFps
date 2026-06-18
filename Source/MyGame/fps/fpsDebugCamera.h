#pragma once
#include "User_Camera.h"
#include "Framework/Config.h"
#include "Game/Entity.h"

extern ConfigVar g_debugcamera;

class CameraComponent;

class fpsDebugCamera {
public:
	void init();
	void shutdown();
	void update(EntityPtr game_camera_ent);
	void on_imgui();

private:
	void draw_frustum_lines(CameraComponent* game_camera);

	User_Camera fly_cam;
	EntityPtr debug_cam_ent;
	EntityPtr camera_model_ent;
	bool was_active = false;
	bool initialized_fly_cam = false;
};
