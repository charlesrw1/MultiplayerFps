#include "GUISystemPublic.h"
#include "UI/RmlUi/RmlUiSystem.h"
#include "BaseGUI.h"
#include "UIBuilder.h"
#include <algorithm>
#include "Render/MaterialPublic.h"
#include "imgui.h"
#include "Render/IGraphicsDevice.h"
#include "EditorPopups.h"
#include "Framework/MyImguiLib.h"
#include "Assets/AssetBrowser.h"
#include "Assets/AssetSizeViewer.h"
#include "AssetTools/DiagnosticsWindow.h"
#include "Assets/AssetReferenceViewer.h"
#include "GameEnginePublic.h"
#include "Render/DrawPublic.h"
#include "Assets/AssetDatabase.h"
#include "DebugConsole.h"
#include <SDL3/SDL.h>
UiSystem::UiSystem() {
	ui_default = g_assets.find<MaterialInstance>("eng/uiDefault.mm").get();
	if (!ui_default)
		Fatalf("Couldnt find default ui material");
	defaultFont = g_assets.find<GuiFont>("eng/sengo24.fnt");
	if (!defaultFont)
		Fatalf("couldnt load default font");
	fontDefaultMat = g_assets.find<MaterialInstance>("eng/fontDefault.mm");
	if (!fontDefaultMat)
		Fatalf("couldnt load default font material");
}
UiSystem* UiSystem::inst = nullptr;

static void enable_imgui_docking() {
	ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
}
static void disable_imgui_docking() {
	ImGui::GetIO().ConfigFlags &= ~(ImGuiConfigFlags_DockingEnable);
}

extern ConfigVar g_drawdebugmenu;
extern ConfigVar g_drawconsole;
void UiSystem::pre_events() {}
void UiSystem::handle_event(const SDL_Event& event) {}

void UiSystem::draw_imgui_interfaces(IEditorTool* edState) {

	drawing_to_screen = true;
#ifdef EDITOR_BUILD
	drawing_to_screen = !edState;
#endif

	if (g_window_h.was_changed() || g_window_w.was_changed()) {
		SDL_SetWindowSize(eng->get_os_window(), g_window_w.get_integer(), g_window_h.get_integer());
	}

	// draw imgui interfaces
	// if a tool is active, game screen gets drawn to an imgui viewport
	gfx().imgui_new_frame();
	ImGui::NewFrame();
#ifdef EDITOR_BUILD
	if (edState)
		edState->hook_imgui_newframe();
#endif
	draw_imgui_internal(edState);
}
void UiSystem::draw_imgui_internal(IEditorTool* editorState) {
	CPU_SCOPE("imgui_draw");
	ImGui::PushStyleColor(ImGuiCol_WindowBg, color32_to_imvec4({51, 51, 51}));
	ImGui::PushStyleColor(ImGuiCol_FrameBg, color32_to_imvec4({35, 35, 35}));

	EditorPopupManager::inst->draw_popups();

	if (g_drawdebugmenu.get_bool())
		Debug_Interface::get()->draw();

	if (auto* app = eng->get_app())
		app->on_imgui();

#ifdef EDITOR_BUILD
	// draw tool interface if its active
	if (editorState) {
		dock_over_viewport(ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode, editorState);
		editorState->draw_imgui_public();
		AssetBrowser::inst->imgui_draw();
		AssetBrowser::inst->imgui_draw_inspector();
		AssetSizeViewer::get().imgui_draw();
		DiagnosticsWindow::get().imgui_draw();
		AssetReferenceViewer::get().imgui_draw();
		//editorState->drdrawaw_tab_window();
	}
#endif

	// draw after to enable docking
	if (g_drawconsole.get_bool())
		Debug_Console::inst->draw();

#ifdef EDITOR_BUILD

	// will only be true if in a tool state
	if (!is_drawing_to_screen() && eng->get_level()) {

		uint32_t flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoNav;
		if (is_viewport_hovered)
			flags |= ImGuiWindowFlags_NoMove;

		if (editorState && editorState->wants_scene_viewport_menu_bar())
			flags |= ImGuiWindowFlags_MenuBar;

		if (ImGui::Begin("Scene viewport", nullptr, flags)) {

			if (set_focus_to_viewport_next_tick || UiSystem::inst->is_game_capturing_mouse()) {
				ImGui::SetWindowFocus();
				set_focus_to_viewport_next_tick = false;
			}

			if (editorState)
				editorState->hook_pre_scene_viewport_draw();
			// get_current_tool()->hook_pre_scene_viewport_draw();

			// BRUH
			auto size = ImGui::GetWindowSize();
			size.y -= 50;
			size.x -= 30;
			if (size.y <= 0)
				size.y = 1;
			if (size.x <= 0)
				size.x = 1;

			auto pos = ImGui::GetCursorPos();
			auto winpos = ImGui::GetWindowPos();
			ImGui::Image((ImTextureID)uint64_t(idraw->get_composite_output_texture_handle()),
						 ImVec2(size.x, size.y),	  /* magic numbers ;) */
						 ImVec2(0, 1), ImVec2(1, 0)); // this is the scene draw texture
			auto sz = ImGui::GetItemRectSize();
			is_viewport_hovered = ImGui::IsItemHovered();
			is_viewport_focused = ImGui::IsWindowFocused();

			// save off where the viewport is the GUI for mouse events
			viewportRect = Rect2d(pos.x + winpos.x, pos.y + winpos.y, size.x, size.y);
			assert(!is_drawing_to_screen());

			// hook tool for drag and drop stuff
			if (editorState)
				editorState->hook_scene_viewport_draw();
		}
		ImGui::End();
	} else
#endif
	{
		// normal game path, scene view was already drawn the the window framebuffer
		viewportRect = Rect2d(0, 0, g_window_w.get_integer(), g_window_h.get_integer());
		viewportRect.w = std::max((int)viewportRect.w, 1);
		viewportRect.h = std::max((int)viewportRect.h, 1);

		assert(is_drawing_to_screen());
	}

	if (g_drawimguidemo.get_bool())
		ImGui::ShowDemoWindow();

	ImGui::PopStyleColor(2); // framebg

	set_focus_to_viewport_next_tick = false;
}

