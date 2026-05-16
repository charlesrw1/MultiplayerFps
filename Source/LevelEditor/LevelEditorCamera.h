#pragma once
class EditorInputs;
#include "IInputReciever.h"
#include "Framework/MathLib.h"
#include "Framework/Rect2d.h"
#include "Framework/MulticastDelegate.h"
#include "Render/ViewSetup.h"
#include "OrthoCamera.h"
#include "User_Camera.h"
#include "AllHeader.h"
using std::string;

// Serializable snapshot of an EditorCamera's pose; used by EditorRecents to
// restore camera state when re-opening a recent document.
struct CameraSnapshot {
	bool is_ortho = false;
	// User_Camera (perspective) state
	glm::vec3 position{0};
	glm::vec3 front{1, 0, 0};
	glm::vec3 up{0, 1, 0};
	float yaw = 0;
	float pitch = 0;
	glm::vec3 orbit_target{0};
	float distance = 0;
	bool orbit_mode = false;
	// OrthoCamera state (only meaningful when is_ortho)
	glm::vec3 ortho_position{0};
	glm::vec3 ortho_front{1, 0, 0};
	glm::vec3 ortho_up{0, 1, 0};
	glm::vec3 ortho_side{0, 0, 1};
	float ortho_width = 25.0f;
};

class EditorCamera : public IInputReciever
{
public:
	static EditorCamera* inst;
	EditorCamera() { inst = this; }
	~EditorCamera() { inst = nullptr; }
	string get_name() final { return "editor camera"; }

	void on_focused_tick(EditorInputs& inputs) final;
	bool get_is_using_ortho() const { return mode == OrthoMode; }

	Ray unproject_mouse(int mx, int my) const;
	bool handle_events();

	void tick(EditorInputs& inputs, float dt);
	View_Setup make_view() const;
	void set_ortho_view(glm::vec3 eye_dir);
	void set_perspective_view();

	CameraSnapshot snapshot() const;
	// teleport=true skips the easing interp and snaps straight to s.
	void apply_snapshot(const CameraSnapshot& s, bool teleport = false);

	void set_orbit_target(glm::vec3 v, float r) {
		if (mode == OrthoMode) {
			r = camera.distance * 0.25f;
			// Shift ortho camera so target sits on the view plane; preserve depth along front.
			// Without this, the per-frame rebuild of vs_setup from ortho_camera.position snaps
			// the view back as soon as the interp completes.
			const float front_depth = glm::dot(ortho_camera.position - v, ortho_camera.front);
			ortho_camera.position = v + ortho_camera.front * front_depth;
		}
		camera.set_orbit_target(v, r);

		interp.start_interp(vs_setup);
	}
	glm::mat4 make_friendly_imguizmo_matrix();
	Bounds get_ortho_selection_bounds(Rect2d newRect) const {
		ASSERT(get_is_using_ortho());
		Bounds aabb;
		auto to_worldspace = [&](int x, int y) {
			auto rect = UiSystem::inst->get_vp_rect();
			glm::vec2 normalized(x / float(rect.w), y / float(rect.h));
			normalized = normalized * 2.0f - glm::vec2(1.0);
			float w = ortho_camera.width;
			float aratio = float(rect.h) / rect.w;
			glm::vec3 worldspace = ortho_camera.position - ortho_camera.side * normalized.x * w -
				ortho_camera.up * normalized.y * w * aratio;
			return worldspace;
		};
		glm::vec3 point1 = to_worldspace(newRect.x, newRect.y) - ortho_camera.front * 1000.0f;
		glm::vec3 point2 = to_worldspace(newRect.x + newRect.w, newRect.y + newRect.h) + ortho_camera.front * 1000.0f;
		Bounds camb(point1);
		camb = bounds_union(camb, point2);
		return camb;
	}
	void imgui();

	MulticastDelegate<> on_ortho_state_change;

private:
	bool do_update_flag = false;

	glm::vec3 get_orbit_target() const { return camera.orbit_target; }
	void go_to_cam_mode() {
		if (mode == CamMode)
			return;
		camera.set_orbit_target(camera.orbit_target, camera.distance * 0.25f);
		mode = CamMode;
	}

	OrthoCamera ortho_camera;
	User_Camera camera;
	View_Setup vs_setup;
	MulticastDelegate<> on_ortho_change;
	enum Mode
	{
		OrthoMode,
		CamMode,
	} mode = CamMode;

	class InterpolateManager
	{
	public:
		void start_interp(View_Setup current) {
			alpha = 0;
			from = current;
		}
		bool is_interping() { return alpha >= 0; }
		View_Setup get_interp(View_Setup current, glm::vec3 orbit_target);

	private:
		View_Setup from;
		float alpha = -1;
	};
	InterpolateManager interp;
};
