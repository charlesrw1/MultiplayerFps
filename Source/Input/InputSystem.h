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
	int index{};	// controller specific index, 0 indexed, the public facing (1,2,3,4 number)
	SDL_GameController* devicePtr = nullptr;	// internal ptr
	InputUser* user = nullptr;

	bool is_in_use() const {
		return user != nullptr;
	}
};

class InputAction;
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
	// InputUsers must call assign_device() with a handle to start recieving input
	InputUser* register_input_user(int localPlayerIndex);
	void free_input_user(InputUser*& user);

	InputAction* register_input_action(std::unique_ptr<InputAction> action);
	GlobalInputBinding find_bind_for_string(const std::string& key_str);

	 // get the keyboard device, this is created on init. If keyboard input is disabled by config, this will return an invalid handle
	handle<InputDevice> get_keyboard_device_handle();
	// get all connected devices (keyboard + controllers) (dont cache these pointers, use for quick checks)
	void get_connected_devices(std::vector<const InputDevice*>& outDevices);
	// get the internal device ptr
	const InputDevice* get_device(handle<InputDevice> handle);

	// trigger on_device_changed, if the InputDevice is being used by another user, then it will log a wanring and issue on_device_changed, but continue. 
	void set_my_device(InputUser* user, handle<InputDevice> handle);

	void set_input_mapping_status(InputUser* user, const std::string& map_id, bool enable);

	// called when a device gained/lost connection (controllers)
	// if a device was owned by an InputUser, *their* on_device_lost will also be fired
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