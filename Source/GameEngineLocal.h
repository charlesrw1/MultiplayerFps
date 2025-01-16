#pragma once
#include <SDL2/SDL.h>
#include <string>
#include "Types.h"
#include "Framework/Config.h"
#include "Render/ParticlesPublic.h"
#include "Game/Entity.h"

#include "Debug.h"

#include "GameEnginePublic.h"
#include "OsInput.h"
#include "Framework/MulticastDelegate.h"


class GUIFullscreen;
class OnScreenLogGui;

using glm::vec3;
class IEditorTool;
class Player;
class Model;
using std::string;

class Archive;
class Client;
class Server;
class SceneAsset;
class ImNodesContext;
class GUI_RootControl;
class UIControl;
class GameEngineLocal : public GameEnginePublic
{
public:
	GameEngineLocal();

	// Public Interface
	virtual GameMode* get_gamemode() const override {
		return level ? level->get_gamemode() : nullptr;
	}
	virtual Level* get_level() const override {
		return level.get();
	}
	virtual Entity* get_entity(uint64_t handle) {
		ASSERT(get_level());
		auto o = level->get_entity(handle);
		return o ? o->cast_to<Entity>() : nullptr;
	}
	virtual BaseUpdater* get_object(uint64_t handle) {
		ASSERT(get_level());
		return level->get_entity(handle);
	}
	virtual Entity* get_local_player() override {
		ASSERT(get_level());
		return level->get_local_player();
	}
	virtual Entity* get_player_slot(uint32_t index) override {
		// fixme:
		if (index == 0) return get_local_player();
		else return nullptr;
	}
	virtual uint32_t get_local_player_slot() override {
		return 0;
	}
	virtual GuiSystemPublic* get_gui() const override {
		return gui_sys.get();
	}
	virtual Client* get_client() override {
		return cl.get();
	}
	virtual Server* get_server() override {
		return sv.get();
	}
	virtual SDL_Window* get_os_window() override {
		return window;
	}
	virtual IEditorTool* get_current_tool() const override {
		return active_tool;
	}
	virtual const OsInput* get_input_state() override {
		return &inp;
	}
	virtual ImGuiContext* get_imgui_context() const {
		return imgui_context;
	}

	virtual void log_to_fullscreen_gui(LogType type, const char* msg) override;

	virtual void login_new_player(uint32_t index) override;
	virtual void logout_player(uint32_t index) override;

	virtual void leave_level() override;
	virtual void open_level(string levelname) override;
	virtual void connect_to(string address) override;

	virtual bool is_editor_level() const {
		/* this passes when you are in the loading phase or past the loading phase */
		/* added is_loading... to work with constructors before level gets set */
		return is_loading_editor_level || (get_level() && get_level()->is_editor_level());
	}

	glm::ivec2 get_game_viewport_size() const override;	// either full window or sub window

	void init();
	void cleanup();

	void loop();
	void draw_screen();
	void pre_render_update();
	// state relevant functions
	void set_tick_rate(float tick_rate);

	bool is_in_game() const {
		return state == Engine_State::Game;
	}
	virtual Engine_State get_state() const override { return state; }
	bool is_in_an_editor_state() const { return get_current_tool() != nullptr; }
	void change_editor_state(IEditorTool* next_tool, const char* arg, const char* file = "");

	void execute_map_change();
	void on_map_change_callback(bool is_for_editor, SceneAsset* loadedLevel);

	void stop_game();
	void spawn_starting_players(bool initial);

	bool game_draw_screen();

	// Host functions

	MulticastDelegate<bool>& get_on_map_delegate() override {
		return on_map_load_return;
	}
	MulticastDelegate<bool> on_map_load_return;
public:

	void set_keybind(SDL_Scancode code, uint16_t keymod, std::string bind);

	bool map_spawned() { return level != nullptr; }

	std::unique_ptr<GuiSystemPublic> gui_sys;
	std::unique_ptr<Client> cl;
	std::unique_ptr<Server> sv;

	GUIFullscreen* engine_fullscreen_gui{};
	OnScreenLogGui* gui_log{};

	OsInput inp;

	string queued_mapname;
	bool is_loading_editor_level = false;
	std::unique_ptr<Level> level= nullptr;

	ImGuiContext* imgui_context = nullptr;
	SDL_Window* window = nullptr;
	SDL_GLContext gl_context = nullptr;

	bool show_console = false;
	bool dedicated_server = false;
	
	bool is_game_focused() const override { return game_focused; }
	void set_game_focused(bool focus) override;

	bool is_drawing_to_window_viewport() const;

	glm::ivec2 window_viewport_size = glm::ivec2(DEFAULT_WIDTH,DEFAULT_HEIGHT);

	int argc = 0;
	char** argv = nullptr;

	bool is_host() const override { return true; }

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

	friend class Ent_Iterator;


	std::string* find_keybind(SDL_Scancode code, uint16_t keymod);

	std::unordered_map<uint32_t, string> keybinds;
};

extern GameEngineLocal eng_local;