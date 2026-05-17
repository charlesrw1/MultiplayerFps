#include "InputSystem.h"
#include "Framework/Hashset.h"

#include "Framework/Config.h"

#include "GameEnginePublic.h"
#include <SDL3/SDL.h>

Input::Device::Device(SDL_Gamepad* ptr, int index) : index(index), ptr(ptr) {
	assert(ptr && index >= 0);
	buttonState.resize(SDL_GAMEPAD_BUTTON_COUNT);
}

MulticastDelegate<int, bool> Input::on_con_status;
MulticastDelegate<int> Input::on_any_input;
Input* Input::inst;
Input::Input() {
	int numjoysticks = 0;
	SDL_JoystickID* ids = SDL_GetJoysticks(&numjoysticks);
	sys_print(Debug, "%d existing controllers connected\n", numjoysticks);
	if (ids) SDL_free(ids);

	keyState = SDL_GetKeyboardState(nullptr);
	keyPressedReleasedState.resize(SDL_SCANCODE_COUNT);
	mouseButtonsState.resize(5);
}
Input::~Input() {
	for (Device& c : devices) {
		SDL_CloseGamepad(c.ptr);
		c.ptr = nullptr;
	}
}
void Input::handle_event(const SDL_Event& event) {
	switch (event.type) {
	case SDL_EVENT_GAMEPAD_ADDED: {
		int instance_id = (int)event.gdevice.which;
		if (devices.size() >= 4) {
			sys_print(Warning, "SDL_EVENT_GAMEPAD_ADDED: already at 4-gamepad cap\n");
		} else {
			opt<int> find_existing = find_device_for_index(instance_id);
			if (find_existing.has_value()) {
				sys_print(Warning, "SDL_EVENT_GAMEPAD_ADDED but device already exists %d\n", find_existing.value());
			} else {
				SDL_Gamepad* controller = SDL_OpenGamepad(event.gdevice.which);
				if (controller) {
					devices.push_back(Device(controller, instance_id));
					sys_print(Debug, "Controller %d added to game system\n", instance_id);
					Input::on_con_status.invoke(instance_id, true);
				}
			}
		}
	} break;
	case SDL_EVENT_GAMEPAD_REMOVED: {
		int deviceId = (int)event.gdevice.which;

		SDL_Gamepad* controller = SDL_GetGamepadFromID(event.gdevice.which);
		if (!controller)
			return;

		opt<int> idx = find_device_for_ptr(controller);
		if (idx.has_value()) {
			const int controller_index = devices.at(*idx).index;
			sys_print(Debug, "Controller %d removed from game system\n", controller_index);
			SDL_CloseGamepad(devices.at(*idx).ptr);
			devices.at(*idx).ptr = nullptr;
			devices.erase(devices.begin() + *idx);
			Input::on_con_status.invoke(controller_index, false);
		} else {
			sys_print(Warning, "SDL_EVENT_GAMEPAD_REMOVED does not exist: %d\n", deviceId);
		}
	} break;

	case SDL_EVENT_MOUSE_WHEEL: {
		mouseScrollAcum += (int)event.wheel.y;
		recieved_input_from_this = -1;
	} break;
	case SDL_EVENT_KEY_DOWN: {
		recieved_input_from_this = -1;
		keyPressedReleasedState.at(event.key.scancode).is_pressed = true;
	} break;
	case SDL_EVENT_KEY_UP: {
		keyPressedReleasedState.at(event.key.scancode).is_released = true;
	} break;
	case SDL_EVENT_MOUSE_BUTTON_DOWN: {
		recieved_input_from_this = -1;
		if (event.button.button >= 1 && event.button.button <= 5)
			mouseButtonsState.at((int)event.button.button - 1).is_pressed = true;
	} break;
	case SDL_EVENT_MOUSE_BUTTON_UP: {
		if (event.button.button >= 1 && event.button.button <= 5)
			mouseButtonsState.at((int)event.button.button - 1).is_released = true;
	} break;

	case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
		const int AXIS_EPSILON = 10000;
		if (std::abs((int)event.gaxis.value) > AXIS_EPSILON)
			recieved_input_from_this = (int)event.gaxis.which;
	} break;
	case SDL_EVENT_GAMEPAD_BUTTON_DOWN: {
		recieved_input_from_this = (int)event.gbutton.which;
		opt<int> idx = find_device_for_index((int)event.gbutton.which);
		if (idx.has_value()) {
			devices.at(*idx).buttonState.at(event.gbutton.button).is_pressed = true;
		}
	} break;
	case SDL_EVENT_GAMEPAD_BUTTON_UP: {
		recieved_input_from_this = (int)event.gbutton.which;
		opt<int> idx = find_device_for_index((int)event.gbutton.which);
		if (idx.has_value()) {
			devices.at(*idx).buttonState.at(event.gbutton.button).is_released = true;
		}
	} break;
	case SDL_EVENT_MOUSE_MOTION: {
		recieved_input_from_this = -1;
	} break;
	}
}
void Input::pre_events() {
	mouseScrollAcum = 0;
	for (auto& key : keyPressedReleasedState) {
		key = PressReleaseState();
	}
	for (auto& key : mouseButtonsState) {
		key = PressReleaseState();
	}
	for (auto& d : devices) {
		for (auto& key : d.buttonState)
			key = PressReleaseState();
	}
}
void Input::tick() {
	// if (recieved_input_from_this.has_value()) {
	//	Input::on_any_input.invoke(*recieved_input_from_this);
	//}
	if (devices.empty())
		default_dev_index = std::nullopt;
	else
		default_dev_index = devices.front().index;

	float relX = 0.f, relY = 0.f;
	SDL_GetRelativeMouseState(&relX, &relY);
	mouseXAccum = (int)relX;
	mouseYAccum = (int)relY;

	assert(keyPressedReleasedState.size() <= SDL_SCANCODE_COUNT);

	for (int i = 0; i < keyPressedReleasedState.size(); i++) {
		keyPressedReleasedState[i].is_down = keyState[i];
	}
	float mx = 0.f, my = 0.f;
	const SDL_MouseButtonFlags mouseState = SDL_GetMouseState(&mx, &my);
	mouseX = (int)mx;
	mouseY = (int)my;
	for (int i = 0; i < 3; i++) {
		const bool is_down_this_tick = (mouseState & SDL_BUTTON_MASK(i + 1)) != 0;
		mouseButtonsState[i].is_down = is_down_this_tick;
	}

	for (Device& d : devices) {
		assert(d.buttonState.size() == SDL_GAMEPAD_BUTTON_COUNT);
		for (int i = 0; i < d.buttonState.size(); i++) {
			const bool is_down_this_tick = SDL_GetGamepadButton(d.ptr, (SDL_GamepadButton)i);
			d.buttonState[i].is_down = is_down_this_tick;
		}
	}
}

