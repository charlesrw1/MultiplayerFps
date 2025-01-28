#include "InputSystem.h"
#include "Framework/Hashset.h"

#include "Framework/Config.h"
#include "InputAction.h"

#include "GameEnginePublic.h"

class GameInputSystemImpl
{
public:
	GameInputSystemImpl() : allUsers(2) {}

	InputDevice* get_device(handle<InputDevice> handle)
	{
		if (!handle.is_valid())
			return nullptr;
		if (handle.id == 0)
			return &keyboardDevice;
		if (handle.id >= 1 && handle.id <= 4)
			return &controllerDevices[handle.id - 1];
		return nullptr;
	}

	hash_set<InputUser> allUsers;

	InputDevice keyboardDevice;
	InputDevice controllerDevices[4];

	std::vector<std::unique_ptr<InputAction>> registered_actions;

	std::unordered_map<std::string, GlobalInputBinding> str_to_keybind;
	std::unordered_map<std::string, int> mapping_id_to_integer;
	int mapping_id_counter = 0;
	int get_mapping_id_for_str(const std::string& id) {
		assert(mapping_id_to_integer.find(id) != mapping_id_to_integer.end());
		return mapping_id_to_integer[id];
	}

	static float sample_device_value_for_binding(GlobalInputBinding b, InputDevice* device);

	int mouseXAccum = 0;
	int mouseYAccum = 0;
	int mouseScrollAccum = 0;
};
static GameInputSystemImpl* implLocal=nullptr;
GameInputSystem::GameInputSystem() {
	impl = new GameInputSystemImpl;
	implLocal = impl;
}
GameInputSystem::~GameInputSystem() {
	delete impl;
	implLocal = nullptr;
}

InputDevice* GameInputSystem::get_keyboard_device()
{
	return &impl->keyboardDevice;
}

void GameInputSystem::init()
{
	impl->keyboardDevice.type = InputDeviceType::KeyboardMouse;
	for (int i = 0; i < 4; i++) {
		impl->controllerDevices[i].index = i;
		impl->controllerDevices[i].type = InputDeviceType::Controller;
	}

	int numjoysticks = SDL_NumJoysticks();
	sys_print(Debug,"%d existing controllers connected\n", numjoysticks);
	using GIB = GlobalInputBinding;
	const int count = (int)GIB::NumInputs;
	for (int i = 0; i < count; i++)
	{
		auto str = get_button_type_string(GIB(i));
		if (!str.empty()) {
			//assert(impl->str_to_keybind.find(str) == impl->str_to_keybind.end());
			// one string maps to multiple keys I guess
			impl->str_to_keybind.insert({ str,GIB(i) });
		}
	}
}
InputAction* GameInputSystem::register_input_action(std::unique_ptr<InputAction> action)
{
	if (impl->mapping_id_to_integer.find(action->mapping_id_str) == impl->mapping_id_to_integer.end()) {
		assert(impl->mapping_id_counter <= 30);
		impl->mapping_id_to_integer.insert({ action->mapping_id_str, impl->mapping_id_counter++ });
	}
	action->mapping_id_num = impl->get_mapping_id_for_str(action->mapping_id_str);
	impl->registered_actions.push_back(std::move(action));
	return impl->registered_actions.back().get();
}
GlobalInputBinding GameInputSystem::find_bind_for_string(const std::string & key_str)
{
	auto find = impl->str_to_keybind.find(key_str);
	return find == impl->str_to_keybind.end() ? GlobalInputBinding::Empty : find->second;
}


