#pragma once
#include <cstdint>
#include <string>
#include "Framework/Config.h"
#include "glm/glm.hpp"
#include "DeferredSpawnScope.h"
#include "Framework/ScopedBoolean.h"
#include "Framework/ClassBase.h"
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
class ClassTypeInfo;
class GuiSystemPublic;
class GameMode;
class BaseUpdater;

class Application : public ClassBase {
public:
	CLASS_BODY(Application, scriptable);

	REF virtual void start() {}
	REF virtual void update() {}
	REF virtual void stop() {}
	REF virtual void on_map_changed() {}

	// misc callbacks
	REF virtual void on_controller_status(int index, bool connected) {}

	// should go elsewhere tbh
	REF virtual bool create_prefab(Entity* object, string prefab_str) { return false; }

};

class GameEnginePublic
{
public:
	virtual Application* get_app() const = 0;
	virtual Level* get_level() const = 0;
	virtual Entity* get_entity(uint64_t handle) = 0;
	virtual BaseUpdater* get_object(uint64_t handle) = 0;

	virtual Client* get_client() = 0;
	virtual Server* get_server() = 0;
	virtual SDL_Window* get_os_window() = 0;

	virtual ImGuiContext* get_imgui_context() const = 0;
	virtual bool is_host() const = 0;
	virtual bool is_editor_level() const = 0;

	virtual void log_to_fullscreen_gui(LogType type, const char* msg) = 0;

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
		//assert(get_state() != Engine_State::Game);
		tick_interval = next_interval;
	}
	void set_game_time(double newtime) {
		time = newtime;
	}
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