bool Input::is_shift_down() {
	return is_key_down(SDL_SCANCODE_LSHIFT) || is_key_down(SDL_SCANCODE_RSHIFT);
}
bool Input::is_ctrl_down() {
	return is_key_down(SDL_SCANCODE_LCTRL) || is_key_down(SDL_SCANCODE_RCTRL);
}
bool Input::is_alt_down() {
	return is_key_down(SDL_SCANCODE_LALT) || is_key_down(SDL_SCANCODE_RALT);
}

SDL_GamepadType Input::get_con_type() {
	return get_con_type_idx(inst->default_dev_index.value_or(-1));
}
SDL_GamepadType Input::get_con_type_idx(int idx) {
	auto device = inst->get_device_ptr(idx);
	if (!device)
		return SDL_GamepadType::SDL_GAMEPAD_TYPE_UNKNOWN;
	return SDL_GetGamepadType(device);
}

bool Input::last_recieved_input_from_con() {
	return inst->recieved_input_from_this.has_value() && inst->recieved_input_from_this.value() != -1;
}

void Input::rumble(uint16_t low_freq, uint16_t high_freq, uint32_t duration_ms) {
	auto device = inst->get_device_ptr(inst->default_dev_index.value_or(-1));
	if (device)
		SDL_RumbleGamepad(device, low_freq, high_freq, duration_ms);
}

