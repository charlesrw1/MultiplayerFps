#pragma once
#include "Framework/MulticastDelegate.h"
#include <glm/glm.hpp>
#include <SDL2/SDL.h>
#include "InputShared.h"
#include <unordered_map>
#include <memory>
#include <vector>

class InputModifier
{
public:
	virtual InputValue modify(InputValue value, float dt) const = 0;
};
class InputTrigger
{
public:
	virtual TriggerMask check_trigger(InputValue value, float dt) const = 0;
};

class InputAction
{
public:
	static InputAction* register_action(const std::string& binding_group, const std::string& action_name, bool is_additive = false);
	InputAction* add_bind(const std::string& name, GIB binding, InputModifier* modifier, InputTrigger* trigger);

	std::string name_id;
	std::string mapping_id_str;	// for swappable mappings

	int mapping_id_num = 0;

	struct Binding {
		std::string special_name;

		std::unique_ptr<InputModifier> modifier; // modifies the input value to an output value (optional)
		std::unique_ptr<InputTrigger> trigger;	// determines if action is started/down/triggered after the hardware was sampled and modifiers applied
		
		GIB default_binding{};
		GIB transient_user_bind{};

		GIB get_bind() const {
			return transient_user_bind == GIB::Empty ? default_binding : transient_user_bind;
		}

		InputDeviceType get_binding_device_type() const{
			return get_device_type_for_keybind(get_bind());
		}
	};
	std::vector<Binding> binds;
	bool is_additive = false;

	static GIB controller_button(SDL_GameControllerButton button)
	{
		return GIB((int)GIB::ControllerButtonStart + (int)button);
	}
	static GIB keyboard_key(SDL_Scancode key)
	{
		return GIB((int)GIB::KeyboardStart + (int)key);
	}
	static GIB controller_axis(SDL_GameControllerAxis axis)
	{
		return GIB((int)GIB::ControllerAxisStart + (int)axis);
	}
	
	std::string get_action_bind_path(const Binding* b) const;

	std::string get_action_name() const {
		return mapping_id_str + "/" + name_id;
	}

	friend class GameInputSystem;
};


class InputActionInstance
{
public:
	template<typename T>
	T get_value() const {
		return state.get_value<T>();
	}
	float get_active_duration() const {
		return active_duration;
	}
	const InputAction* get_action() const {
		return action;
	}

	MulticastDelegate<> on_start;
	MulticastDelegate<> on_trigger;
private:

	bool is_enabled = false;
	InputValue state{};
	float active_duration = 0.0;
	bool is_active = false;
	InputAction* action = nullptr;

	friend class InputUser;
	friend class GameInputSystem;
};

class InputDevice;
class InputUser
{
public:
	void destroy();

	void enable_mapping(const std::string& mapping_id);
	void disable_mapping(const std::string& mapping_id);

	InputActionInstance* get(const std::string& name);

	void assign_device(InputDevice* device);
	bool has_device() const { return assigned_device != nullptr; }
	InputDevice* get_device() const { return assigned_device; }
	
	MulticastDelegate<> on_changed_device;

private:
	bool has_mapping_tracked(int index) const {
		return tracked_mapping_bitmasks & (1ul << index);
	}
	bool has_mapping_enabled(int index) const {
		assert(has_mapping_tracked(index));
		return mapping_enabled_bitmask & (1ul << index);
	}

	int playerIndex = 0;	// not the controller index, this is used for player specific bindings
	InputDevice* assigned_device = nullptr;
	std::unordered_map<std::string, InputActionInstance> trackedActions;
	uint32_t tracked_mapping_bitmasks = 0;
	uint32_t mapping_enabled_bitmask = 0;

	friend class GameInputSystem;
};