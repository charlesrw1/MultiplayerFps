#pragma once
#include "InputShared.h"
#include "Framework/Handle.h"
#include <array>
#include "Framework/MulticastDelegate.h"
#include <vector>
#include <memory>

class InputUser;
struct InputDevice
{
	handle<InputDevice> selfHandle;
	InputDeviceType type{};	// keyboard or controller
	uint8_t index{};	// controller specific index, 0 indexed, the public facing (1,2,3,4 number)
	SDL_GameController* devicePtr = nullptr;	// internal ptr
	InputUser* user = nullptr;	// most likely 0 or 1, but can share a device too

	bool is_in_use() const {
		return user != nullptr;
	}
};

class GameInputSystemImpl;
class GameInputSystem
{
public:
	static GameInputSystem& get() {
		static GameInputSystem inst;
		return inst;
	}

	// create initial devices
	void init();
	void handle_event(const SDL_Event& event);
	void tick_users(float dt);	// executes any callbacks

	// create a user which can recieve callbacks and have its input data updated
	InputUser* register_input_user(int localPlayerIndex);
	void free_input_user(InputUser*& user);

	 // get the keyboard device, this is created on init. If keyboard input is disabled by config, this will return an invalid handle
	handle<InputDevice> get_keyboard_device_handle();
	// get all connected devices (keyboard + controllers) (dont cache these pointers, use for quick checks)
	void get_connected_devices(std::vector<const InputDevice*>& outDevices);
	// get the internal device ptr
	const InputDevice* get_device(handle<InputDevice> handle);

	// called when an InputUser's device was disconnected
	MulticastDelegate<InputUser*> user_lost_device;
	// called when a device gained/lost connection (controllers)
	MulticastDelegate<handle<InputDevice>> device_connected;
	MulticastDelegate<handle<InputDevice>> device_disconnected;
	// called when a device sends an input, use for detecting stuff (not for input handling!)
	MulticastDelegate<handle<InputDevice>, GlobalInputBinding> on_device_input;
private:
	~GameInputSystem();
	GameInputSystem();
	GameInputSystemImpl* impl = nullptr;
};

inline GameInputSystem& GetGInput() {
	return GameInputSystem::get();
}