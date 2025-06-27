#include "InputSystem.h"
#include "Framework/Hashset.h"

#include "Framework/Config.h"
#include "InputAction.h"

#include "GameEnginePublic.h"

Input::Device::Device(SDL_GameController* ptr, int index) : index(index), ptr(ptr) {
	assert(ptr && index >= 0 && index <= 3);
	buttonState.resize(SDL_CONTROLLER_BUTTON_MAX);
}

MulticastDelegate<int, bool> Input::on_con_status;
MulticastDelegate<int> Input::on_any_input;
Input* Input::inst;
Input::Input()
{
	int numjoysticks = SDL_NumJoysticks();
	sys_print(Debug, "%d existing controllers connected\n", numjoysticks);

	keyState = SDL_GetKeyboardState(nullptr);
	keyPressedReleasedState.resize(SDL_NUM_SCANCODES);
	mouseButtonsState.resize(5);
}
Input::~Input()
{
	for (Device& c : devices) {
		SDL_GameControllerClose(c.ptr);
		c.ptr = nullptr;
	}
}
void Input::handle_event(const SDL_Event& event)
{
	switch (event.type)
	{
	case SDL_CONTROLLERDEVICEADDED: {
		int joyindex = event.cdevice.which;
		if (joyindex >= 4||joyindex<0) {
			sys_print(Warning, "SDL_CONTROLLERDEVICEADDED: device index not in [0,3]\n");
		}
		else {
			opt<int> find_existing = find_device_for_index(joyindex);
			if (find_existing.has_value()) {
				sys_print(Warning, "SDL_CONTROLLERDEVICEADDED but device already exists %d\n", find_existing.value());
			}
			else {
				SDL_GameController* controller = SDL_GameControllerOpen(joyindex);
				devices.push_back(Device(controller, joyindex));
				sys_print(Debug, "Controller %d added to game system\n", joyindex);
				Input::on_con_status.invoke(joyindex, true);
			}
		}
	}
	break;
	case SDL_CONTROLLERDEVICEREMOVED: {
		int deviceId = event.cdevice.which;

		SDL_GameController* controller = SDL_GameControllerFromInstanceID(deviceId);
		if (!controller)
			return;

		opt<int> idx = find_device_for_ptr(controller);
		if (idx.has_value()) {
			const int controller_index = devices.at(*idx).index;
			sys_print(Debug, "Controller %d removed from game system\n", controller_index);
			SDL_GameControllerClose(devices.at(*idx).ptr);
			devices.at(*idx).ptr = nullptr;
			devices.erase(devices.begin() + *idx);
			Input::on_con_status.invoke(controller_index, false);
		}
		else{
			sys_print(Warning, "SDL_CONTROLLERDEVICEREMOVED does not exist: %d\n", deviceId);
		}
	}
	break;

	case SDL_MOUSEWHEEL: {
		mouseScrollAcum += event.wheel.y;
		recieved_input_from_this = -1;
	} break;
	case SDL_KEYDOWN: {
		recieved_input_from_this = -1;
		keyPressedReleasedState.at(event.key.keysym.scancode).is_pressed = true;
	}break;
	case SDL_KEYUP: {
		keyPressedReleasedState.at(event.key.keysym.scancode).is_released = true;
	}break;
	case SDL_MOUSEBUTTONDOWN: {
		recieved_input_from_this = -1;
		if (event.button.button >= 1 && event.button.button <= 5)
			mouseButtonsState.at((int)event.button.button - 1).is_pressed = true;
	}break;
	case SDL_MOUSEBUTTONUP: {
		if(event.button.button>=1&&event.button.button<=5)
			mouseButtonsState.at((int)event.button.button - 1).is_released = true;
	}break;

	case SDL_CONTROLLERAXISMOTION: {
		const int AXIS_EPSILON = 10000;
		if(std::abs((int)event.caxis.value) > AXIS_EPSILON)
			recieved_input_from_this = event.caxis.which;
	}break;
	case SDL_CONTROLLERBUTTONDOWN: {
		recieved_input_from_this = event.cbutton.which;
		opt<int> idx = find_device_for_index(event.cbutton.which);
		if (idx.has_value()) {
			devices.at(*idx).buttonState.at(event.cbutton.button).is_pressed = true;
		}
	}break;
	case SDL_CONTROLLERBUTTONUP: {
		recieved_input_from_this = event.cbutton.which;
		opt<int> idx = find_device_for_index(event.cbutton.which);
		if (idx.has_value()) {
			devices.at(*idx).buttonState.at(event.cbutton.button).is_released = true;
		}
	}break;
	case SDL_MOUSEMOTION: {
		recieved_input_from_this = -1;
	}break;
	}
}
void Input::pre_events()
{
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
void Input::tick()
{
	if (recieved_input_from_this.has_value()) {
		Input::on_any_input.invoke(*recieved_input_from_this);
		recieved_input_from_this = std::nullopt;
	}
	if (devices.empty())
		default_dev_index = std::nullopt;
	else
		default_dev_index = 0;

	SDL_GetRelativeMouseState(&mouseXAccum, &mouseYAccum);

	assert(keyPressedReleasedState.size() <= SDL_NUM_SCANCODES);

	for (int i = 0; i < keyPressedReleasedState.size(); i++) {
		keyPressedReleasedState[i].is_down = keyState[i];
	}
	const int mouseState = SDL_GetMouseState(&mouseX, &mouseY);
	for (int i = 0; i < 3; i++) {
		const bool is_down_this_tick = (mouseState&SDL_BUTTON(i + 1))!=0;
		mouseButtonsState[i].is_down = is_down_this_tick;
	}

	for (Device& d : devices) {
		assert(d.buttonState.size() == SDL_CONTROLLER_BUTTON_MAX);
		for (int i = 0; i < d.buttonState.size(); i++) {
			const bool is_down_this_tick = SDL_GameControllerGetButton(d.ptr, (SDL_GameControllerButton)i);
			d.buttonState[i].is_down = is_down_this_tick;
		}
	}
}

bool Input::is_shift_down()
{
	return is_key_down(SDL_SCANCODE_LSHIFT)||is_key_down(SDL_SCANCODE_RSHIFT);
}
bool Input::is_ctrl_down()
{
	return is_key_down(SDL_SCANCODE_LCTRL) || is_key_down(SDL_SCANCODE_RCTRL);
}
bool Input::is_alt_down()
{
	return is_key_down(SDL_SCANCODE_LALT) || is_key_down(SDL_SCANCODE_RALT);
}

SDL_GameControllerType Input::get_con_type()
{
	return get_con_type_idx(inst->default_dev_index.value_or(-1));
}
SDL_GameControllerType Input::get_con_type_idx(int idx)
{
	auto device = inst->get_device_ptr(idx);
	if (!device)
		return SDL_GameControllerType::SDL_CONTROLLER_TYPE_UNKNOWN;
	return SDL_GameControllerGetType(device);
}


bool Input::is_key_down(SDL_Scancode key)
{
	assert(key >= 0 && key < SDL_NUM_SCANCODES);
	return inst->keyPressedReleasedState[key].is_down;
}
bool Input::was_key_pressed(SDL_Scancode key)
{
	assert(key >= 0 && key < SDL_NUM_SCANCODES);
	return inst->keyPressedReleasedState[key].is_pressed;
}
bool Input::was_key_released(SDL_Scancode key)
{
	assert(key >= 0 && key < SDL_NUM_SCANCODES);
	return inst->keyPressedReleasedState[key].is_released;
}

ivec2 Input::get_mouse_delta()
{
	return ivec2(inst->mouseXAccum,inst->mouseYAccum);
}

ivec2 Input::get_mouse_pos()
{
	return ivec2(inst->mouseX,inst->mouseY);
}

bool Input::is_mouse_down(int button)
{
	assert(button >= 0 && button <= 4);
	return inst->mouseButtonsState.at(button).is_down;
}

bool Input::was_mouse_pressed(int button)
{
	assert(button >= 0 && button <= 4);
	return inst->mouseButtonsState.at(button).is_pressed;
}

bool Input::was_mouse_released(int button)
{
	assert(button >= 0 && button <= 4);
	return inst->mouseButtonsState.at(button).is_released;
}

int Input::get_mouse_scroll()
{
	return inst->mouseScrollAcum;
}


bool Input::is_con_button_down(SDL_GameControllerButton button)
{
	return is_con_button_down_idx(button,inst->default_dev_index.value_or(-1));
}

bool Input::was_con_button_pressed(SDL_GameControllerButton button)
{
	return was_con_button_pressed_idx(button, inst->default_dev_index.value_or(-1));
}

bool Input::was_con_button_released(SDL_GameControllerButton button)
{
	return was_con_button_released_idx(button, inst->default_dev_index.value_or(-1));
}

double Input::get_con_axis(SDL_GameControllerAxis axis)
{
	return get_con_axis_idx(axis, inst->default_dev_index.value_or(-1));
}

bool Input::is_con_button_down_idx(SDL_GameControllerButton b, int idx)
{
	auto device = inst->get_device_ptr(idx);
	if (!device)
		return false;
	return (bool)SDL_GameControllerGetButton(device, b);
}

bool Input::was_con_button_pressed_idx(SDL_GameControllerButton b, int idx)
{
	opt<int> dev = inst->find_device_for_index(idx);
	if (dev.has_value()) {
		return inst->devices.at(*dev).buttonState.at(b).is_pressed;
	}
	return false;
}

bool Input::was_con_button_released_idx(SDL_GameControllerButton b, int idx)
{
	opt<int> dev = inst->find_device_for_index(idx);
	if (dev.has_value()) {
		return inst->devices.at(*dev).buttonState.at(b).is_released;
	}
	return false;
}

double Input::get_con_axis_idx(SDL_GameControllerAxis a, int idx)
{
	auto device = inst->get_device_ptr(idx);
	if (!device) 
		return 0.0;
	int16_t val = SDL_GameControllerGetAxis(device, a);
	return double(val) / double(INT16_MAX);
}

bool Input::is_any_con_active()
{
	return !inst->devices.empty();
}

int Input::get_num_active_cons()
{
	return (int)inst->devices.size();
}

bool Input::is_con_active(int idx)
{
	return inst->find_device_for_index(idx).has_value();
}
SDL_GameController* Input::get_device_ptr(int idx) const {
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
opt<int> Input::find_device_for_ptr(SDL_GameController* ptr) const
{
	for (int i = 0; i < devices.size(); i++)
		if (devices[i].ptr == ptr)
			return i;
	return std::nullopt;
}