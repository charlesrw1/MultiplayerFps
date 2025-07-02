#include "GUISystemPublic.h"
#include "Widgets/Layouts.h"
#include "UIBuilder.h"
#include <algorithm>
#include "Render/MaterialPublic.h"
#include "EngineEditorState.h"
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include "EditorPopups.h"
#include "Framework/MyImguiLib.h"
#include "Assets/AssetBrowser.h"
#include "GameEnginePublic.h"
#include "Render/DrawPublic.h"
#include "Assets/AssetDatabase.h"
#include "DebugConsole.h"
#include <SDL2/SDL.h>
UiSystem::UiSystem()  {
	ui_default = g_assets.find_global_sync<MaterialInstance>("eng/uiDefault.mm").get();
	if (!ui_default)
		Fatalf("Couldnt find default ui material");
	defaultFont = g_assets.find_global_sync<GuiFont>("eng/sengo24.fnt");
	if (!defaultFont)
		Fatalf("couldnt load default font");
	fontDefaultMat = g_assets.find_global_sync<MaterialInstance>("eng/fontDefault.mm");
	if (!fontDefaultMat)
		Fatalf("couldnt load default font material");
}
UiSystem* UiSystem::inst = nullptr;

static void enable_imgui_docking()
{
	ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
}
static void disable_imgui_docking()
{
	ImGui::GetIO().ConfigFlags &= ~(ImGuiConfigFlags_DockingEnable);
}


extern ConfigVar g_drawdebugmenu;
extern ConfigVar g_drawconsole;
void UiSystem::pre_events() {}
void UiSystem::handle_event(const SDL_Event& event)
{

}

void UiSystem::draw_imgui_interfaces(EditorState* edState) {

	drawing_to_screen = !edState || !edState->get_tool();

	if (edState && edState->has_tool())
		enable_imgui_docking();
	else
		disable_imgui_docking();

	// draw imgui interfaces
	// if a tool is active, game screen gets drawn to an imgui viewport
	ImGui_ImplSDL2_NewFrame();
	ImGui_ImplOpenGL3_NewFrame();
	ImGui::NewFrame();
#ifdef EDITOR_BUILD
	if (edState)
		edState->imgui_hook_new_frame();
#endif
	draw_imgui_internal(edState);
}
void UiSystem::draw_imgui_internal(EditorState* editorState) {
	CPUSCOPESTART(imgui_draw);
	ImGui::PushStyleColor(ImGuiCol_WindowBg, color32_to_imvec4({ 51, 51, 51 }));
	ImGui::PushStyleColor(ImGuiCol_FrameBg, color32_to_imvec4({ 35, 35, 35 }));

	EditorPopupManager::inst->draw_popups();

	if (g_drawdebugmenu.get_bool())
		Debug_Interface::get()->draw();

#ifdef EDITOR_BUILD
	// draw tool interface if its active
	if (editorState&&editorState->has_tool()) {
		dock_over_viewport(ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode, editorState->get_tool());
		editorState->imgui_draw();
		AssetBrowser::inst->imgui_draw();
		editorState->draw_tab_window();
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

		if (editorState->has_tool() && editorState->wants_scene_viewport_menu_bar())
			flags |= ImGuiWindowFlags_MenuBar;

		if (ImGui::Begin("Scene viewport", nullptr, flags)) {

			if (editorState->has_tool())
				editorState->hook_pre_viewport();
			//get_current_tool()->hook_pre_scene_viewport_draw();

			// BRUH
			auto size = ImGui::GetWindowSize();
			size.y -= 50;
			size.x -= 30;
			if (size.y < 0) size.y = 0;
			if (size.x < 0) size.x = 0;

			auto pos = ImGui::GetCursorPos();
			auto winpos = ImGui::GetWindowPos();
			ImGui::Image((ImTextureID)uint64_t(idraw->get_composite_output_texture_handle()),
				ImVec2(size.x, size.y), /* magic numbers ;) */
				ImVec2(0, 1), ImVec2(1, 0));	// this is the scene draw texture
			auto sz = ImGui::GetItemRectSize();
			is_viewport_hovered = ImGui::IsItemHovered();
			is_viewport_focused = ImGui::IsWindowFocused();

			// save off where the viewport is the GUI for mouse events
			viewportRect = Rect2d(pos.x + winpos.x, pos.y + winpos.y, size.x, size.y);
			assert(!is_drawing_to_screen());
	
			// hook tool for drag and drop stuff
			if (editorState->has_tool())
				editorState->hook_viewport();
		}
		ImGui::End();
	}
	else
#endif
	{
		// normal game path, scene view was already drawn the the window framebuffer
		viewportRect = Rect2d(0, 0, g_window_w.get_integer(), g_window_h.get_integer());
		assert(is_drawing_to_screen());
	}

	if (g_drawimguidemo.get_bool())
		ImGui::ShowDemoWindow();

	ImGui::PopStyleColor(2);//framebg
}

void UiSystem::update() {
	// reset cursor if in relative mode
	if (is_game_capturing_mouse()) {
		SDL_WarpMouseInWindow(eng->get_os_window(), saved_mouse_x, saved_mouse_y);
	}
	window.clear();	// clear the window
	auto rect = get_vp_rect();
	auto mat = glm::orthoZO(0.f, (float)rect.get_size().x, (float)rect.get_size().y, 0.f, -1.f, 1.f);
	window.view_mat = mat;
	window.reset_verticies();
}

void UiSystem::sync_to_renderer() {
	RenderWindowBackend::inst->update_window({ 1 }, window);
}
bool UiSystem::is_drawing_to_screen() const
{
	return drawing_to_screen;
}
bool UiSystem::is_vp_hovered() const
{
	return is_viewport_hovered;
}
bool UiSystem::is_vp_focused() const
{
	return is_viewport_focused;
}
glm::ivec2 UiSystem::convert_screen_to_vp(glm::ivec2 screen) const
{
	return screen - get_vp_rect().get_pos();
}
void UiSystem::set_game_capture_mouse(bool b) {
	if (b == game_focused)
		return;
	if (game_focused) {
		// reset deltas
		SDL_GetRelativeMouseState(nullptr, nullptr);
		SDL_GetMouseState(&saved_mouse_x, &saved_mouse_y);
		SDL_SetRelativeMouseMode(SDL_TRUE);
	}
	else {
		SDL_SetRelativeMouseMode(SDL_FALSE);
	}
	game_focused = b;
}
bool UiSystem::is_game_capturing_mouse() const {
	return game_focused;
}


bool UiSystem::blocking_keyboard_inputs() const
{
	return ImGui::GetIO().WantCaptureKeyboard;
}

bool UiSystem::blocking_mouse_inputs() const
{
	return ImGui::GetIO().WantCaptureMouse;
}


