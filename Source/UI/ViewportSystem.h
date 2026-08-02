#pragma once
#include <glm/glm.hpp>
#include "Framework/Rect2d.h"
#include "Framework/Util.h"
union SDL_Event;
class IEditorTool;

// Viewport/input-focus query system: where the scene viewport is on screen, whether it's
// hovered/focused, mouse-capture state, and driving the ImGui pass. Unrelated to 2d drawing
// (see UI/Canvas2d.h for that) -- this is a pure static class, never instantiated, matching
// Canvas2d's pattern (see Canvas2d.h design-doc header comment for why).
class ViewportSystem
{
public:
	// viewport actions
	static bool is_vp_hovered(); // is the scene viewport hovered?
	static bool is_vp_focused(); // is the scene viewport focused for inputs? (obstructed by imgui or gui widgets?)
	static Rect2d get_vp_rect();
	static glm::ivec2 convert_screen_to_vp(glm::ivec2 screen);
	static bool is_drawing_to_screen();

	static bool blocking_mouse_inputs();
	static bool blocking_keyboard_inputs();

	static void set_game_capture_mouse(bool b);
	static bool is_game_capturing_mouse();
	static void set_focus_to_viewport();
	static void pre_events();
	static void handle_event(const SDL_Event& event);
	static void update();
	static void draw_imgui_interfaces(IEditorTool* edState);

private:
	static void draw_imgui_internal(IEditorTool* edState);
};
