#include "OrthoCamera.h"
#include "Framework/Config.h"
#include "Input/InputSystem.h"
#include "UI/GUISystemPublic.h"

ConfigVar ortho_cam_scroll_amt("ortho_cam_scroll_amt", "0.25", CVAR_FLOAT | CVAR_UNBOUNDED, "");
bool OrthoCamera::can_take_input() const {
	return Input::is_mouse_down(1) && UiSystem::inst->is_vp_focused() && Input::is_shift_down();
}
void OrthoCamera::set_position_and_front(glm::vec3 position, glm::vec3 front) {
	this->position = position;
	this->front = front;
	if (abs(dot(front, glm::vec3(0, 1, 0))) > 0.999) {
		up = glm::vec3(1, 0, 0);
	}
	else
		up = glm::vec3(0, 1, 0);
	side = cross(up, front);

	on_ortho_set.invoke();
}
void OrthoCamera::scroll_callback(int amt) {
	width -= (width * ortho_cam_scroll_amt.get_float()) * amt;
	if (abs(width) < 0.000001)
		width = 0.0001;
}

 glm::mat4 OrthoCamera::get_proj_matrix(float aspect_ratio) const {
	return glm::orthoZO(-width, width, -width * aspect_ratio, width * aspect_ratio, 1000.f, 0.001f /* reverse z*/);
}

// used for ImGuizmo which doesnt like reverse Z

glm::mat4 OrthoCamera::get_friendly_proj_matrix(float aspect_ratio) const {
	return glm::ortho(-width, width, -width * aspect_ratio, width * aspect_ratio, 0.001f, 1000.f);
}

void OrthoCamera::update_from_input(float aspectratio) {
	if (can_take_input()) {
		auto mouseDelta = Input::get_mouse_delta();
		auto rect = UiSystem::inst->get_vp_rect();
		float world_delta_x = (mouseDelta.x / float(rect.w)) * width * 2;
		float world_delta_y = (mouseDelta.y / float(rect.h)) * width * aspectratio * 2;

		position += side * world_delta_x;
		position += up * world_delta_y;
	}
	if (UiSystem::inst->is_vp_hovered()) {
		scroll_callback(Input::get_mouse_scroll());
	}
}
