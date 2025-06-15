#pragma once
#include <SDL2/SDL.h>
#include <string>
#include "Types.h"
#include "Framework/Config.h"
#include "Game/Entity.h"

#include "Debug.h"

#include "GameEnginePublic.h"
#include "OsInput.h"
#include "Framework/MulticastDelegate.h"

#include "Level.h"

class GUIFullscreen;
class OnScreenLog;

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
class ImNodesContext;
class GUI_RootControl;
class UIControl;
struct SceneDrawParamsEx;
struct View_Setup;
class GameEngineLocal : public GameEnginePublic
{
public:
	GameEngineLocal();

	// Public Interface
	GameMode* get_gamemode() const final {
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
	Entity* get_local_player() final {
		ASSERT(get_level());
		return nullptr;
	}
	Entity* get_player_slot(uint32_t index) final {
		// fixme:
		if (index == 0) return get_local_player();
		else return nullptr;
	}
	uint32_t get_local_player_slot() final {
		return 0;
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
	IEditorTool* get_current_tool() const final {
		return active_tool;
	}
	const OsInput* get_input_state() final {
		return &inp;
	}
	ImGuiContext* get_imgui_context() const final {
		return imgui_context;
	}

	void log_to_fullscreen_gui(LogType type, const char* msg) final;

	void login_new_player(uint32_t index) final;
	void logout_player(uint32_t index) final;

	void leave_level() final;
	void open_level(string levelname) final;
	void connect_to(string address) final;

	bool is_editor_level() const  final {
		/* this passes when you are in the loading phase or past the loading phase */
		/* added is_loading... to work with constructors before level gets set */
		return is_loading_editor_level || (get_level() && get_level()->is_editor_level());
	}

	glm::ivec2 get_game_viewport_size() const final;	// either full window or sub window
	Engine_State get_state() const final { return state; }
	MulticastDelegate<bool>& get_on_map_delegate() final {
		return on_map_load_return;
	}
	bool is_game_focused() const final { return game_focused; }
	void set_game_focused(bool focus) final;
	bool is_host() const final { return true; }

// local functions
public:
	void init(int argc, char** argv);
	void cleanup();

	void get_draw_params(SceneDrawParamsEx& param, View_Setup& setup);
	bool game_thread_update();
	void loop();
	bool state_machine_update();

	// state relevant functions
	void set_tick_rate(float tick_rate);

	bool is_in_game() const {
		return state == Engine_State::Game;
	}

#ifdef EDITOR_BUILD
	bool is_in_an_editor_state() const { return get_current_tool() != nullptr; }
	void change_editor_state(IEditorTool* next_tool, const char* arg, const char* file = "");
#else
	bool is_in_an_editor_state() const { return false; }
#endif

	void execute_map_change();
	void on_map_change_callback(bool is_for_editor, SceneAsset* loadedLevel);

	void stop_game();
	void spawn_starting_players(bool initial);

	// Host functions

	MulticastDelegate<bool> on_map_load_return;
public:

	void set_keybind(SDL_Scancode code, uint16_t keymod, std::string bind);

	bool map_spawned() { return level != nullptr; }

	//std::unique_ptr<Client> cl;
	//std::unique_ptr<Server> sv;

	OnScreenLog* gui_log{};

	OsInput inp;

	string queued_mapname;
	bool is_waiting_on_load_level_callback = false;
	bool is_loading_editor_level = false;
	std::unique_ptr<Level> level= nullptr;

	ImGuiContext* imgui_context = nullptr;
	SDL_Window* window = nullptr;
	SDL_GLContext gl_context = nullptr;

	bool show_console = false;
	bool dedicated_server = false;
	bool wants_gc_flag = false;

	bool is_drawing_to_window_viewport() const;

	glm::ivec2 window_viewport_size = glm::ivec2(1200,800);

	int argc = 0;
	char** argv = nullptr;


#ifdef EDITOR_BUILD
	// stores state changes to enable forwards/backwards when editing
	// like ["start_ed Map mymap", "map mymap"]
	std::vector<std::string> engine_map_state_history;
	std::vector<std::string> engine_map_state_future;
#endif
private:
	// when game goes into focus mode, the mouse position is saved so it can be reset when exiting focus mode
	int saved_mouse_x=0, saved_mouse_y=0;
	// focused= mouse is captured, assumes relative inputs are being taken, otherwise cursor is shown
	bool game_focused = false;

	Engine_State state = Engine_State::Idle;
	IEditorTool* active_tool = nullptr;

	bool is_hosting_game = false;

	void init_sdl_window();
	void key_event(SDL_Event event);

	void draw_any_imgui_interfaces();

	void game_update_tick();
	void do_asset_gc();

	friend class Ent_Iterator;

	vector<string>* find_keybinds(SDL_Scancode code, uint16_t keymod);
	std::unordered_map<uint32_t, vector<string>> keybinds;
};

extern GameEngineLocal eng_local;