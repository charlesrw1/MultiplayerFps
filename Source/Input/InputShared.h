#pragma once
#include <glm/glm.hpp>
#include <SDL2/SDL.h>	// for enums

struct InputValue
{
	glm::vec2 v{};
	template<typename T>
	T get_value() const {
		static_assert(0, "needs bool/float/vec2 type");
		return T();
	}
	template<typename T>
	void set_value(T x) {
		static_assert(0, "needs bool/float/vec2 type");
	}

};
template<>
inline float InputValue::get_value<float>() const {
	return v.x;
}
template<>
inline glm::vec2 InputValue::get_value<glm::vec2>() const {
	return v;
}
template<>
inline bool InputValue::get_value<bool>() const {
	return (v.x > 0.5) ? true : false;
}

template<>
inline void InputValue::set_value<float>(float x) {
	v.x = x;
}
template<>
inline void InputValue::set_value<glm::vec2>(glm::vec2 x) {
	v = x;
}
template<>
inline void InputValue::set_value<bool>(bool x) {
	v.x = (float)x;
}

enum class TriggerMask
{
	Active = 1,		// is active (will send start/active/end events)
	Triggered = 2,	// is triggered (will send triggerd event)
};

// device type, controllers also have an index to reprsent the actual device as in InputDevice
enum class InputDeviceType : uint8_t
{
	KeyboardMouse,
	Controller,
};

enum class GlobalInputBinding
{
	Empty,

	KeyboardStart,
	KeyboardEnd = SDL_NUM_SCANCODES - 1,

	MouseButtonStart,
	MBLeft = MouseButtonStart,
	MBRight,
	MBMiddle,
	MB4,
	MB5,
	MouseButtonEnd = MB5,

	MouseX,
	MouseY,
	MouseScroll,

	ControllerButtonStart,
	ControllerButtonEnd = ControllerButtonStart + SDL_CONTROLLER_BUTTON_MAX - 1,

	ControllerAxisStart,
	ControllerAxisEnd = ControllerAxisStart + SDL_CONTROLLER_AXIS_MAX - 1,
};

InputDeviceType get_device_type_for_keybind(GlobalInputBinding bind);