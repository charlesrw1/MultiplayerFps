#include "LevelEditorCamera.h"
#include "imgui.h"
#include "Framework/Config.h"
#include "EditorInputs.h"

Ray EditorCamera::unproject_mouse(int mx, int my) const {
	Ray r;
	// get ui size

	const auto viewport_size = UiSystem::inst->get_vp_rect().get_size();
	const auto viewport_pos = UiSystem::inst->get_vp_rect().get_pos();

	const auto size = viewport_pos;
	const int wx = viewport_size.x;
	const int wy = viewport_size.y;
	const float aratio = float(wy) / wx;
	glm::vec3 ndc = glm::vec3(float(mx - size.x) / wx, float(my - size.y) / wy, 0);
	ndc = ndc * 2.f - 1.f;
	ndc.y *= -1;

	if (get_is_using_ortho()) {
		glm::vec3 pos = ortho_camera.position - ortho_camera.side * ndc.x * ortho_camera.width +
			ortho_camera.up * ndc.y * ortho_camera.width * aratio;
		glm::vec3 front = ortho_camera.front;
		r.pos = pos;
		r.dir = front;
	}
	else {
		r.pos = vs_setup.origin;

		glm::mat4 invviewproj = glm::inverse(vs_setup.viewproj);
		glm::vec4 point = invviewproj * glm::vec4(ndc, 1.0);
		point /= point.w;

		glm::vec3 dir = glm::normalize(glm::vec3(point) - r.pos);

		r.dir = dir;
	}
	return r;
}

// test:
// ortho selection box
// test mouse picking
// test keypad ortho

bool EditorCamera::handle_events() {

	const bool has_shift = Input::is_shift_down();
	const bool has_ctrl = Input::is_ctrl_down();
	const float ORTHO_DIST = 20.0;
	auto start_interp = [&]() { interp.start_interp(vs_setup); };
	if (Input::was_key_pressed(SDL_SCANCODE_KP_5)) {
		go_to_cam_mode();
		ortho_camera.on_ortho_set.invoke();
	}
	else if (Input::was_key_pressed(SDL_SCANCODE_KP_7) && has_ctrl) {
		mode = OrthoMode;
		ortho_camera.set_position_and_front(camera.orbit_target + glm::vec3(0, -(ORTHO_DIST + 50.0), 0),
			glm::vec3(0, 1, 0));
		start_interp();
	}
	else if (Input::was_key_pressed(SDL_SCANCODE_KP_7)) {
		mode = OrthoMode;
		ortho_camera.set_position_and_front(camera.orbit_target + glm::vec3(0, (ORTHO_DIST + 50.0), 0),
			glm::vec3(0, -1, 0));
		start_interp();
	}
	else if (Input::was_key_pressed(SDL_SCANCODE_KP_3) && has_ctrl) {
		mode = OrthoMode;
		ortho_camera.set_position_and_front(camera.orbit_target + glm::vec3(-ORTHO_DIST, 0, 0), glm::vec3(1, 0, 0));
		start_interp();
	}
	else if (Input::was_key_pressed(SDL_SCANCODE_KP_3)) {
		mode = OrthoMode;
		ortho_camera.set_position_and_front(camera.orbit_target + glm::vec3(ORTHO_DIST, 0, 0), glm::vec3(-1, 0, 0));
		start_interp();
	}
	else if (Input::was_key_pressed(SDL_SCANCODE_KP_1) && has_ctrl) {
		mode = OrthoMode;
		ortho_camera.set_position_and_front(camera.orbit_target + glm::vec3(0, 0, -ORTHO_DIST), glm::vec3(0, 0, 1));
		start_interp();
	}
	else if (Input::was_key_pressed(SDL_SCANCODE_KP_1)) {
		mode = OrthoMode;
		ortho_camera.set_position_and_front(camera.orbit_target + glm::vec3(0, 0, ORTHO_DIST), glm::vec3(0, 0, -1));
		start_interp();
	}
	else
		return false;
	return true;
}
View_Setup EditorCamera::make_view() const {
	return vs_setup;
}
glm::mat4 EditorCamera::make_friendly_imguizmo_matrix() {
	auto window_sz = UiSystem::inst->get_vp_rect().get_size();
	const float aratio = (float)window_sz.y / window_sz.x;
	return (get_is_using_ortho()) ? ortho_camera.get_friendly_proj_matrix(aratio)
		: vs_setup.make_opengl_perspective_with_near_far();
}
void EditorCamera::set_ortho_view(glm::vec3 eye_dir) {
	const float DIST = 70.0f; // matches numpad KP1/3/7 distances
	mode = OrthoMode;
	ortho_camera.set_position_and_front(camera.orbit_target + eye_dir * DIST, -eye_dir);
	interp.start_interp(vs_setup);
	on_ortho_state_change.invoke();
}

void EditorCamera::set_perspective_view() {
	go_to_cam_mode();
	ortho_camera.on_ortho_set.invoke();
	on_ortho_state_change.invoke();
}

EditorCamera* EditorCamera::inst = nullptr;
void ed_cam_debug() {
	if (EditorCamera::inst)
		EditorCamera::inst->imgui();
}
ADD_TO_DEBUG_MENU(ed_cam_debug);

