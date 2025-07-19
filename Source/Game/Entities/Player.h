#ifndef PLAYERMOVE_H
#define PLAYERMOVE_H
#include "Types.h"
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
	{
		FrozenView = 1,
	};
};

class HealthComponent;
class InputUser;
class CharacterController;
class BikeEntity;


struct HitResult {
	STRUCT_BODY();
	REF obj<Entity> what;
	REF glm::vec3 pos;
	REF glm::vec3 normal;
	REF bool hit = false;
};


#include "EngineSystemCommands.h"
#include "../Level.h"
class GameplayStatic : public ClassBase {
public:
	CLASS_BODY(GameplayStatic);

	REF static Entity* spawn_prefab(PrefabAsset* prefab);
	REF static Entity* spawn_entity();

	REF static HitResult cast_ray(glm::vec3 start, glm::vec3 end, int channel_mask, PhysicsBody* ignore_this);

	REF static int get_collision_mask_for_physics_layer(PL physics_layer);

	REF static void send_back_result(HitResult res) {
		printf("%f %f %f\n", res.pos.x, res.pos.y, res.pos.z);
	}

	REF static vector<Component*> find_components(const ClassTypeInfo* info);
	REF static Entity* find_by_name(string name);
	REF static float get_dt() {
		return eng->get_dt();
	}
	REF static float get_time() {
		return eng->get_game_time();
	}

	REF static void change_level(string mapname) {
		Cmd_Manager::inst->append_cmd(uptr<SystemCommand>(new OpenMapCommand(mapname, true)));
	}

	REF static string get_current_level_name() {
		if (!eng->get_level())
			return "";
		return eng->get_level()->get_source_asset_name();
	}
	REF static std::vector<obj<Entity>> sphere_overlap(glm::vec3 center, float radius, int channel_mask);

	REF static void reset_debug_text_height();
	REF static void debug_text(string s);
	REF static void debug_sphere(glm::vec3 center, float radius, float life, const lColor& color);
	REF static void debug_line_normal(glm::vec3 p, glm::vec3 n, float len, float life, const lColor& color);

	// kind of hack bs till i work it out better
	// basically nil tables are null and can be checked, but when an object is deleted, the _ptr field int he table is nullptr'd, but the table is non-nil
	// i dont think you can check _ptr in lua since its userdata. so this will get the ClassBase* which does the nil and _ptr null check etc. 
	REF static bool is_null(ClassBase* e) {
		return e == nullptr;
	}

};
#include "Input/InputSystem.h"
//
/// <summary>
/// 
/// </summary>
/// 
/// 
#include "UI/GUISystemPublic.h"

struct lVec2 {
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
class lInput : public ClassBase {
public:
	CLASS_BODY(lInput);
	REF static bool is_key_down(int key) {
		return Input::is_key_down(SDL_Scancode(key));
	}
	REF static bool was_key_pressed(int key) {
		return Input::was_key_pressed(SDL_Scancode(key));
	}
	REF static bool was_key_released(int key) {
		return Input::was_key_released(SDL_Scancode(key));
	}
	REF static bool is_con_button_down(int con_button) {
		return Input::is_con_button_down(SDL_GameControllerButton(con_button));
	}
	REF static bool was_con_button_pressed(int con_button) {
		return Input::was_con_button_pressed(SDL_GameControllerButton(con_button));
	}
	REF static bool was_con_button_released(int con_button) {
		return Input::was_con_button_released(SDL_GameControllerButton(con_button));
	}
	REF static float get_con_axis(int con_axis) {
		return Input::get_con_axis(SDL_GameControllerAxis(con_axis));
	}
	REF static bool is_any_con_active() {
		return Input::is_any_con_active();
	}
	REF static bool is_mouse_down(int button) {
		return Input::is_mouse_down(button);
	}
	REF static bool was_mouse_pressed(int button) {
		return Input::was_mouse_pressed(button);
	}
	REF static bool was_mouse_released(int button) {
		return Input::was_mouse_released(button);
	}
	REF static lVec2 get_mouse_delta() {
		return Input::get_mouse_delta();
	}
	REF static lVec2 get_mouse_pos() {
		return Input::get_mouse_pos();
	}
	REF static void set_capture_mouse(bool b) {
		UiSystem::inst->set_game_capture_mouse(b);
	}
};




/// <summary>
/// 
/// </summary>
class Player : public Component {
public:
	CLASS_BODY(Player);

	Player();
	~Player() override;//

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

	bool has_flag(PlayerFlags::Enum flag) const {
		return flags & flag;
	}
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


	glm::vec3 get_look_vec() {
		return AnglesToVector(view_angles.x, view_angles.y);
	}
};

#endif // !PLAYERMOVE_H
