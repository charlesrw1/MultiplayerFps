#pragma once
#pragma once
#include "Framework/ClassBase.h"
#include "Framework/MulticastDelegate.h"
#include "Assets/IAsset.h"
#include "Game/SerializePtrHelpers.h"
#include <glm/glm.hpp>
#include <SDL2/SDL.h>
#include "MiscEditors/DataClass.h"
#include "Framework/InlineVec.h"
#include "InputShared.h"
#include "Framework/ReflectionMacros.h"
#include "Framework/ArrayReflection.h"
#include <unordered_map>

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
	InputAction* add_bind(const std::string& name, GlobalInputBinding binding, InputModifier* modifier, InputTrigger* trigger);

	std::string name_id;
	std::string mapping_id_str;	// for swappable mappings
	int mapping_id_num = 0;

	struct Binding {
		std::string special_name;
		GlobalInputBinding defaultBinding{};
		std::unique_ptr<InputModifier> modifier; // modifies the input value to an output value (optional)
		std::unique_ptr<InputTrigger> trigger;	// determines if action is started/down/triggered after the hardware was sampled and modifiers applied
		GlobalInputBinding transient_user_bind{};

		GlobalInputBinding get_bind() const {
			return transient_user_bind == GlobalInputBinding::Empty ? defaultBinding : transient_user_bind;
		}

		InputDeviceType get_binding_device_type() const{
			return get_device_type_for_keybind(get_bind());
		}
	};
	std::vector<Binding> binds;
	bool is_additive = false;

	static GlobalInputBinding controller_button(SDL_GameControllerButton button)
	{
		return GlobalInputBinding((int)GlobalInputBinding::ControllerButtonStart + (int)button);
	}
	static GlobalInputBinding keyboard_key(SDL_Scancode key)
	{
		return GlobalInputBinding((int)GlobalInputBinding::KeyboardStart + (int)key);
	}
	static GlobalInputBinding controller_axis(SDL_GameControllerAxis axis)
	{
		return GlobalInputBinding((int)GlobalInputBinding::ControllerAxisStart + (int)axis);
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

	void bind_triggered_function(std::function<void()> func) {
		triggeredCallback = std::make_unique<CallbackFunc>(std::move(func));
	}
	void bind_start_function(std::function<void()> func) {
		startCallback = std::make_unique<CallbackFunc>(std::move(func));
	}
	void bind_end_function(std::function<void()> func) {
		endCallback = std::make_unique<CallbackFunc>(std::move(func));
	}
	void bind_active_function(std::function<void()> func) {
		activeCallback = std::make_unique<CallbackFunc>(std::move(func));
	}
	void clear_all_functions() {
		triggeredCallback.reset();
		startCallback.reset();
		endCallback.reset();
		activeCallback.reset();
	}

	float get_active_duration() const {
		return active_duration;
	}

	const InputAction* get_action() const {
		return action;
	}
private:
	using CallbackFunc = std::function<void()>;

	std::unique_ptr<CallbackFunc> triggeredCallback = nullptr;
	std::unique_ptr<CallbackFunc> startCallback = nullptr;
	std::unique_ptr<CallbackFunc> endCallback = nullptr;
	std::unique_ptr<CallbackFunc> activeCallback = nullptr;

	bool is_enabled = false;
	InputValue state{};
	float active_duration = 0.0;
	bool is_active = false;
	InputAction* action = nullptr;

	friend class InputUser;
	friend class GameInputSystem;
};

struct InputDevice;
class InputUser
{
public:
	void enable_mapping(const std::string& mapping_id);
	void disable_mapping(const std::string& mapping_id);

	InputActionInstance* get(const std::string& name);

	void assign_device(handle<InputDevice> device);
	handle<InputDevice> get_device() const { return assigned_device; }
	InputDeviceType get_device_type() const;
	bool has_device() const {
		return assigned_device.is_valid();
	}
	
	MulticastDelegate<> on_lost_device;
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
	handle<InputDevice> assigned_device;
	std::unordered_map<std::string, InputActionInstance> trackedActions;
	uint32_t tracked_mapping_bitmasks = 0;
	uint32_t mapping_enabled_bitmask = 0;

	friend class GameInputSystem;
};