#include "InputSystem.h"
#include "Framework/Hashset.h"

#include "Framework/Config.h"
#include "InputAction.h"

CLASS_IMPL(Modifier);
CLASS_IMPL(Trigger);
CLASS_IMPL(InputAction);

class GameInputSystemImpl
{
public:
	GameInputSystemImpl() : allUsers(2) {}

	hash_set<InputUser> allUsers;

	// store these internally this way
	InputDevice keyboardDevice;
	InputDevice controllerDevices[4];
};

GameInputSystem::GameInputSystem() {
	impl = new GameInputSystemImpl;
}
GameInputSystem::~GameInputSystem() {
	delete impl;
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
	sys_print("``` %d existing controllers connected\n", numjoysticks);

}

const InputDevice* GameInputSystem::get_device(handle<InputDevice> handle)
{
	if (!handle.is_valid()) 
		return nullptr;
	if (handle.id == 0)
		return &impl->keyboardDevice;
	if (handle.id >= 1 && handle.id <= 4)
		return &impl->controllerDevices[handle.id - 1];
	return nullptr;
}
void GameInputSystem::get_connected_devices(std::vector<const InputDevice*>& outDevices)
{
	outDevices.push_back(&impl->keyboardDevice);
	for (int i = 0; i < 4; i++) {
		if (impl->controllerDevices[i].selfHandle.is_valid())
			outDevices.push_back(&impl->controllerDevices[i]);
	}
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
		auto d = (InputDevice*)get_device(user->assigned_device);
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
			sys_print("SDL_CONTROLLERDEVICEADDED: device index is greater than 3\n");
		}
		else {
			auto controller = SDL_GameControllerOpen(joyindex);
			assert(impl->controllerDevices[joyindex].devicePtr == nullptr);
			impl->controllerDevices[joyindex].devicePtr = controller;
			impl->controllerDevices[joyindex].selfHandle = { joyindex + 1 };

			sys_print("Controller %d added to game system\n",joyindex);

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

		sys_print("Controller %d removed from game system\n", myIndex);
		auto& device = impl->controllerDevices[myIndex];
		auto wasId = device.selfHandle;
		device_disconnected.invoke(wasId);

		auto user = device.user;
		device.selfHandle = { -1 };
		SDL_GameControllerClose(device.devicePtr);
		device.devicePtr = nullptr;
		device.user = nullptr;

		if (user) {
			sys_print("*** user lost device\n");
			assert(user->assigned_device.id == wasId.id);
			device.user->assigned_device = { -1 };
			user_lost_device.invoke(user);
		}
	}
		break;
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
		return 0.0;
	}
	else if (b == GlobalInputBinding::MouseY) {
		return 0.0;
	}
	else if (b == GlobalInputBinding::MouseScroll) {
		return 0.0;
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
	for (auto u : impl->allUsers)
	{
		if (!u->assigned_device.is_valid())
			continue;	// no valid device

		auto device = (InputDevice*)get_device(u->assigned_device);
		assert(device);
		const auto myType = device->type;

		for (auto& bindAndCallback : u->trackedActions) {
			auto action = bindAndCallback.first;
			auto& callbacks = bindAndCallback.second;
			if (!callbacks.isEnabled) continue;

			InputValue deviceValue = {};
			for (int b = 0; b < action->binds.size(); b++) {
				auto& bind = action->binds[b];
				auto binding = (bind.currentBinding == GlobalInputBinding::Empty) ? bind.defaultBinding : bind.currentBinding;
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
					if (isActive && !callbacks.isActive && callbacks.startCallback)
						(*callbacks.startCallback)();
					else if (isActive && callbacks.isActive && callbacks.activeCallback)
						(*callbacks.activeCallback)();
					if (!isActive && callbacks.isActive && callbacks.endCallback)
						(*callbacks.endCallback)();
					if (callbacks.isActive)
						callbacks.activeDuration += dt;
					callbacks.isActive = isActive;
					if (!callbacks.isActive)
						callbacks.activeDuration = 0.0;
				}


				if (action->isAdditive)
					deviceValue.v += rawValue.v;
				else
					deviceValue.v = rawValue.v;
			}
			callbacks.state = deviceValue;
		}
	}
}

void GameInputSystem::set_my_device(InputUser* u, handle<InputDevice> handle)
{
	if (u->assigned_device.id == handle.id)
		return;
	if (u->assigned_device.is_valid()) {
		auto device = (InputDevice*)get_device(u->assigned_device);
		assert(device->user == u);
		device->user = nullptr;

	}
	if (!handle.is_valid())
		return;

	auto device = (InputDevice*)get_device(handle);
	if (device->user) {
		sys_print("??? stealing a device from another inputuser\n");
		device->user->assigned_device = { -1 };
		device->user = nullptr;
	}
	device->user = u;
	u->assigned_device = device->selfHandle;
}