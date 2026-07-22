#pragma once
#include "User_Camera.h"
#include "Framework/Config.h"
#include "Game/Entity.h"

extern ConfigVar g_debugcamera;
extern ConfigVar g_thirdperson_orbit;

class CameraComponent;
class fpsPlayer;

class fpsDebugCamera {
public:
	void init();
	void shutdown();
	void update(EntityPtr game_camera_ent, fpsPlayer* player);
	void on_imgui();

private:
	void draw_frustum_lines(CameraComponent* game_camera);
	void draw_pogo_visualization(fpsPlayer* player);

	User_Camera fly_cam;
	EntityPtr debug_cam_ent;
	EntityPtr camera_model_ent;
	bool was_active = false;
	bool initialized_fly_cam = false;

	float thirdperson_distance = 3.5f;
	float thirdperson_height = 0.3f;
};
