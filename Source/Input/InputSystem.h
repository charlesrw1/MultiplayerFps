#pragma once
#include "InputShared.h"
#include "Framework/Handle.h"
#include "Framework/MulticastDelegate.h"
#include <vector>
#include <memory>
#include "Animation/Editor/Optional.h"

#include "glm/glm.hpp"
using glm::ivec2;
using glm::vec2;
using std::vector;

// input wrapper over sdl with controller support
class Input {
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
	static bool is_con_button_down(SDL_GameControllerButton button);
	static bool was_con_button_pressed(SDL_GameControllerButton button);
	static bool was_con_button_released(SDL_GameControllerButton button);
	static double get_con_axis(SDL_GameControllerAxis axis);
	// uses specified controller
	static bool is_con_button_down_idx(SDL_GameControllerButton b, int idx);
	static bool was_con_button_pressed_idx(SDL_GameControllerButton b, int idx);
	static bool was_con_button_released_idx(SDL_GameControllerButton b, int idx);
	static double get_con_axis_idx(SDL_GameControllerAxis a, int idx);
	static bool is_any_con_active();
	static int get_num_active_cons();
	static bool is_con_active(int idx);
	static SDL_GameControllerType get_con_type();
	static SDL_GameControllerType get_con_type_idx(int idx);
	static bool last_recieved_input_from_con();
	static MulticastDelegate<int /* index */, bool/* connected/disconnected */> on_con_status;
	static MulticastDelegate<int /* controller index or -1 if keyboard */> on_any_input; // invoked on any input recieved from a device
private:

	struct PressReleaseState {
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

	struct Device {
		Device(SDL_GameController* ptr, int index);
		int index = -1;
		SDL_GameController* ptr = nullptr;
		vector<PressReleaseState> buttonState;
	};
	vector<Device> devices;
	opt<int> default_dev_index;
	opt<int> recieved_input_from_this;

	int mouseScrollAcum = 0;
	int mouseXAccum = 0;
	int mouseYAccum = 0;
	int mouseX = 0;
	int mouseY = 0;

	const Uint8* keyState = nullptr;
	vector<PressReleaseState> keyPressedReleasedState;	// 0 = not down, 1 = down, 2 = not down and released, 3 = down and pressed,
	vector<PressReleaseState> mouseButtonsState;

	opt<int> find_device_for_index(int idx) const;
	opt<int> find_device_for_ptr(SDL_GameController* ptr) const;
	SDL_GameController* get_device_ptr(int idx) const;
};