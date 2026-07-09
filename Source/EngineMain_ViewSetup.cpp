#define IMGUI_DEFINE_MATH_OPERATORS
#include <SDL3/SDL.h>
#include "glad/glad.h"
#include "GameEngineLocal.h"
#include "glm/ext/matrix_transform.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "Framework/MathLib.h"
#include "Framework/Config.h"
#include "Render/DrawPublic.h"
#include "Game/Components/CameraComponent.h"
#include "Sound/SoundPublic.h"
#include "imgui.h"
#include "UI/GUISystemPublic.h"
#include "Framework/SysPrint.h"
#include "IEditorTool.h"

#include "Logging.h"

// Declared in EngineMain.cpp
extern ConfigVar disable_render_time_tick;

// Force the editor scene viewport to a fixed resolution, overriding whatever
// size the imgui dock layout (editor.ini) hands us. 0 = disabled (use the real
// panel size). Used by screenshot tests so goldens stay deterministic across
// dock-layout changes. Both must be > 0 to take effect.
ConfigVar ed_force_viewport_w("ed.force_viewport_w", "0", CVAR_INTEGER | CVAR_DEV,
							  "force editor scene viewport width (0=off)", 0, 4000);
ConfigVar ed_force_viewport_h("ed.force_viewport_h", "0", CVAR_INTEGER | CVAR_DEV,
							  "force editor scene viewport height (0=off)", 0, 4000);

// ---------------------------------------------------------------------------
// is_drawing_to_window_viewport
// ---------------------------------------------------------------------------

bool GameEngineLocal::is_drawing_to_window_viewport() const {
	ASSERT(UiSystem::inst);
	return !UiSystem::inst->is_drawing_to_screen();
}

// ---------------------------------------------------------------------------
// draw_any_imgui_interfaces — stub, reserved for per-frame imgui overlays
// ---------------------------------------------------------------------------

#include "Framework/MyImguiLib.h"
void GameEngineLocal::draw_any_imgui_interfaces() {}

// ---------------------------------------------------------------------------
// get_draw_params — builds SceneDrawParamsEx and View_Setup for the frame
// ---------------------------------------------------------------------------

void GameEngineLocal::get_draw_params(SceneDrawParamsEx& params, View_Setup& setup) {
	ASSERT(UiSystem::inst);

	const float time_to_use = (disable_render_time_tick.get_bool()) ? 0 : time;

	params = SceneDrawParamsEx(time_to_use, frame_time);
	params.output_to_screen = UiSystem::inst->is_drawing_to_screen();

	View_Setup vs_for_gui;
	auto viewport = UiSystem::inst->get_vp_rect().get_size();
	vs_for_gui.width = viewport.x;
	vs_for_gui.height = viewport.y;

#ifdef EDITOR_BUILD
	if (editor_tool) {
		params.is_editor = true;
		auto vs = editor_tool->get_vs();

		if (!vs) {
			params.draw_world = false;
			vs = &vs_for_gui;
		}
		isound->set_listener_position(vs->origin, glm::normalize(glm::cross(vs->front, glm::vec3(0, 1, 0))));
		setup = *vs;

		// Deterministic override for tests: pin the scene viewport to a fixed size
		// so screenshot goldens don't depend on the imgui dock layout. Rebuild the
		// View_Setup from the same camera so the projection aspect stays correct.
		const int fw = ed_force_viewport_w.get_integer();
		const int fh = ed_force_viewport_h.get_integer();
		if (fw > 0 && fh > 0)
			setup = View_Setup(setup.view, setup.fov, setup.near, setup.far, fw, fh);
	} else
#endif
	{
		params = SceneDrawParamsEx(time_to_use, frame_time);
		params.output_to_screen = UiSystem::inst->is_drawing_to_screen();
		View_Setup vs_for_gui2;
		auto viewport2 = UiSystem::inst->get_vp_rect().get_size();
		vs_for_gui2.width = viewport2.x;
		vs_for_gui2.height = viewport2.y;

		CameraComponent* scene_camera = CameraComponent::get_scene_camera();

		if (!scene_camera) {
			params.draw_world = false;
			params.draw_ui = true;
			setup = vs_for_gui2;
		} else {
			glm::mat4 view;
			float fov = 60.f;
			scene_camera->get_view(view, fov);

			glm::mat4 in = glm::inverse(view);
			auto pos = in[3];
			auto front = -in[2];
			View_Setup vs = View_Setup(view, glm::radians(fov), 0.01, 100.0, viewport2.x, viewport2.y);
			scene_camera->last_vs = vs;
			setup = vs;

			isound->set_listener_position(vs.origin, glm::normalize(glm::cross(vs.front, glm::vec3(0, 1, 0))));
		}
	}
}

// ---------------------------------------------------------------------------
// View_Setup constructors
// ---------------------------------------------------------------------------

// RH, reverse Z, infinite far plane perspective matrix
glm::mat4 View_Setup::make_opengl_perspective_with_near_far() const {
	ASSERT(width > 0 && height > 0);
	return glm::perspectiveRH_NO(fov, width / (float)height, near, far);
}

View_Setup::View_Setup(glm::mat4 viewMat, float fov, float near, float far, int width, int height)
	: view(viewMat), fov(fov), near(near), far(far), width(width), height(height) {
	ASSERT(width > 0 && height > 0);
	auto inv = glm::inverse(viewMat);
	this->origin = inv[3];
	this->front = -inv[2];
	const float aspectRatio = width / (float)height;
	proj = MakeInfReversedZProjRH(fov, aspectRatio, near);
	viewproj = proj * view;
}

View_Setup::View_Setup(glm::vec3 origin, glm::vec3 front, float fov, float near, float far, int width, int height)
	: origin(origin), front(front), fov(fov), near(near), far(far), width(width), height(height) {
	ASSERT(width > 0 && height > 0);
	view = glm::lookAt(origin, origin + front, glm::vec3(0, 1.f, 0));

	const float aspectRatio = width / (float)height;
	proj = MakeInfReversedZProjRH(fov, aspectRatio, near);

	viewproj = proj * view;
}
