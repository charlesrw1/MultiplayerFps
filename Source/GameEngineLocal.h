#pragma once
#include <SDL2/SDL.h>
#include <string>
#include "Types.h"
#include "Framework/Config.h"
#include "Game/Entity.h"

#include "Debug.h"

#include "GameEnginePublic.h"
#include "Framework/MulticastDelegate.h"
#include <unordered_set>
#include "Level.h"
#include "UI/OnScreenLogGui.h"

class GUIFullscreen;
class OnScreenLog;
using std::unordered_set;
using glm::vec3;
class IEditorTool;
class Player;
class Model;
using std::string;
using std::vector;
class Archive;
class Client;
class Server;
class SceneAsset;
struct ImNodesContext;
class GUI_RootControl;
class UIControl;
struct SceneDrawParamsEx;
struct View_Setup;
class IntegrationTester;
class EditorState;



class GameEngineLocal : public GameEnginePublic
{
public:
	GameEngineLocal();

	// Public Interface
	Application* get_app() const final {
		return nullptr;
	}
	Level* get_level() const final {
		return level.get();
	}
	Entity* get_entity(uint64_t handle) final {
		ASSERT(get_level());
		auto o = level->get_entity(handle);
		return o ? o->cast_to<Entity>() : nullptr;
	}
	BaseUpdater* get_object(uint64_t handle) final {
		ASSERT(get_level());
		return level->get_entity(handle);
	}

	Client* get_client() final {
		return nullptr;// cl.get();
	}
	Server* get_server() final {
		return nullptr;// sv.get();
	}
	SDL_Window* get_os_window() final {
		return window;
	}
	ImGuiContext* get_imgui_context() const final {
		return imgui_context;
	}

	void log_to_fullscreen_gui(LogType type, const char* msg) final;



	bool is_editor_level() const  final {
		/* this passes when you are in the loading phase or past the loading phase */
		/* added is_loading... to work with constructors before level gets set */
		return is_loading_editor_level || (get_level() && get_level()->is_editor_level());
	}

	bool is_host() const final { return true; }

// local functions
public:
	void init(int argc, char** argv);
	void cleanup();

	void get_draw_params(SceneDrawParamsEx& param, View_Setup& setup);
	bool game_thread_update();
	void loop();

	// state relevant functions
	void set_tick_rate(float tick_rate);

	bool is_in_game() const {
		return get_level() != nullptr;
	}

#ifdef EDITOR_BUILD
#else
	bool is_in_an_editor_state() const { return false; }
#endif


	void stop_game();
	void spawn_starting_players(bool initial);

	// Host functions

	MulticastDelegate<> on_begin_map_change;
	MulticastDelegate<> on_leave_level;
	MulticastDelegate<> on_leave_editor;
	MulticastDelegate<IEditorTool*> on_enter_editor;
public:
	void add_commands();
	void set_keybind(SDL_Scancode code, uint16_t keymod, std::string bind);

	bool map_spawned() { return level != nullptr; }

	OnScreenLog gui_log;
	string queued_mapname;
	bool is_loading_editor_level = false;
	std::unique_ptr<Level> level= nullptr;
	std::unique_ptr<Application> app;
	ImGuiContext* imgui_context = nullptr;
	SDL_Window* window = nullptr;
	SDL_GLContext gl_context = nullptr;
	bool show_console = false;
	bool dedicated_server = false;

	bool is_drawing_to_window_viewport() const;

	int argc = 0;
	char** argv = nullptr;

	uptr<ConsoleCmdGroup> commands;
#ifdef EDITOR_BUILD
	// stores state changes to enable forwards/backwards when editing
	// like ["start_ed Map mymap", "map mymap"]
	std::vector<std::string> engine_map_state_history;
	std::vector<std::string> engine_map_state_future;
	uptr<IntegrationTester> tester;
	uptr<EditorState> editorState;
#endif

	void set_tester(IntegrationTester* tester, bool headless_mode);

	void insert_this_map_as_level(SceneAsset*& asset, bool is_for_playing);
	bool is_waiting_on_map_load = false;
private:

	bool is_hosting_game = false;
	bool headless_mode = false;
	void init_sdl_window();
	void key_event(SDL_Event event);
	void draw_any_imgui_interfaces();
	void game_update_tick();

	friend class Ent_Iterator;

	vector<string>* find_keybinds(SDL_Scancode code, uint16_t keymod);
	std::unordered_map<uint32_t, vector<string>> keybinds;
};

extern GameEngineLocal eng_local;