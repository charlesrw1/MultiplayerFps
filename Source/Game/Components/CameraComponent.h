#pragma once
#include "Game/EntityComponent.h"
#include "Render/DrawPublic.h"
#include "GameEnginePublic.h"

class MeshBuilderComponent;
class MeshComponent;
class CameraComponent : public Component
{
public:
	CLASS_BODY(CameraComponent);

	CameraComponent() {
		set_call_init_in_editor(true);
	}
	static CameraComponent* get_scene_camera() {
		return scene_camera;
	}
	void start() override;
	void stop() override;
	void on_changed_transform() override {
		if(eng->is_editor_level())
			update_meshbuilder();
	}
	void update_meshbuilder();
	bool get_is_enabled() const {return is_enabled;}
	void set_is_enabled(bool b);

	void get_view(glm::mat4& viewMatrix, float& fov);
#ifdef EDITOR_BUILD
	const char* get_editor_outliner_icon() const final {
		return "eng/editor/camera_icon.png";
	}
#endif
	float fov = 70.f;
	View_Setup last_vs;	// valid for enabled, fixme
private:
	bool is_enabled = false;
	MeshBuilderComponent* editor_mbview = nullptr;
	MeshComponent* editor_mesh = nullptr;
	// GLOBAL CAMERA, renderer uses this
	static CameraComponent* scene_camera;
};