#include "RmlUiSystem.h"
#include "RmlUiRenderHook.h"
#include "RmlUiLua.h"
#include "UI/GUISystemPublic.h"
#include "Framework/Log.h"
#include <cstring>
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/Input.h>
#include <SDL3/SDL.h>
#ifdef EDITOR_BUILD
#include "Assets/FileWatcher.h"
#endif

RmlUiSystem* RmlUiSystem::inst = nullptr;
std::function<void()> g_rmlui_render_contexts;

namespace {
int translate_sdl_mods(SDL_Keymod mod) {
	int out = 0;
	if (mod & SDL_KMOD_CTRL) out |= Rml::Input::KM_CTRL;
	if (mod & SDL_KMOD_SHIFT) out |= Rml::Input::KM_SHIFT;
	if (mod & SDL_KMOD_ALT) out |= Rml::Input::KM_ALT;
	if (mod & SDL_KMOD_GUI) out |= Rml::Input::KM_META;
	if (mod & SDL_KMOD_CAPS) out |= Rml::Input::KM_CAPSLOCK;
	if (mod & SDL_KMOD_NUM) out |= Rml::Input::KM_NUMLOCK;
	return out;
}

Rml::Input::KeyIdentifier translate_sdl_key(SDL_Keycode key) {
	using namespace Rml::Input;
	switch (key) {
	case SDLK_A: return KI_A; case SDLK_B: return KI_B; case SDLK_C: return KI_C;
	case SDLK_D: return KI_D; case SDLK_E: return KI_E; case SDLK_F: return KI_F;
	case SDLK_G: return KI_G; case SDLK_H: return KI_H; case SDLK_I: return KI_I;
	case SDLK_J: return KI_J; case SDLK_K: return KI_K; case SDLK_L: return KI_L;
	case SDLK_M: return KI_M; case SDLK_N: return KI_N; case SDLK_O: return KI_O;
	case SDLK_P: return KI_P; case SDLK_Q: return KI_Q; case SDLK_R: return KI_R;
	case SDLK_S: return KI_S; case SDLK_T: return KI_T; case SDLK_U: return KI_U;
	case SDLK_V: return KI_V; case SDLK_W: return KI_W; case SDLK_X: return KI_X;
	case SDLK_Y: return KI_Y; case SDLK_Z: return KI_Z;
	case SDLK_0: return KI_0; case SDLK_1: return KI_1; case SDLK_2: return KI_2;
	case SDLK_3: return KI_3; case SDLK_4: return KI_4; case SDLK_5: return KI_5;
	case SDLK_6: return KI_6; case SDLK_7: return KI_7; case SDLK_8: return KI_8;
	case SDLK_9: return KI_9;
	case SDLK_SPACE: return KI_SPACE;
	case SDLK_RETURN: return KI_RETURN;
	case SDLK_BACKSPACE: return KI_BACK;
	case SDLK_TAB: return KI_TAB;
	case SDLK_ESCAPE: return KI_ESCAPE;
	case SDLK_DELETE: return KI_DELETE;
	case SDLK_INSERT: return KI_INSERT;
	case SDLK_HOME: return KI_HOME;
	case SDLK_END: return KI_END;
	case SDLK_PAGEUP: return KI_PRIOR;
	case SDLK_PAGEDOWN: return KI_NEXT;
	case SDLK_LEFT: return KI_LEFT;
	case SDLK_RIGHT: return KI_RIGHT;
	case SDLK_UP: return KI_UP;
	case SDLK_DOWN: return KI_DOWN;
	case SDLK_LSHIFT: case SDLK_RSHIFT: return KI_LSHIFT;
	case SDLK_LCTRL: case SDLK_RCTRL: return KI_LCONTROL;
	case SDLK_LALT: case SDLK_RALT: return KI_LMENU;
	case SDLK_F1: return KI_F1; case SDLK_F2: return KI_F2; case SDLK_F3: return KI_F3;
	case SDLK_F4: return KI_F4; case SDLK_F5: return KI_F5; case SDLK_F6: return KI_F6;
	case SDLK_F7: return KI_F7; case SDLK_F8: return KI_F8; case SDLK_F9: return KI_F9;
	case SDLK_F10: return KI_F10; case SDLK_F11: return KI_F11; case SDLK_F12: return KI_F12;
	default: return KI_UNKNOWN;
	}
}
} // namespace