std::vector<InputDevice*> GameInputSystem::get_connected_devices()
{
	std::vector<InputDevice*> outDevices;
	outDevices.push_back(&impl->keyboardDevice);
	for (int i = 0; i < 4; i++) {
		if (impl->controllerDevices[i].is_connected())
			outDevices.push_back(&impl->controllerDevices[i]);
	}
	return outDevices;
}
void GameInputSystem::set_input_mapping_status(InputUser* user, const std::string& map_id, bool enable)
{
	int index = impl->get_mapping_id_for_str(map_id);
	if (!user->has_mapping_tracked(index))
	{
		for (int i = 0; i < impl->registered_actions.size(); i++) {
			if (impl->registered_actions[i]->mapping_id_num == index)
			{
				auto action = impl->registered_actions[i].get();
				auto str = action->get_action_name();
				assert(user->trackedActions.find(str) == user->trackedActions.end());
				InputActionInstance& a = user->trackedActions[str];
				a.action = action;
				a.is_enabled = enable;
			}
		}
		user->tracked_mapping_bitmasks |= 1ul << index;
	}
	else {
		if (user->has_mapping_enabled(index) == enable)
			return;

		for (auto& a : user->trackedActions)
		{
			if (a.second.action->mapping_id_num == index)
				a.second.is_enabled = enable;
		}
	}
	if (enable)
		user->mapping_enabled_bitmask |= (1ul << index);
	else
		user->mapping_enabled_bitmask &= ~(1ul << index);
}

InputUser* GameInputSystem::register_input_user(int localPlayerIndex)
{
	InputUser* u = new InputUser;
	u->playerIndex = localPlayerIndex;
	impl->allUsers.insert(u);
	return u;
}
void GameInputSystem::free_input_user(InputUser*& user)
{
	if (user->get_device()) {
		ASSERT(user->get_device()->user == user);
		user->get_device()->set_user(nullptr);
	}

	impl->allUsers.remove(user);
	delete user;
	user = nullptr;
}

void GameInputSystem::handle_event(const SDL_Event& event)
{
	switch (event.type)
	{
	case SDL_CONTROLLERDEVICEADDED:
	{
		int joyindex = event.cdevice.which;
		if (joyindex >= 4) {
			sys_print(Warning,"SDL_CONTROLLERDEVICEADDED: device index is greater than 3\n");
		}
		else {
			auto controller = SDL_GameControllerOpen(joyindex);
			assert(impl->controllerDevices[joyindex].sdl_controller_ptr == nullptr);
			impl->controllerDevices[joyindex].sdl_controller_ptr = controller;
			impl->controllerDevices[joyindex].self_index = { joyindex + 1 };

			sys_print(Debug,"Controller %d added to game system\n",joyindex);

			device_connected.invoke(&impl->controllerDevices[joyindex]);
		}
	}
		break;
	case SDL_CONTROLLERDEVICEREMOVED:
	{
		int deviceId = event.cdevice.which;

		auto controller = SDL_GameControllerFromInstanceID(deviceId);
		if (!controller)
			return;

		int myIndex = 0;
		for (; myIndex < 4; myIndex++) {
			if (impl->controllerDevices[myIndex].sdl_controller_ptr == controller)
				break;
		}
		assert(myIndex != 4);

		sys_print(Debug,"Controller %d removed from game system\n", myIndex);
		auto device = &impl->controllerDevices[myIndex];
		auto wasId = device->self_index;
		device_disconnected.invoke(device);

		auto user = device->user;
		device->self_index = { 0 };
		SDL_GameControllerClose(device->sdl_controller_ptr);
		device->sdl_controller_ptr = nullptr;
		device->user = nullptr;

		if (user) {
			sys_print(Debug,"user lost device\n");
			assert(user->assigned_device == device);
			user->assigned_device = nullptr;
			user->on_changed_device.invoke();
		}
	}
		break;

	case SDL_MOUSEWHEEL:
	{
		impl->mouseScrollAccum += event.wheel.y;
	} break;
	case SDL_KEYDOWN:
	case SDL_CONTROLLERBUTTONDOWN:
	case SDL_MOUSEBUTTONDOWN:
	{
		if (!on_device_input.has_any_listeners())
			return;
	}
	}
}
InputDeviceType get_device_type_for_keybind(GlobalInputBinding bind)
{
	assert(bind != GlobalInputBinding::Empty);
	if (bind < GlobalInputBinding::ControllerButtonStart)
		return InputDeviceType::KeyboardMouse;
	return InputDeviceType::Controller;
}

