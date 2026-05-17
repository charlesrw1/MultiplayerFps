#pragma once

#include "Framework/MulticastDelegate.h"
#include <vector>
#include <memory>
#include "Framework/Optional.h"
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_scancode.h>
#include "Input/Sdl2CompatGamepad.h"
#include "glm/glm.hpp"
using glm::ivec2;
using glm::vec2;
using std::vector;

// input wrapper over sdl with controller support
class Input
{
public:
	Input();
	~Input();
	void pre_events();
	void handle_event(const SDL_Event& ev);
	void tick();

	static Input* inst;
	static bool is_key_down(SDL_Scancode key);
	static bool was_key_pressed(SDL_Scancode key);
	static bool was_key_released(SDL_Scancode key);
	static ivec2 get_mouse_delta();
	static ivec2 get_mouse_pos();
	static bool is_mouse_down(int button);
	static bool is_mouse_double_clicked(int button);
	static bool was_mouse_pressed(int button);
	static bool was_mouse_released(int button);
	static int get_mouse_scroll();
	static bool is_shift_down();
	static bool is_ctrl_down();
	static bool is_alt_down();

	// uses first controller
	static bool is_con_button_down(SDL_GamepadButton button);
	static bool was_con_button_pressed(SDL_GamepadButton button);
	static bool was_con_button_released(SDL_GamepadButton button);
	static double get_con_axis(SDL_GamepadAxis axis);
	// uses specified controller (idx = SDL_JoystickID instance id)
	static bool is_con_button_down_idx(SDL_GamepadButton b, int idx);
	static bool was_con_button_pressed_idx(SDL_GamepadButton b, int idx);
	static bool was_con_button_released_idx(SDL_GamepadButton b, int idx);
	static double get_con_axis_idx(SDL_GamepadAxis a, int idx);
	static bool is_any_con_active();
	static int get_num_active_cons();
	static bool is_con_active(int idx);
	static SDL_GamepadType get_con_type();
	static SDL_GamepadType get_con_type_idx(int idx);
	static bool last_recieved_input_from_con();
	static void rumble(uint16_t low_freq, uint16_t high_freq, uint32_t duration_ms);
	static MulticastDelegate<int /* index */, bool /* connected/disconnected */> on_con_status;
	static MulticastDelegate<int /* controller index or -1 if keyboard */>
		on_any_input; // invoked on any input recieved from a device
private:
	struct PressReleaseState
	{
		PressReleaseState() {
			is_down = false;
			is_pressed = false;
			is_released = false;
		}
		bool is_down : 1;
		bool is_pressed : 1;
		bool is_released : 1;
	};
	static_assert(sizeof(PressReleaseState) == 1, "");

	struct Device
	{
		Device(SDL_Gamepad* ptr, int index);
		int index = -1; // SDL_JoystickID instance id in SDL3
		SDL_Gamepad* ptr = nullptr;
		vector<PressReleaseState> buttonState;
	};
	vector<Device> devices;
	opt<int> default_dev_index; // instance id of default gamepad
	opt<int> recieved_input_from_this;

	int mouseScrollAcum = 0;
	int mouseXAccum = 0;
	int mouseYAccum = 0;
	int mouseX = 0;
	int mouseY = 0;

	const bool* keyState = nullptr; // SDL3: SDL_GetKeyboardState returns const bool*
	vector<PressReleaseState>
		keyPressedReleasedState; // 0 = not down, 1 = down, 2 = not down and released, 3 = down and pressed,
	vector<PressReleaseState> mouseButtonsState;

	opt<int> find_device_for_index(int idx) const;
	opt<int> find_device_for_ptr(SDL_Gamepad* ptr) const;
	SDL_Gamepad* get_device_ptr(int idx) const;
};
