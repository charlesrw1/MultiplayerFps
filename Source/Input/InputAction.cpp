#include "InputSystem.h"
#include "InputAction.h"

void InputUser::assign_device(handle<InputDevice> device) {
	GetGInput().set_my_device(this, device);
}
InputAction* InputAction::register_action(const std::string& binding_group, const std::string& action_name, bool is_additive)
{
	InputAction* a = new InputAction;
	a->mapping_id_str = binding_group;
	a->name_id = action_name;
	a->is_additive = is_additive;
	return GameInputSystem::get().register_input_action(std::unique_ptr<InputAction>(a));
}
InputAction* InputAction::add_bind(const std::string& name, GlobalInputBinding binding, InputModifier* modifier, InputTrigger* trigger)
{
	Binding b;
	b.special_name = name;
	b.modifier = std::unique_ptr<InputModifier>(modifier);
	b.trigger = std::unique_ptr<InputTrigger>(trigger);
	b.defaultBinding = binding;
	binds.push_back(std::move(b));
	return this;
}

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


std::string InputAction::get_action_bind_path(const Binding* b) const
{
	assert(b);
	assert(b->transient_user_bind != GlobalInputBinding::Empty);
	assert(!mapping_id_str.empty());
	assert(!name_id.empty());
	
	std::string out;
	out.reserve(64);
	out += get_action_name();
	out += '/';
	out += get_device_type_string(b->get_binding_device_type());
	if (!b->special_name.empty())
	{
		out += '/';
		out += b->special_name;
	}

	out += get_button_type_string(b->get_bind());

	return out;
}
InputDeviceType InputUser::get_device_type() const
{
	assert(assigned_device.is_valid());
	auto device = GetGInput().get_device(assigned_device);
	assert(device);
	return device->type;
}

InputActionInstance* InputUser::get(const std::string& name)
{
	auto find = trackedActions.find(name);
	return find == trackedActions.end() ? nullptr : &find->second;
}

void InputUser::enable_mapping(const std::string& mapping_id)
{
	GetGInput().set_input_mapping_status(this, mapping_id, true);
}
void InputUser::disable_mapping(const std::string& mapping_id)
{
	GetGInput().set_input_mapping_status(this, mapping_id, false);
}