void UiSystem::update() {
	// reset cursor if in relative mode
	if (is_game_capturing_mouse()) {
		SDL_WarpMouseInWindow(eng->get_os_window(), (float)saved_mouse_x, (float)saved_mouse_y);
	}
	window.clear(); // clear the window
	auto rect = get_vp_rect();
	auto mat = glm::orthoZO(0.f, (float)rect.get_size().x, (float)rect.get_size().y, 0.f, -1.f, 1.f);
	window.view_mat = mat;
	window.reset_verticies();
}

void ui_state_debug() {
	ImGui::Text("is_vp_focused: %d\n", (int)UiSystem::inst->is_vp_focused());
	ImGui::Text("is_vp_hovered: %d\n", (int)UiSystem::inst->is_vp_hovered());
	ImGui::Text("is_game_capturing_mouse: %d\n", (int)UiSystem::inst->is_game_capturing_mouse());
}

ADD_TO_DEBUG_MENU(ui_state_debug);

void UiSystem::sync_to_renderer() {
	RenderWindowBackend::inst->update_window({1}, window);
}
bool UiSystem::is_drawing_to_screen() const {
	return drawing_to_screen;
}
bool UiSystem::is_vp_hovered() const {
	return is_viewport_hovered;
}
bool UiSystem::is_vp_focused() const {
	return is_viewport_focused;
}
glm::ivec2 UiSystem::convert_screen_to_vp(glm::ivec2 screen) const {
	return screen - get_vp_rect().get_pos();
}
void UiSystem::set_game_capture_mouse(bool b) {
	if (b == game_focused)
		return;
	game_focused = b;
	if (game_focused) {
		// reset deltas
		SDL_GetRelativeMouseState(nullptr, nullptr);
		float mx = 0.f, my = 0.f;
		SDL_GetMouseState(&mx, &my);
		saved_mouse_x = (int)mx;
		saved_mouse_y = (int)my;
		SDL_SetWindowRelativeMouseMode(eng->get_os_window(), true);
	} else {
		SDL_SetWindowRelativeMouseMode(eng->get_os_window(), false);
	}
}
bool UiSystem::is_game_capturing_mouse() const {
	return game_focused;
}

void UiSystem::set_focus_to_viewport() {
	set_focus_to_viewport_next_tick = true;
}

bool UiSystem::blocking_keyboard_inputs() const {
	// keep it for text input, not keyboard input. imgui *always* seems to want keyboard capture (for navigation or
	// whatever) which is annoying but i really do want it blocking for text input
	return ImGui::GetIO().WantTextInput || (RmlUiSystem::inst && RmlUiSystem::inst->wants_keyboard_capture());
}

bool UiSystem::blocking_mouse_inputs() const {
	return ImGui::GetIO().WantCaptureMouse || (RmlUiSystem::inst && RmlUiSystem::inst->wants_mouse_capture());
}
