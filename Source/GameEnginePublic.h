#pragma once
#include <cstdint>
#include <string>
#include "Framework/Config.h"
#include "glm/glm.hpp"
#include "DeferredSpawnScope.h"
#include "Framework/ScopedBoolean.h"
extern ConfigVar g_thirdperson;
extern ConfigVar g_fov;
extern ConfigVar g_mousesens;
extern ConfigVar g_fakemovedebug;
extern ConfigVar g_drawdebugmenu;
extern ConfigVar g_drawimguidemo;
extern ConfigVar g_slomo;

extern ConfigVar g_window_w;
extern ConfigVar g_window_h;
extern ConfigVar g_window_fullscreen;
extern ConfigVar g_host_port;

enum class Engine_State
{
	Idle,		// main menu or in tool state
	Loading,	// loading next level
	Game,		// in game state
};


template<typename... Args>
class MulticastDelegate;

class Level;
class Entity;
class Player;
class Client;
class Server;
struct SDL_Window;
class IEditorTool;
class Schema;
class Level;
class OsInput;
struct ImGuiContext;
struct ClassTypeInfo;
class GuiSystemPublic;
class GameMode;
class BaseUpdater;
class GameEnginePublic
{
public:
	virtual GameMode* get_gamemode() const = 0;
	virtual Level* get_level() const = 0;
	virtual Entity* get_entity(uint64_t handle) = 0;
	virtual BaseUpdater* get_object(uint64_t handle) = 0;
	virtual Entity* get_local_player() = 0;
	virtual Entity* get_player_slot(uint32_t index) = 0;
	virtual uint32_t get_local_player_slot() = 0;
	virtual Client* get_client() = 0;
	virtual Server* get_server() = 0;
	virtual SDL_Window* get_os_window() = 0;
	virtual IEditorTool* get_current_tool() const = 0;
	virtual Engine_State get_state() const = 0;
	virtual const OsInput* get_input_state() = 0;
	virtual bool is_game_focused() const = 0;
	virtual void set_game_focused(bool focus) = 0;
	virtual glm::ivec2 get_game_viewport_size() const = 0;
	virtual ImGuiContext* get_imgui_context() const = 0;
	virtual bool is_host() const = 0;
	virtual bool is_editor_level() const = 0;

	virtual void log_to_fullscreen_gui(LogType type, const char* msg) = 0;

	virtual void leave_level() = 0;
	// queues a level to be loaded
	virtual void open_level(std::string levelname) = 0;
	virtual void connect_to(std::string address) = 0;

	virtual void login_new_player(uint32_t index) = 0;
	virtual void logout_player(uint32_t index) = 0;

	double get_game_time() const {
		return time;
	}
	// update() interval
	double get_dt() const {
		return frame_time;
	}
	// physics update() interval
	double get_fixed_tick_interval() const {
		return tick_interval;
	}
	double get_frame_remainder() const {
		return frame_remainder;
	}

	// used by client/server for syncing
	void set_tick_interval(double next_interval) {
		assert(get_state() != Engine_State::Game);
		tick_interval = next_interval;
	}
	void set_game_time(double newtime) {
		time = newtime;
	}

	// callbacks
	virtual MulticastDelegate<bool>& get_on_map_delegate() = 0;	// called after a map was loaded (both editor and game, called after gamemode.init)protected:

	double time = 0.0;			// this is essentially tick*tick_interval +- smoothing on client
	double frame_time = 0.0;	// total frame time of program
	double frame_remainder = 0.0;	// frame time accumulator
	double tick_interval = 1.0/60.0;	// 1/tick_rate

	bool get_is_in_overlapped_period() const {
		return b_is_in_overlapped_period.get_value();
	}
protected:
	ScopedBooleanValue b_is_in_overlapped_period;
};


extern GameEnginePublic* eng;