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

	// store these internally this way
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

handle<InputDevice> GameInputSystem::get_keyboard_device_handle()
{
	return impl->keyboardDevice.selfHandle;
}

void GameInputSystem::init()
{
	impl->keyboardDevice.selfHandle = { 0 };
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

const InputDevice* GameInputSystem::get_device(handle<InputDevice> handle)
{
	return impl->get_device(handle);
}
void GameInputSystem::get_connected_devices(std::vector<const InputDevice*>& outDevices)
{
	outDevices.push_back(&impl->keyboardDevice);
	for (int i = 0; i < 4; i++) {
		if (impl->controllerDevices[i].selfHandle.is_valid())
			outDevices.push_back(&impl->controllerDevices[i]);
	}
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
	if (user->assigned_device.is_valid()) {
		auto d = impl->get_device(user->assigned_device);
		assert(d->user == user);
		d->user = nullptr;
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
			assert(impl->controllerDevices[joyindex].devicePtr == nullptr);
			impl->controllerDevices[joyindex].devicePtr = controller;
			impl->controllerDevices[joyindex].selfHandle = { joyindex + 1 };

			sys_print(Debug,"Controller %d added to game system\n",joyindex);

			device_connected.invoke(impl->controllerDevices[joyindex].selfHandle);
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
			if (impl->controllerDevices[myIndex].devicePtr == controller)
				break;
		}
		assert(myIndex != 4);

		sys_print(Debug,"Controller %d removed from game system\n", myIndex);
		auto& device = impl->controllerDevices[myIndex];
		auto wasId = device.selfHandle;
		device_disconnected.invoke(wasId);

		auto user = device.user;
		device.selfHandle = { -1 };
		SDL_GameControllerClose(device.devicePtr);
		device.devicePtr = nullptr;
		device.user = nullptr;

		if (user) {
			sys_print(Debug,"user lost device\n");
			assert(user->assigned_device.id == wasId.id);
			user->assigned_device = { -1 };
			user->on_lost_device.invoke();
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

float sample_device_value_for_binding(GlobalInputBinding b, InputDevice* device)
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
		uint32_t state = SDL_GetMouseState(&dummy,&dummy);
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
		int state = SDL_GameControllerGetButton(device->devicePtr, (SDL_GameControllerButton)index);
		return (state) ? 1.0 : 0.0;
	}
	else if (b <= GlobalInputBinding::ControllerAxisEnd) {
		int index = (int)b - (int)GlobalInputBinding::ControllerAxisStart;
		int16_t state = SDL_GameControllerGetAxis(device->devicePtr, (SDL_GameControllerAxis)index);
		return glm::clamp((double)state / INT16_MAX,-1.0,1.0);
	}

}

void GameInputSystem::tick_users(float dt)
{
	SDL_GetRelativeMouseState(&impl->mouseXAccum, &impl->mouseYAccum);

	for (auto u : impl->allUsers)
	{
		if (!u->assigned_device.is_valid())
			continue;	// no valid device

		auto device = impl->get_device(u->assigned_device);
		assert(device);
		const auto myType = device->type;

		for (auto& bindAndCallback : u->trackedActions) {
			auto action = bindAndCallback.second.action;
			auto& callbacks = bindAndCallback.second;
			if (!callbacks.is_enabled) 
				continue;

			InputValue deviceValue = {};
			for (int b = 0; b < action->binds.size(); b++) {
				auto& bind = action->binds[b];
				auto binding = bind.get_bind();
				if (get_device_type_for_keybind(binding) != myType)
					continue;
				
				InputValue rawValue;
				rawValue.v.x = sample_device_value_for_binding(binding, device);
				if (bind.modifier)
					rawValue = bind.modifier->modify(rawValue, dt);
				
				if (bind.trigger) {
					int out = (int)bind.trigger->check_trigger(rawValue, dt);
					bool isActive = (out & (int)TriggerMask::Active);
					bool isTriggered = (out & (int)TriggerMask::Triggered);
					if (isTriggered && callbacks.triggeredCallback)
						(*callbacks.triggeredCallback)();
					if (isActive && !callbacks.is_active && callbacks.startCallback)
						(*callbacks.startCallback)();
					else if (isActive && callbacks.is_active && callbacks.activeCallback)
						(*callbacks.activeCallback)();
					if (!isActive && callbacks.is_active && callbacks.endCallback)
						(*callbacks.endCallback)();
					if (callbacks.is_active)
						callbacks.active_duration += dt;
					callbacks.is_active = isActive;
					if (!callbacks.is_active)
						callbacks.active_duration = 0.0;
				}


				if (action->is_additive)
					deviceValue.v += rawValue.v;
				else
					deviceValue.v = glm::max(deviceValue.v, rawValue.v);
			}
			callbacks.state = deviceValue;
		}
	}

	implLocal->mouseScrollAccum = 0;
}

void GameInputSystem::set_my_device(InputUser* u, handle<InputDevice> handle)
{
	if (u->assigned_device.id == handle.id)
		return;
	if (u->assigned_device.is_valid()) {
		auto device = impl->get_device(u->assigned_device);
		assert(device->user == u);
		device->user = nullptr;

	}
	if (!handle.is_valid())
		return;

	auto device =impl->get_device(handle);
	if (device->user) {
		sys_print(Warning, "stealing a device from another inputuser\n");
		device->user->assigned_device = { -1 };
		device->user->on_lost_device.invoke();
		device->user = nullptr;
	}
	device->user = u;
	u->assigned_device = device->selfHandle;
	u->on_changed_device.invoke();
}