#ifndef PLAYERMOVE_H
#define PLAYERMOVE_H
#include "User_Camera.h"
#include "GameEnginePublic.h"
#include <memory>

#include "PlayerAnimDriver.h"
#include "Game/Entity.h"
#include "Game/Components/MeshComponent.h"
#include "Game/Components/LightComponents.h"
#include "Game/Components/PhysicsComponents.h"
#include "Framework/MulticastDelegate.h"

using std::unique_ptr;
using std::vector;

class Entity;
class MeshBuilder;
class Animation_Set;

extern void find_spawn_position(Entity* ent);

enum class Action_State
{
	Idle,
	Moving,
	Jumped,
	Falling,
};

struct PlayerFlags
{
	enum Enum
	{ FrozenView = 1, };
};

class HealthComponent;
class InputUser;
class CharacterController;
class BikeEntity;
#include "Game/GameplayStatic.h"
class SpawnerComponent;
class LuaMiscFuncs : public ClassBase
{
public:
	CLASS_BODY(LuaMiscFuncs, scriptable);
	static LuaMiscFuncs* inst;
	REF virtual Entity* create_ragdoll() { return nullptr; }
};

class CameraPathFollower
{
public:
	CameraPathFollower(std::vector<SpawnerComponent*> components);
	static glm::vec3 catmull_rom(glm::vec3 p0, glm::vec3 p1, glm::vec3 p2, glm::vec3 p3, float t);
	void update();

	struct Point
	{
		glm::vec3 p;
		glm::quat q;
	};
	std::vector<Point> points;
	double time_per_point = 6.0;
	double time_start = 0.0;
};
template <typename T> inline T negative_modulo(T x, T mod_) {
	return ((x % mod_) + mod_ % mod_);
}
#include "Scripting/ScriptFunctionCodegen.h"
#include "Framework/MathLib.h"
#include "Game/Entities/Player.h"
struct CamPathPoints
{
	STRUCT_BODY();

	REF float time = 1.0;
	REF std::vector<lTransform> points;
};
class CamPathFollowerLua : public ClassBase
{
public:
	CLASS_BODY(CamPathFollowerLua);

	REF void clear_all() { paths.clear(); }
	REF void add(CamPathPoints points) { paths.push_back(points); }
	REF void goto_next() {
		cur_time = 0.0;
		cur_idx = (cur_idx + 1) % paths.size();
	}
	REF void goto_prev() {
		cur_time = 0.0;
		cur_idx -= 1;
		if (cur_idx < 0)
			cur_idx += paths.size();
	}
	REF void update();
	float cur_time = 0.0;
	int cur_idx = -1;
	std::vector<CamPathPoints> paths;
};

#include "Input/InputSystem.h"
//
/// <summary>
///
/// </summary>
///
///
#include "UI/GUISystemPublic.h"

struct lVec2
{
	STRUCT_BODY();
	lVec2() = default;
	lVec2(const glm::ivec2& v) {
		x = v.x;
		y = v.y;
	}
	lVec2(const glm::vec2& v) {
		x = v.x;
		y = v.y;
	}

	REF float x = 0;
	REF float y = 0;
};
class lInput : public ClassBase
{
public:
	CLASS_BODY(lInput);
	REF static bool is_key_down(int key) { return Input::is_key_down(SDL_Scancode(key)); }
	REF static bool was_key_pressed(int key) { return Input::was_key_pressed(SDL_Scancode(key)); }
	REF static bool was_key_released(int key) { return Input::was_key_released(SDL_Scancode(key)); }
	REF static bool is_con_button_down(int con_button) {
		return Input::is_con_button_down(SDL_GameControllerButton(con_button));
	}
	REF static bool was_con_button_pressed(int con_button) {
		return Input::was_con_button_pressed(SDL_GameControllerButton(con_button));
	}
	REF static bool was_con_button_released(int con_button) {
		return Input::was_con_button_released(SDL_GameControllerButton(con_button));
	}
	REF static float get_con_axis(int con_axis) { return Input::get_con_axis(SDL_GameControllerAxis(con_axis)); }
	REF static bool is_any_con_active() { return Input::is_any_con_active(); }
	REF static bool is_mouse_down(int button) { return Input::is_mouse_down(button); }
	REF static bool was_mouse_pressed(int button) { return Input::was_mouse_pressed(button); }
	REF static bool was_mouse_released(int button) { return Input::was_mouse_released(button); }
	REF static lVec2 get_mouse_delta() { return Input::get_mouse_delta(); }
	REF static lVec2 get_mouse_pos() { return Input::get_mouse_pos(); }
	REF static void set_capture_mouse(bool b) { UiSystem::inst->set_game_capture_mouse(b); }
	REF static bool is_imgui_blocking_inputs() {
		return UiSystem::inst->blocking_keyboard_inputs() || UiSystem::inst->blocking_mouse_inputs();
	}
};

/// <summary>
///
/// </summary>
class Player : public Component
{
public:
	CLASS_BODY(Player);

	Player();
	~Player() override; //

	REF virtual void do_something() {}

	MeshComponent* player_mesh{};
	CapsuleComponent* player_capsule{};
	MeshComponent* viewmodel_mesh{};
	HealthComponent* health{};
	SpotLightComponent* spotlight{};

	BikeEntity* bike = nullptr;

	MulticastDelegate<int> score_update_delegate;

	std::unique_ptr<CharacterController> ccontroller;

	// PlayerBase overrides
	void get_view(glm::mat4& viewMatrix, float& fov);

	// Entity overrides
	void update() final;
	void start() final;
	void stop() final;

	void on_jump_callback();

public:
	glm::vec3 calc_eye_position();

	void find_a_spawn_point();

	void on_foot_update();

	// current viewangles for player
	glm::vec3 view_angles = glm::vec3(0.f);
	glm::quat view_quat{};
	glm::vec3 view_pos{};

	float distTraveledSinceLastFootstep = 0.0;

	// how long has current state been active
	// how long in air? how long on ground?
	float state_time = 0.0;
	bool is_crouching = false;
	Action_State action = Action_State::Idle;

	bool is_on_ground() const { return action != Action_State::Falling && action != Action_State::Jumped; }

	glm::vec3 velocity{};

	bool has_flag(PlayerFlags::Enum flag) const { return flags & flag; }
	void set_flag(PlayerFlags::Enum flag, bool val) {
		if (val)
			flags = PlayerFlags::Enum(flags | flag);
		else
			flags = PlayerFlags::Enum(flags & (~flag));
	}

	PlayerFlags::Enum flags = {};

private:
	float wall_jump_cooldown = 0.0;
	// physics stuff

	glm::vec3 get_look_vec() { return AnglesToVector(view_angles.x, view_angles.y); }
};

#endif // !PLAYERMOVE_H
