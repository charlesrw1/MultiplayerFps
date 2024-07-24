#pragma once
#include <cstdint>
#include <string>
#include "Framework/Config.h"
#include "glm/glm.hpp"

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

class Level;
class Entity;
class Player;
class UIControl;
class Client;
class Server;
class SDL_Window;
class IEditorTool;
class Schema;
class Level;
class OsInput;
struct ImGuiContext;
class ClassTypeInfo;
class GameEnginePublic
{
public:
	virtual Level* get_level() const = 0;
	virtual Entity* get_entity(uint64_t handle) = 0;
	virtual Entity* get_local_player() = 0;
	virtual Entity* get_player_slot(uint32_t index) = 0;
	virtual uint32_t get_local_player_slot() = 0;
	virtual UIControl* get_gui() = 0;
	virtual Client* get_client() = 0;
	virtual Server* get_server() = 0;
	virtual SDL_Window* get_os_window() = 0;
	virtual IEditorTool* get_current_tool() const = 0;
	virtual Engine_State get_state() const = 0;
	virtual const OsInput* get_input_state() = 0;
	virtual bool is_game_focused() const = 0;
	virtual glm::ivec2 get_game_viewport_size() const = 0;
	virtual ImGuiContext* get_imgui_context() const = 0;
	virtual bool is_host() const = 0;
	virtual bool is_editor_level() const = 0;

	virtual void leave_level() = 0;
	// queues a level to be loaded
	virtual void open_level(std::string levelname) = 0;
	virtual void connect_to(std::string address) = 0;

	virtual void login_new_player(uint32_t index) = 0;
	virtual void logout_player(uint32_t index) = 0;

	template<typename T>
	T* spawn_entity_class() {
		static_assert(std::is_base_of<Entity, T>::value, "spawn_entity_class not derived from Entity");
		Entity* e = spawn_entity_from_classtype(&T::StaticType);
		return (T*)e;
	}
	virtual Entity* spawn_entity_from_classtype(const ClassTypeInfo* ti) = 0;
	virtual Entity* spawn_entity_schema(const Schema* schema) = 0;
	virtual void remove_entity(Entity* e) = 0;

	double get_game_time() const {
		return time;
	}
	double get_frame_time() const {
		return frame_time;
	}
	double get_tick_interval() const {
		return tick_interval;
	}
	uint32_t get_game_tick() const {
		return tick;
	}
	double get_frame_remainder() const {
		return frame_remainder;
	}

	// used by client/server for syncing
	void set_tick_interval(double next_interval) {
		assert(get_state() != Engine_State::Game);
		tick_interval = next_interval;
	}
	void set_game_tick(uint32_t newtick) {
		tick = newtick;
	}
	void set_game_time(double newtime) {
		time = newtime;
	}
protected:

	double time = 0.0;			// this is essentially tick*tick_interval +- smoothing on client
	uint32_t tick = 0;				// this is the discretized time tick
	double frame_time = 0.0;	// total frame time of program
	double frame_remainder = 0.0;	// frame time accumulator
	double tick_interval = 1.0/60.0;	// 1/tick_rate
};


extern GameEnginePublic* eng;