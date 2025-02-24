#pragma once
#include "Game/EntityComponent.h"
#include "Render/DrawPublic.h"
#include "GameEnginePublic.h"

class MeshBuilderComponent;
class MeshComponent;
CLASS_H(CameraComponent, EntityComponent)
public:

	static CameraComponent* get_scene_camera() {
		return scene_camera;
	}

	CameraComponent() {
		set_call_init_in_editor(true);
	}
	void start() override;
	void end() override;

	void on_changed_transform() override {
		if(eng->is_editor_level())
			update_meshbuilder();
	}

	void update_meshbuilder();

	bool get_is_enabled() const {
		return is_enabled;
	}
	void set_is_enabled(bool b) {
		if (is_enabled == b) return;
		if (is_enabled) {
			ASSERT(scene_camera == this);
			scene_camera = nullptr;
		}
		else {
			if (scene_camera) {
				ASSERT(scene_camera->is_enabled);
				scene_camera->is_enabled = false;
			}
			scene_camera = this;
			is_enabled = true;
		}
	}

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