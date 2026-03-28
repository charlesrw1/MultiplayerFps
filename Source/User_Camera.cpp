#include "User_Camera.h"
#include <glm/gtc/matrix_transform.hpp>
#include "UI/GUISystemPublic.h"
#include "Input/InputSystem.h"
glm::mat4 User_Camera::get_view_matrix() const {
	return glm::lookAt(position, position + front, up);
}
bool User_Camera::can_take_input() const {
	return orbit_mode || UiSystem::inst->is_game_capturing_mouse() || Input::last_recieved_input_from_con();
}

void User_Camera::set_orbit_target(glm::vec3 target, float object_size) {
	orbit_target = target;
	position = orbit_target - front * object_size * 4.f;
}

void User_Camera::update_from_input(int width, int height, float aratio, float fov) {
	// int xpos, ypos;
	// xpos = mouse_dx;
	// ypos = mouse_dy;

	auto deadzone = [](float in) -> float {
		const float dead_zone_val = 0.1;
		return glm::abs(in) > dead_zone_val ? in : 0.f;
	};

	auto mousedelta = Input::get_mouse_delta();

	const float controllerSens = 5.f;
	mousedelta.x += deadzone(Input::get_con_axis(SDL_CONTROLLER_AXIS_RIGHTX)) * controllerSens;
	mousedelta.y += deadzone(Input::get_con_axis(SDL_CONTROLLER_AXIS_RIGHTY)) * controllerSens;

	int xpos = mousedelta.x;
	int ypos = mousedelta.y;

	float x_off = xpos;
	float y_off = ypos;

	float sensitivity = 0.01;
	x_off *= sensitivity;
	y_off *= sensitivity;

	auto update_pitch_yaw = [&]() {
		yaw += x_off;
		pitch -= y_off;

		if (pitch > HALFPI - 0.01)
			pitch = HALFPI - 0.01;
		if (pitch < -HALFPI + 0.01)
			pitch = -HALFPI + 0.01;

		if (yaw > TWOPI) {
			yaw -= TWOPI;
		}
		if (yaw < 0) {
			yaw += TWOPI;
		}
	};

	if (orbit_mode) {

		// auto keystate = SDL_GetKeyboardState(nullptr);
		bool pan_in_orbit_model = Input::is_shift_down(); // keystate[SDL_SCANCODE_LSHIFT];

		if (!pan_in_orbit_model) {
			update_pitch_yaw();
		}

		front = AnglesToVector(pitch, yaw);
		glm::vec3 right = normalize(cross(up, front));
		glm::vec3 real_up = glm::cross(right, front);
		distance = glm::length(orbit_target - position);

		// panning
		float x_orb = -deadzone(Input::get_con_axis(SDL_CONTROLLER_AXIS_LEFTX)) * distance * 0.2;
		float y_orb = -deadzone(Input::get_con_axis(SDL_CONTROLLER_AXIS_LEFTY)) * distance * 0.2;

		if (pan_in_orbit_model && !Input::last_recieved_input_from_con()) {
			// scale by dist, not accurate, fixme
			float x_s = tan(fov / 2) * distance * 0.5;
			float y_s = x_s * aratio;
			x_orb += x_s * x_off;
			y_orb += y_s * y_off;
		}
		orbit_target = orbit_target - real_up * y_orb + right * x_orb;

		position = orbit_target - front * distance;
	}
	else {
		update_pitch_yaw();

		front = AnglesToVector(pitch, yaw);
		glm::vec3 delta = glm::vec3(0.f);
		vec3 right = normalize(cross(up, front));
		if (Input::is_key_down(SDL_SCANCODE_W))
			delta += move_speed * front;
		if (Input::is_key_down(SDL_SCANCODE_S))
			delta -= move_speed * front;
		if (Input::is_key_down(SDL_SCANCODE_A))
			delta += right * move_speed;
		if (Input::is_key_down(SDL_SCANCODE_D))
			delta -= right * move_speed;
		if (Input::is_key_down(SDL_SCANCODE_Z) || Input::is_con_button_down(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER))
			delta += move_speed * up;
		if (Input::is_key_down(SDL_SCANCODE_X) || Input::is_con_button_down(SDL_CONTROLLER_BUTTON_LEFTSHOULDER))
			delta -= move_speed * up;

		delta -= move_speed * front * deadzone(Input::get_con_axis(SDL_CONTROLLER_AXIS_LEFTY));
		delta -= move_speed * right * deadzone(Input::get_con_axis(SDL_CONTROLLER_AXIS_LEFTX));

		position += delta;
	}

	{
		int scroll_amt = Input::get_mouse_scroll();

		if (orbit_mode) {
			float lookatpointdist = dot(position - orbit_target, front);
			glm::vec3 lookatpoint = position + front * lookatpointdist;
			lookatpointdist += (lookatpointdist * 0.25) * scroll_amt;
			if (abs(lookatpointdist) < 0.01)
				lookatpointdist = 0.01;
			position = (lookatpoint - front * lookatpointdist);
		}
		else {
			move_speed += (move_speed * 0.5) * scroll_amt;
			if (abs(move_speed) < 0.000001)
				move_speed = 0.0001;
		}
	}
}