float GameInputSystemImpl::sample_device_value_for_binding(GlobalInputBinding b, InputDevice* device)
{
	assert(get_device_type_for_keybind(b) == device->type);
	if (b <= GlobalInputBinding::KeyboardEnd) {
		auto keys = SDL_GetKeyboardState(nullptr);
		int index = (int)b - (int)GlobalInputBinding::KeyboardStart;
		assert(index >= 0 && index < SDL_NUM_SCANCODES);
		return keys[index] ? 1.0 : 0.0;
	}
	else if (b <= GlobalInputBinding::MouseButtonEnd) {
		int index = (int)b - (int)GlobalInputBinding::MouseButtonStart;
		assert(index >= 0 && index < 5);
		int dummy{};
		uint32_t state = SDL_GetMouseState(&dummy, &dummy);
		bool down = state & SDL_BUTTON(index + 1);
		return (down) ? 1.0 : 0.0;
	}
	else if (b == GlobalInputBinding::MouseX) {
		return implLocal->mouseXAccum;
	}
	else if (b == GlobalInputBinding::MouseY) {
		return implLocal->mouseYAccum;
	}
	else if (b == GlobalInputBinding::MouseScroll) {
		return implLocal->mouseScrollAccum;
	}
	else if (b <= GlobalInputBinding::ControllerButtonEnd) {
		int index = (int)b - (int)GlobalInputBinding::ControllerButtonStart;
		int state = SDL_GameControllerGetButton(device->sdl_controller_ptr, (SDL_GameControllerButton)index);
		return (state) ? 1.0 : 0.0;
	}
	else if (b <= GlobalInputBinding::ControllerAxisEnd) {
		int index = (int)b - (int)GlobalInputBinding::ControllerAxisStart;
		int16_t state = SDL_GameControllerGetAxis(device->sdl_controller_ptr, (SDL_GameControllerAxis)index);
		return glm::clamp((double)state / INT16_MAX, -1.0, 1.0);
	}
	else
		return 0.0;

}

void GameInputSystem::tick_users(float dt)
{
	SDL_GetRelativeMouseState(&impl->mouseXAccum, &impl->mouseYAccum);

	for (auto u : impl->allUsers)
	{
		auto device = u->get_device();

		if (!device)
			continue;	// no valid device

		ASSERT(device&&device->get_user()==u);
		const auto myType = device->type;

		for (auto& bindAndCallback : u->trackedActions) {
			auto action = bindAndCallback.second.action;
			auto& action_inst = bindAndCallback.second;
			if (!action_inst.is_enabled) 
				continue;

			InputValue device_val = {};
			for (int b = 0; b < action->binds.size(); b++) {
				auto& bind = action->binds[b];
				auto binding = bind.get_bind();
				if (get_device_type_for_keybind(binding) != myType)
					continue;
				
				InputValue raw_val;
				raw_val.v.x = GameInputSystemImpl::sample_device_value_for_binding(binding, device);
				if (bind.modifier)
					raw_val = bind.modifier->modify(raw_val, dt);
				
				if (bind.trigger) {
					int out = (int)bind.trigger->check_trigger(raw_val, dt);
					bool isActive = (out & (int)TriggerMask::Active);
					bool isTriggered = (out & (int)TriggerMask::Triggered);
					if (isTriggered)
						action_inst.on_trigger.invoke();
					if (isActive && !action_inst.is_active)
						action_inst.on_start.invoke();
					if (action_inst.is_active)
						action_inst.active_duration += dt;
					action_inst.is_active = isActive;
					if (!action_inst.is_active)
						action_inst.active_duration = 0.0;
				}


				if (action->is_additive)
					device_val.v += raw_val.v;
				else
					device_val.v = glm::max(device_val.v, raw_val.v);
			}
			action_inst.state = device_val;
		}
	}

	implLocal->mouseScrollAccum = 0;
}