void EditorCamera::imgui() {
	ImGui::Text("is_orbit: %d", (int)get_is_using_ortho());
	auto t = camera.orbit_target;
	ImGui::Text("orbit_target: %f %f %f", t.x, t.y, t.z);
	float cam_dist = camera.distance;
	ImGui::Text("cam_dist: %f", cam_dist);
	t = camera.position;
	ImGui::Text("cam_pos: %f %f %f", t.x, t.y, t.z);
	t = ortho_camera.position;
	ImGui::Text("ortho_pos: %f %f %f", t.x, t.y, t.z);
}
void EditorCamera::on_focused_tick(EditorInputs& inputs) {
	do_update_flag = true;
}
void EditorCamera::tick(EditorInputs& inputs, float dt) {

	auto window_sz = UiSystem::inst->get_vp_rect().get_size();
	float aratio = (float)window_sz.y / window_sz.x;
	float fov = glm::radians(g_fov.get_float());

	if (inputs.can_use_mouse_click() && UiSystem::inst->is_vp_hovered()) {
		if (get_is_using_ortho() && ortho_camera.can_take_input()) {
			inputs.set_focus(this);
		}
		else if (Input::is_mouse_down(2)) {
			UiSystem::inst->set_game_capture_mouse(true);
			inputs.set_focus(this);
		}
		else if (Input::is_mouse_down(1)) {
			inputs.set_focus(this);
		}
	}

	if (get_is_using_ortho() && inputs.get_focused() == nullptr && Input::get_mouse_scroll() != 0)
		do_update_flag = true; // hack

	if (do_update_flag) {

		auto window_sz = UiSystem::inst->get_vp_rect().get_size();
		float aratio = (float)window_sz.y / window_sz.x;
		float fov = glm::radians(g_fov.get_float());

		if (get_is_using_ortho()) {
			ortho_camera.update_from_input(aratio);
			// get orbit target

			const glm::vec3 diff = -camera.orbit_target + ortho_camera.position;
			glm::vec3 side = ortho_camera.side;
			glm::vec3 up = ortho_camera.up;
			camera.orbit_target += glm::dot(side, diff) * side + glm::dot(up, diff) * up;

			if (!ortho_camera.can_take_input())
				inputs.set_focus(nullptr); // release focus
		}
		else {
			camera.orbit_mode = (Input::is_mouse_down(1) && UiSystem::inst->is_vp_hovered()) ||
				Input::last_recieved_input_from_con(); // && !UiSystem::inst->is_game_capturing_mouse();

			camera.update_from_input(window_sz.x, window_sz.y, aratio, fov);

			if (!Input::is_mouse_down(1) && !Input::is_mouse_down(2)) {
				inputs.set_focus(nullptr); // relase focus
				UiSystem::inst->set_game_capture_mouse(false);
			}
		}
		do_update_flag = false;
	}
	else {
		static int c = 0;
		// printf("no update %d\n", c++);
	}
	if (!get_is_using_ortho())
		vs_setup = View_Setup(camera.position, camera.front, fov, 0.01, 100.0, window_sz.x, window_sz.y);
	else {
		View_Setup vs;
		vs.far = 100.0;
		vs.front = ortho_camera.front;
		vs.origin = ortho_camera.position;
		vs.height = window_sz.y;
		vs.width = window_sz.x;
		vs.proj = ortho_camera.get_proj_matrix(aratio);
		vs.view = ortho_camera.get_view_matrix();
		vs.viewproj = vs.proj * vs.view;
		vs.near = 0.001;
		vs.fov = fov;
		vs.is_ortho = true;
		vs_setup = vs;
	}

	if (interp.is_interping()) {
		vs_setup = interp.get_interp(vs_setup, camera.orbit_target);
	}
}
#include "Game/Entities/Player.h"
#include "OrthoCamera.h"
View_Setup EditorCamera::InterpolateManager::get_interp(View_Setup current, glm::vec3 orbit) {
	GameplayStatic::reset_debug_text_height();
	int adfasdf;
	const float dt = eng->get_dt();
	if (from.is_ortho && current.is_ortho) {
		alpha += dt * 3.0;

		View_Setup out = current;
		const float dist = glm::length(orbit - from.origin);

		glm::quat from_quat = glm::conjugate(glm::quat_cast(from.view));
		glm::quat dest_quat = glm::conjugate(glm::quat_cast(current.view));

		glm::quat slerped = glm::slerp(from_quat, dest_quat, evaluate_easing(Easing::CubicEaseInOut, alpha));
		glm::mat3 rot = glm::mat3_cast(slerped);

		glm::vec3 forward = -rot[2];

		glm::vec3 want_pos = orbit - forward * dist;

		GameplayStatic::debug_text(string_format("%f %f %f", forward.x, forward.y, forward.z));
		GameplayStatic::debug_text(string_format("%f %f %f", want_pos.x, want_pos.y, want_pos.z));

		glm::mat3 R = glm::transpose(rot);
		glm::mat4 view(1.0f);
		view[0] = glm::vec4(R[0], 0.0f);
		view[1] = glm::vec4(R[1], 0.0f);
		view[2] = glm::vec4(R[2], 0.0f);
		view[3] = glm::vec4(-R * want_pos, 1.0f);

		out.front = forward;
		out.view = view;
		out.viewproj = out.proj * out.view;
		out.origin = want_pos;

		if (alpha >= 1.0)
			alpha = -1;
		return out;
	}
	else if (!from.is_ortho && !current.is_ortho) {
		View_Setup out = current;
		alpha += dt * 4.0;

		glm::vec3 want_pos = glm::mix(from.origin, current.origin, evaluate_easing(Easing::CubicEaseInOut, alpha));
		out.origin = want_pos;
		out.view[3] = glm::vec4(-glm::mat3(out.view) * want_pos, 1.0);
		out.viewproj = out.proj * out.view;

		if (alpha >= 1.0)
			alpha = -1;
		return out;
	}
	else {
		alpha = -1;
		return current;
	}
}