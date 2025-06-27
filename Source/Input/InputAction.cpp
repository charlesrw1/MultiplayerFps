#include "InputSystem.h"
#include "InputAction.h"
#include "Framework/Util.h"

std::string get_device_type_string(InputDeviceType type)
{
	if (type == InputDeviceType::KeyboardMouse)
		return "keyboard";
	else
		return "controller";
}

std::string get_button_type_string(GlobalInputBinding bind)
{
	using GIB = GlobalInputBinding;
	if (bind == GIB::Empty)
		return "";
	else if (bind <= GIB::KeyboardEnd)
	{
		SDL_Scancode code = SDL_Scancode((int)bind - (int)GIB::KeyboardStart);
		return SDL_GetScancodeName(code);
	}
	else if (bind == GIB::MBLeft)
		return "mb1";
	else if (bind == GIB::MBRight)
		return "mb2";
	else if (bind == GIB::MBMiddle)
		return "mb3";
	else if (bind == GIB::MB4)
		return "mb4";
	else if (bind == GIB::MB5)
		return "mb5";
	else if (bind == GIB::MouseX)
		return "mouse_x";
	else if (bind == GIB::MouseY)
		return "mouse_y";
	else if (bind == GIB::MouseScroll)
		return "mouse_scroll";
	else if (bind <= GIB::ControllerButtonEnd)
	{
		SDL_GameControllerButton b = SDL_GameControllerButton((int)bind - (int)GIB::ControllerButtonStart);
		return SDL_GameControllerGetStringForButton(b);
	}
	else if (bind <= GIB::ControllerAxisEnd)
	{
		SDL_GameControllerAxis b = SDL_GameControllerAxis((int)bind - (int)GIB::ControllerAxisStart);
		return SDL_GameControllerGetStringForAxis(b);
	}
	assert(0);
	return "";
}
