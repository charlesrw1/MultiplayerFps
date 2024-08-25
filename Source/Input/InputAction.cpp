#include "InputSystem.h"
#include "InputAction.h"

void InputUser::assign_device(handle<InputDevice> device) {
	GetGInput().set_my_device(this, device);
}