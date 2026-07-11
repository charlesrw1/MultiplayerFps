#pragma once
// Core RmlUi singleton: owns the Rml::Context, the engine-side System/File
// interfaces, document lifecycle, and (editor builds) hot reload. Mirrors
// UiSystem/imgui in shape - constructed after gfx().rmlui_init() installs
// the render interface, ticked from the main loop next to UiSystem/ImGui.
#include "RmlUiSystemInterface.h"
#include <unordered_map>
#include <string>
#include <memory>
union SDL_Event;
#ifdef EDITOR_BUILD
class FileWatcher;
#endif
namespace Rml { class Context; class ElementDocument; }

using RmlDocHandle = int;
constexpr RmlDocHandle RML_INVALID_DOC = 0;

class RmlUiSystem {
public:
	static RmlUiSystem* inst;

	RmlUiSystem();
	~RmlUiSystem();

	void init();
	void shutdown();

	// Called each frame from the main loop's sync-update step: resizes the
	// context on viewport change, polls hot reload, and calls Context::Update().
	// The actual draw call happens later, inside Renderer::scene_draw_internal
	// (Source/Render/DrawLocal_SceneDrawInternal.cpp) via gfx().rmlui_render(),
	// which pulls this context's geometry through g_rmlui_render_contexts
	// (RmlUiRenderHook.h) - not a method on this class, so it can draw into
	// whatever composite render target the renderer currently has bound.
	void update();

	// Backend-agnostic SDL event translation -> Rml::Context input calls.
	void handle_event(const SDL_Event& event);

	// Mirrors ImGui's WantCaptureMouse/WantTextInput: true when the mouse is
	// over an interactive RmlUi element / a text-editable element has focus,
	// so game code (UiSystem::blocking_mouse_inputs/blocking_keyboard_inputs)
	// knows to withhold that input from gameplay.
	bool wants_mouse_capture() const;
	bool wants_keyboard_capture() const;

	RmlDocHandle load_document(const std::string& path);
	void show_document(RmlDocHandle handle);
	void hide_document(RmlDocHandle handle);
	void close_document(RmlDocHandle handle);

	Rml::Context* get_context() { return context; }
	Rml::ElementDocument* get_document(RmlDocHandle handle) { return find_doc(handle); }

private:
	Rml::ElementDocument* find_doc(RmlDocHandle handle);
	void reload_document(RmlDocHandle handle);

	RmlUiSystemInterface system_interface;
	RmlUiFileInterface file_interface;
	Rml::Context* context = nullptr;

	RmlDocHandle next_handle = 1;
	std::unordered_map<RmlDocHandle, Rml::ElementDocument*> documents;
	std::unordered_map<RmlDocHandle, bool> document_was_visible; // for hot-reload restore
	std::unordered_map<RmlDocHandle, std::string> document_paths;

	int last_vp_w = 0, last_vp_h = 0;

	// Tracks whether SDL_StartTextInput() is currently active for RmlUi, so
	// update() can start/stop it on wants_keyboard_capture() transitions -
	// SDL only dispatches SDL_EVENT_TEXT_INPUT (character composition) while
	// text input mode is active; ProcessKeyDown alone (always fed) covers
	// non-character keys like backspace/arrows/shift-select but never
	// produces typed characters.
	bool text_input_active = false;

#ifdef EDITOR_BUILD
	std::unique_ptr<FileWatcher> file_watcher;
	void poll_hot_reload();
#endif
};
