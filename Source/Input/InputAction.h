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

CLASS_H(Modifier, ClassBase)
public:
	virtual InputValue modify(InputValue value, float dt) const = 0;
};
CLASS_H(Trigger, ClassBase)
public:
	virtual TriggerMask check_trigger(InputValue value, float dt) const = 0;
};

enum class ActionStateCallback : uint8_t
{
	OnStart,	// action started (like a button being down)
	Active,		// action is between OnStart and OnEnd (like a button being held down, generated each frame)
	Fired,		// action was "triggered" (like a button that pulses while down)
	OnEnd,		// action ended (like a button being released)
};

CLASS_H(InputAction, IAsset)
public:

private:
	struct Binding {
		uint16_t uid = 0; // unique id per input action, to reference for settings
		int8_t forPlayerIndex = -1;	// -1 = default for device, represents a specific binding for a player index. Ie player 1 and 2 can share the keyboard (use forPlayerIndex=1 to set player 2's keyboard bindings)
		GlobalInputBinding defaultBinding{};

		GlobalInputBinding currentBinding{};	// can be modified by settings, otherwise uses default

		std::unique_ptr<Modifier> modifier; // modifies the input value to an output value (optional)
		std::unique_ptr<Trigger> trigger;	// determines if action is started/down/triggered after the hardware was sampled and modifiers applied
	};
	std::vector<Binding> binds;
	bool isAdditive = false;

	friend class GameInputSystem;
};

#include <unordered_map>

struct InputDevice;
class InputUser
{
public:
	void enable(InputAction* a);	// will init tracking of this InputAction for this user
	void disable(InputAction* a);
	// bind_function implicity enables() an action
	void bind_function(InputAction* action, ActionStateCallback type, std::function<void()> func);
	void remove_functions(InputAction* action);

	InputValue get_value(InputAction* a);
	float get_active_duration(InputAction* a);

	void assign_device(handle<InputDevice> device);
	handle<InputDevice> get_device() const { return assigned_device; }
private:
	int playerIndex = 0;	// not the controller index, this is used for player specific bindings
	handle<InputDevice> assigned_device;

	struct ActionInternal {
		bool isEnabled = true;	// if false, callbacks wont be sent

		std::function<void()>* triggeredCallback = nullptr;
		std::function<void()>* startCallback = nullptr;
		std::function<void()>* endCallback = nullptr;
		std::function<void()>* activeCallback = nullptr;

		InputValue state{};
		float activeDuration = 0.0;
		bool isActive = false;
	};

	std::unordered_map<InputAction*, ActionInternal> trackedActions;
	friend class GameInputSystem;
};