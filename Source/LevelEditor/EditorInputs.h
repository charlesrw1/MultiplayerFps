#pragma once

#include "IInputReciever.h"
#include "UI/GUISystemPublic.h"
enum class MouseSelectionAction
{
	SELECT_ONLY,
	UNSELECT,
	ADD_SELECT,
};

class EditorInputs
{
public:
	bool can_use_mouse_click() { return mouse_click && !focused_item; }
	void eat_mouse_click() {
		//	ASSERT(mouse_click);
		mouse_click = false;
	}
	bool can_use_keyboard() { return keyboard && !focused_item && !UiSystem::inst->blocking_keyboard_inputs(); }
	void eat_keyboard() {
		ASSERT(keyboard);
		keyboard = false;
	}
	void reset_keyboard_and_mouse() { keyboard = mouse_click = true; }
	void set_focus(IInputReciever* recieve) {
		if (focused_item != recieve) {
			printf("set focus: %s\n", (recieve) ? recieve->get_name().c_str() : "<null>");
		}

		focused_item = recieve;
	}
	IInputReciever* get_focused() { return focused_item; }

private:
	IInputReciever* focused_item = nullptr;
	bool keyboard = true;
	bool mouse_click = true;
};