bool Input::is_key_down(SDL_Scancode key) {
	assert(key >= 0 && key < SDL_SCANCODE_COUNT);
	return inst->keyPressedReleasedState[key].is_down;
}
bool Input::was_key_pressed(SDL_Scancode key) {
	assert(key >= 0 && key < SDL_SCANCODE_COUNT);
	return inst->keyPressedReleasedState[key].is_pressed;
}
bool Input::was_key_released(SDL_Scancode key) {
	assert(key >= 0 && key < SDL_SCANCODE_COUNT);
	return inst->keyPressedReleasedState[key].is_released;
}

ivec2 Input::get_mouse_delta() {
	return ivec2(inst->mouseXAccum, inst->mouseYAccum);
}

ivec2 Input::get_mouse_pos() {
	return ivec2(inst->mouseX, inst->mouseY);
}

bool Input::is_mouse_down(int button) {
	assert(button >= 0 && button <= 4);
	return inst->mouseButtonsState.at(button).is_down;
}

bool Input::was_mouse_pressed(int button) {
	assert(button >= 0 && button <= 4);
	return inst->mouseButtonsState.at(button).is_pressed;
}

bool Input::was_mouse_released(int button) {
	assert(button >= 0 && button <= 4);
	return inst->mouseButtonsState.at(button).is_released;
}

int Input::get_mouse_scroll() {
	return inst->mouseScrollAcum;
}

bool Input::is_con_button_down(SDL_GamepadButton button) {
	return is_con_button_down_idx(button, inst->default_dev_index.value_or(-1));
}

bool Input::was_con_button_pressed(SDL_GamepadButton button) {
	return was_con_button_pressed_idx(button, inst->default_dev_index.value_or(-1));
}

bool Input::was_con_button_released(SDL_GamepadButton button) {
	return was_con_button_released_idx(button, inst->default_dev_index.value_or(-1));
}

double Input::get_con_axis(SDL_GamepadAxis axis) {
	return get_con_axis_idx(axis, inst->default_dev_index.value_or(-1));
}

bool Input::is_con_button_down_idx(SDL_GamepadButton b, int idx) {
	auto device = inst->get_device_ptr(idx);
	if (!device)
		return false;
	return (bool)SDL_GetGamepadButton(device, b);
}

bool Input::was_con_button_pressed_idx(SDL_GamepadButton b, int idx) {
	opt<int> dev = inst->find_device_for_index(idx);
	if (dev.has_value()) {
		return inst->devices.at(*dev).buttonState.at(b).is_pressed;
	}
	return false;
}

bool Input::was_con_button_released_idx(SDL_GamepadButton b, int idx) {
	opt<int> dev = inst->find_device_for_index(idx);
	if (dev.has_value()) {
		return inst->devices.at(*dev).buttonState.at(b).is_released;
	}
	return false;
}

double Input::get_con_axis_idx(SDL_GamepadAxis a, int idx) {
	auto device = inst->get_device_ptr(idx);
	if (!device)
		return 0.0;
	int16_t val = SDL_GetGamepadAxis(device, a);
	return double(val) / double(INT16_MAX);
}

bool Input::is_any_con_active() {
	return !inst->devices.empty();
}

int Input::get_num_active_cons() {
	return (int)inst->devices.size();
}

bool Input::is_con_active(int idx) {
	return inst->find_device_for_index(idx).has_value();
}
SDL_Gamepad* Input::get_device_ptr(int idx) const {
	auto i = find_device_for_index(idx);
	if (i.has_value())
		return devices.at(i.value()).ptr;
	return nullptr;
}
opt<int> Input::find_device_for_index(int idx) const {
	for (int i = 0; i < devices.size(); i++)
		if (devices[i].index == idx)
			return i;
	return std::nullopt;
}
opt<int> Input::find_device_for_ptr(SDL_Gamepad* ptr) const {
	for (int i = 0; i < devices.size(); i++)
		if (devices[i].ptr == ptr)
			return i;
	return std::nullopt;
}
