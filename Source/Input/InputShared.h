#pragma once
#include <glm/glm.hpp>
#include <SDL2/SDL.h>	// for enums
#include <string>


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

	NumInputs
};
using GIB = GlobalInputBinding;

InputDeviceType get_device_type_for_keybind(GlobalInputBinding bind);
std::string get_button_type_string(GlobalInputBinding bind);
std::string get_device_type_string(InputDeviceType type);