#pragma once
#include "UIBuilder.h"
#include "Render/RenderWindow.h"
#include "Framework/MeshBuilder.h"
#include "BaseGUI.h"
#include "Scripting/ScriptFunctionCodegen.h"
union SDL_Event;

#include "Framework/LuaColor.h"

class Canvas : public ClassBase {
public:
	CLASS_BODY(Canvas);
	REF static void draw_text(std::string str, int x, int y);
	REF static lRect calc_text_size(std::string str);
	REF static void draw_rect(lRect rect, Texture* texture, lColor color);
	REF static lRect get_window_rect();

	REF static void set_window_fullscreen(bool is_fullscreen);
	REF static void set_window_title(std::string name);
	REF static void set_window_capture_mouse(bool capturing_mouse);
};


class EditorState;
class MaterialInstance;
class UiSystem
{
public:
	static UiSystem* inst;
	RenderWindow window;

	UiSystem();
	~UiSystem();

	// viewport actions
	bool is_vp_hovered() const;	// is the scene viewport hovered?
	bool is_vp_focused() const;	// is the scene viewport focused for inputs? (obstructed by imgui or gui widgets?)
	Rect2d get_vp_rect() const { return viewportRect; }
	glm::ivec2 convert_screen_to_vp(glm::ivec2 screen) const;
	bool is_drawing_to_screen() const;

	bool blocking_mouse_inputs() const;
	bool blocking_keyboard_inputs() const;

	void set_game_capture_mouse(bool b);
	bool is_game_capturing_mouse() const;
	void set_focus_to_viewport();
	void pre_events();
	void handle_event(const SDL_Event& event);
	void update();
	void sync_to_renderer();
	void draw_imgui_interfaces(EditorState* edState);
	const MaterialInstance* get_default_ui_mat() const { return ui_default; }

	const MaterialInstance* ui_default = nullptr;
	const MaterialInstance* fontDefaultMat = nullptr;
	const GuiFont* defaultFont = nullptr;
private:
	void draw_imgui_internal(EditorState* edState);
	bool is_viewport_focused = false;
	bool is_viewport_hovered = false;
	Rect2d viewportRect{};
	bool game_capturing_mouse = false;
	// when game goes into focus mode, the mouse position is saved so it can be reset when exiting focus mode
	int saved_mouse_x = 0, saved_mouse_y = 0;
	// focused= mouse is captured, assumes relative inputs are being taken, otherwise cursor is shown
	bool game_focused = false;
	bool drawing_to_screen = true;

	bool set_focus_to_viewport_next_tick = false;
};