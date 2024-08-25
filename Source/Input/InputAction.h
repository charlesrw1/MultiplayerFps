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

template<typename T>
inline PropertyInfo make_unique_ptr_prop(const char* name, uint16_t offset, std::unique_ptr<T>* dummy, int flags)
{
	PropertyInfo pi;
	pi.name = name;
	pi.offset = offset;
	pi.type = core_type_id::StdUniquePtr;
	pi.range_hint = T::StaticType.classname;
	pi.flags = flags;
	return pi;
}
#define REG_UNIQUE_PTR(name, flags) \
	make_unique_ptr_prop(#name,offsetof(TYPE_FROM_START, name), &((TYPE_FROM_START*)0)->name, flags)

CLASS_H(InputAction, ClassBase)
public:


	struct Binding {
		uint16_t uid = 0; // unique id per input action, to reference for settings
		GlobalInputBinding defaultBinding{};
		GlobalInputBinding currentBinding{};	// can be modified by settings, otherwise uses default
		std::unique_ptr<Modifier> modifier; // modifies the input value to an output value (optional)
		std::unique_ptr<Trigger> trigger;	// determines if action is started/down/triggered after the hardware was sampled and modifiers applied
		
		static const PropertyInfoList* get_props() {
			START_PROPS(Binding)
				REG_UNIQUE_PTR(modifier,PROP_DEFAULT),
				REG_UNIQUE_PTR(trigger,PROP_DEFAULT)
			END_PROPS(Binding)
		}

	};
	std::vector<Binding> binds;
	bool isAdditive = false;

	static const PropertyInfoList* get_props() {
		MAKE_VECTORCALLBACK(Binding, binds);
		START_PROPS(InputAction)
			REG_STDVECTOR(binds,PROP_DEFAULT),
			REG_BOOL(isAdditive,PROP_DEFAULT,"0")
		END_PROPS(InputAction)
	}

	friend class GameInputSystem;
};

#include <unordered_map>

struct InputDevice;
class InputUser
{
public:
	void enable(InputAction* a) { 	// will init tracking of this InputAction for this user
		find_internal_action(a)->isEnabled = true;
	}
	void disable(InputAction* a) {
		find_internal_action(a)->isEnabled = false;
	}
	// bind_function implicity enables() an action
	void bind_function(InputAction* action, ActionStateCallback type, std::function<void()> func) {
		auto internal = find_internal_action(action);
		std::unique_ptr<std::function<void()>>* wantPtr = nullptr;
		if (type == ActionStateCallback::OnStart) {
			wantPtr = &internal->startCallback;
		}
		else if (type == ActionStateCallback::Active) {
			wantPtr = &internal->activeCallback;
		}
		else if (type == ActionStateCallback::Fired) {
			wantPtr = &internal->triggeredCallback;
		}
		else {	// on end
			wantPtr = &internal->endCallback;
		}
		*wantPtr = std::make_unique< std::function<void()>>(std::move(func));
	}
	void remove_functions(InputAction* action) {
		auto internal = find_internal_action(action);
		internal->startCallback.reset();
		internal->activeCallback.reset();
		internal->triggeredCallback.reset();
		internal->endCallback.reset();
	}

	template<typename T>
	T get_value(InputAction* a) {
		auto internal = find_internal_action(a);
		return internal->state.get_value<T>();
	}
	float get_active_duration(InputAction* a) {
		auto internal = find_internal_action(a);
		return internal->activeDuration;
	}

	void assign_device(handle<InputDevice> device);

	handle<InputDevice> get_device() const { return assigned_device; }


private:
	int playerIndex = 0;	// not the controller index, this is used for player specific bindings
	handle<InputDevice> assigned_device;

	struct ActionInternal {
		bool isEnabled = true;	// if false, callbacks wont be sent

		std::unique_ptr<std::function<void()>> triggeredCallback = nullptr;
		std::unique_ptr <std::function<void()>> startCallback = nullptr;
		std::unique_ptr <std::function<void()>> endCallback = nullptr;
		std::unique_ptr <std::function<void()>> activeCallback = nullptr;

		InputValue state{};
		float activeDuration = 0.0;
		bool isActive = false;
	};


	ActionInternal* find_internal_action(InputAction* i) {
		return &trackedActions[i];
	}


	std::unordered_map<InputAction*, ActionInternal> trackedActions;
	friend class GameInputSystem;
};