RmlUiSystem::RmlUiSystem() {}
RmlUiSystem::~RmlUiSystem() {}

void RmlUiSystem::init() {
	ASSERT(!context);
	Rml::SetSystemInterface(&system_interface);
	Rml::SetFileInterface(&file_interface);
	const bool ok = Rml::Initialise();
	ASSERT(ok && "Rml::Initialise failed");

	Rect2d vp = UiSystem::inst->get_vp_rect();
	last_vp_w = vp.w > 0 ? vp.w : 1;
	last_vp_h = vp.h > 0 ? vp.h : 1;
	context = Rml::CreateContext("main", Rml::Vector2i(last_vp_w, last_vp_h));
	ASSERT(context && "Rml::CreateContext failed");

	g_rmlui_render_contexts = [this]() {
		if (context)
			context->Render();
	};

#ifdef EDITOR_BUILD
	file_watcher = std::make_unique<FileWatcher>();
	file_watcher->init(FileSys::get_full_path_from_game_path("ui"));
#endif

	// RmlUi's (freetype) font engine needs real .ttf/.otf files - the
	// engine's own GuiFont bitmap-font format (Data/eng/fonts/*.fnt) used by
	// Gui::/Canvas:: is a different, incompatible format. Load every
	// .ttf/.otf under Data/ui/fonts/ if any exist; if none are present yet,
	// RmlUi documents still lay out but render no glyphs (see
	// docs/ui/rmlui_agent_guide.md).
	int fonts_loaded = 0;
	const int game_path_len = (int)strlen(FileSys::get_game_path());
	for (auto file : FileSys::find_game_files_path("ui/fonts")) {
		if (file.size() >= 4 && (file.compare(file.size() - 4, 4, ".ttf") == 0 || file.compare(file.size() - 4, 4, ".otf") == 0)) {
			// find_game_files_path yields absolute disk paths; LoadFontFace goes
			// through RmlUiFileInterface::Open -> FileSys::open_read_game, which
			// expects a path relative to g_project_base (it prepends the base
			// itself) - passing the absolute path back in double-prefixes it and
			// fails to open.
			if (file.compare(0, game_path_len, FileSys::get_game_path()) == 0)
				file = file.substr(game_path_len + 1);
			if (Rml::LoadFontFace(file))
				fonts_loaded++;
			else
				sys_print(Error, "RmlUiSystem: failed to load font face %s\n", file.c_str());
		}
	}
	if (fonts_loaded == 0)
		sys_print(Warning, "RmlUiSystem: no .ttf/.otf fonts found under Data/ui/fonts/ - RmlUi text will not render\n");
}

void RmlUiSystem::shutdown() {
	rmlui_lua_reset();
	g_rmlui_render_contexts = nullptr;
	documents.clear();
	if (context)
		Rml::RemoveContext("main");
	context = nullptr;
	Rml::Shutdown();
}

void RmlUiSystem::update() {
	ASSERT(context);
	Rect2d vp = UiSystem::inst->get_vp_rect();
	const int w = vp.w > 0 ? vp.w : 1;
	const int h = vp.h > 0 ? vp.h : 1;
	if (w != last_vp_w || h != last_vp_h) {
		last_vp_w = w;
		last_vp_h = h;
		context->SetDimensions(Rml::Vector2i(w, h));
	}

#ifdef EDITOR_BUILD
	poll_hot_reload();
#endif

	context->Update();
}

void RmlUiSystem::handle_event(const SDL_Event& event) {
	if (!context)
		return;
	const int mods = translate_sdl_mods(SDL_GetModState());
	switch (event.type) {
	case SDL_EVENT_MOUSE_MOTION:
		context->ProcessMouseMove((int)event.motion.x, (int)event.motion.y, mods);
		break;
	case SDL_EVENT_MOUSE_BUTTON_DOWN:
		context->ProcessMouseButtonDown(event.button.button - 1, mods);
		break;
	case SDL_EVENT_MOUSE_BUTTON_UP:
		context->ProcessMouseButtonUp(event.button.button - 1, mods);
		break;
	case SDL_EVENT_MOUSE_WHEEL:
		context->ProcessMouseWheel(Rml::Vector2f(-event.wheel.x, -event.wheel.y), mods);
		break;
	case SDL_EVENT_KEY_DOWN:
		context->ProcessKeyDown(translate_sdl_key(event.key.key), mods);
		break;
	case SDL_EVENT_KEY_UP:
		context->ProcessKeyUp(translate_sdl_key(event.key.key), mods);
		break;
	case SDL_EVENT_TEXT_INPUT:
		context->ProcessTextInput(Rml::String(event.text.text));
		break;
	default:
		break;
	}
}

Rml::ElementDocument* RmlUiSystem::find_doc(RmlDocHandle handle) {
	auto it = documents.find(handle);
	return it != documents.end() ? it->second : nullptr;
}

RmlDocHandle RmlUiSystem::load_document(const std::string& path) {
	ASSERT(context);
	Rml::ElementDocument* doc = context->LoadDocument(path);
	if (!doc) {
		sys_print(Error, "RmlUiSystem::load_document failed: %s\n", path.c_str());
		return RML_INVALID_DOC;
	}
	RmlDocHandle handle = next_handle++;
	documents[handle] = doc;
	document_paths[handle] = path;
	document_was_visible[handle] = false;
	return handle;
}

void RmlUiSystem::show_document(RmlDocHandle handle) {
	if (Rml::ElementDocument* doc = find_doc(handle)) {
		doc->Show();
		document_was_visible[handle] = true;
	}
}

void RmlUiSystem::hide_document(RmlDocHandle handle) {
	if (Rml::ElementDocument* doc = find_doc(handle)) {
		doc->Hide();
		document_was_visible[handle] = false;
	}
}

void RmlUiSystem::close_document(RmlDocHandle handle) {
	if (Rml::ElementDocument* doc = find_doc(handle)) {
		doc->Close();
		documents.erase(handle);
		document_paths.erase(handle);
		document_was_visible.erase(handle);
	}
}

#ifdef EDITOR_BUILD
void RmlUiSystem::reload_document(RmlDocHandle handle) {
	auto path_it = document_paths.find(handle);
	if (path_it == document_paths.end())
		return;
	const bool was_visible = document_was_visible[handle];
	Rml::ElementDocument* old_doc = find_doc(handle);
	if (old_doc)
		old_doc->Close();

	Rml::ElementDocument* new_doc = context->LoadDocument(path_it->second);
	if (!new_doc) {
		sys_print(Error, "RmlUiSystem: hot reload failed for %s\n", path_it->second.c_str());
		documents.erase(handle);
		return;
	}
	documents[handle] = new_doc;
	if (was_visible)
		new_doc->Show();
}

void RmlUiSystem::poll_hot_reload() {
	if (!file_watcher)
		return;
	auto changed = file_watcher->poll(300);
	if (changed.empty())
		return;

	bool any_rcss = false;
	for (auto& rel_path : changed) {
		if (rel_path.size() >= 5 && rel_path.compare(rel_path.size() - 5, 5, ".rcss") == 0)
			any_rcss = true;
	}
	// RmlUi has no in-place stylesheet-only reload API exposed on Context, so
	// a changed .rcss falls back to reloading every open document (keeps
	// iteration tight without needing per-stylesheet dependency tracking).
	if (any_rcss) {
		for (auto& [handle, doc] : std::unordered_map<RmlDocHandle, Rml::ElementDocument*>(documents))
			reload_document(handle);
		return;
	}

	for (auto& rel_path : changed) {
		if (!(rel_path.size() >= 4 && rel_path.compare(rel_path.size() - 4, 4, ".rml") == 0))
			continue;
		for (auto& [handle, path] : document_paths) {
			if (path.find(rel_path) != std::string::npos) {
				reload_document(handle);
				break;
			}
		}
	}
}
#endif
