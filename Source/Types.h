#ifndef GAMETYPE_H
#define GAMETYPE_H
#include "Util.h"
#include "Animation.h"

#define EDITDOC

const int MAX_CLIENTS = 16;
const int DEFAULT_WIDTH = 1200;
const int DEFAULT_HEIGHT = 800;
const int MAX_GAME_ENTS = 256;
const int ENTITY_BITS = 8;
const int NUM_GAME_ENTS = 1 << ENTITY_BITS;
const int ENTITY_SENTINAL = NUM_GAME_ENTS - 1;

const float STANDING_EYE_OFFSET = 1.6f;
const float CROUCH_EYE_OFFSET = 1.1f;
const float CHAR_HITBOX_RADIUS = 0.3f;
const float CHAR_STANDING_HB_HEIGHT = 1.8f;
const float CHAR_CROUCING_HB_HEIGHT = 1.3f;

class User_Camera
{
public:
	glm::vec3 orbit_target = glm::vec3(0.f);
	glm::vec3 position = glm::vec3(0);
	glm::vec3 front = glm::vec3(1, 0, 0);
	glm::vec3 up = glm::vec3(0, 1, 0);
	float move_speed = 0.1f;
	float yaw = 0, pitch = 0;

	bool orbit_mode = false;

	void scroll_callback(int amt);
	void update_from_input(const bool keys[], int mouse_dx, int mouse_dy, glm::mat4 invproj);
	glm::mat4 get_view_matrix() const;
};

struct View_Setup
{
	glm::vec3 origin, front;
	glm::mat4 view, proj, viewproj;
	float fov, near, far;
	int width, height;
};

enum Game_Command_Buttons
{
	BUTTON_JUMP = (1),
	BUTTON_DUCK = (1 << 2),
	BUTTON_FIRE1 = (1 << 3),
	BUTTON_FIRE2 = (1 << 4),
	BUTTON_RELOAD = (1 << 5),
	BUTTON_USE = (1 << 6),

	BUTTON_ITEM1 = (1 << 8),
	BUTTON_ITEM2 = (1 << 9),
	BUTTON_ITEM3 = (1 << 10),
	BUTTON_ITEM4 = (1 << 11),
	BUTTON_ITEM5 = (1 << 12),
	BUTTON_ITEM_NEXT = (1 << 12),
	BUTTON_ITEM_PREV = (1 << 13),
};

struct Move_Command
{
	int tick = 0;
	float forward_move = 0.f;
	float lateral_move = 0.f;
	float up_move = 0.f;
	int button_mask = 0;
	glm::vec3 view_angles = glm::vec3(0.f);

	bool first_sim = true;	// not replicated, used in player_updates

	static uint8_t quantize(float f) {
		return glm::clamp(int((f + 1.0) * 128.f), 0, 255);
	}
	static float unquantize(uint8_t c) {
		return (float(c) - 128.f) / 128.f;
	}
};


enum Ent_Type
{
	ET_PLAYER,
	ET_ITEM,
	ET_GRENADE,
	ET_DUMMY,
	ET_USED = 254,
	ET_FREE = 0xff,
};

enum Entity_Flags
{
	EF_DEAD = 1,
	EF_FORCED_ANIMATION = 2,
	EF_HIDDEN = 4,
	EF_HIDE_ITEM = 8,
	EF_BOUNCE = 16,
	EF_SLIDE = 32,
	EF_SOLID = 64,
	EF_FROZEN_VIEW = 128,
	EF_TELEPORTED = 256,
};

enum Item_Use_State
{
	ITEM_IDLE,
	ITEM_IN_FIRE,
	ITEM_RELOAD,
	ITEM_SCOPED,
	ITEM_RAISING,
	ITEM_LOWERING,
	ITEM_USING,
};

enum Game_Item_Category
{
	ITEM_CAT_RIFLE,
	ITEM_CAT_BOLT_ACTION,
	ITEM_CAT_BOMB,
	ITEM_CAT_MELEE,
	ITEM_CAT_THROWABLE,
};


struct Game_Item_Stats
{
	const char* name = "";
	const char* world_model = "";
	const char* view_model = "";
	int category = 0;
	int param = 0;

	float fire_rate = 1.f;
	float reload_time = 0.0;
	float holster_time = 0.0;
	float draw_time = 0.0;
	int damage = 0;
	int clip_size = 0;
	int start_ammo = 0;
	float spread = 0.f;
};

struct Game_Inventory
{
	enum {
		UNEQUIP = 0,
		GUN_M16,
		GUN_AK47,
		GUN_M24,
		MELEE_KNIFE,
		ITEM_BOMB,
		ITEM_HE_GRENADE,

		NUM_GAME_ITEMS
	};

	int active_item = UNEQUIP;
	int item_mask = 0;
	int ammo[NUM_GAME_ITEMS];
	int clip[NUM_GAME_ITEMS];
	int state = ITEM_IDLE;
	float timer = 0.f;

	int pending_item = -1;

	int tick_for_staging = 0;// horrible hack again
	int staging_item = 0;
	int staging_ammo = 0;
	int staging_clip = 0;

};
Game_Item_Stats* get_item_stats();	// size = NUM_GAME_ITEMS

enum Player_Movement_State
{
	PMS_GROUND = 1,		// on ground, else in air
	PMS_CROUCHING = 2,	// crouching in air or on ground
	PMS_JUMPING = 4,	// first part of jump
};

enum Entity_Physics
{
	EPHYS_NONE,
	EPHYS_PLAYER,
	EPHYS_GRAVITY,
	EPHYS_PROJECTILE,
	EPHYS_MOVER,		// platforms, doors
};

class Model;
struct Entity
{
	int index = 0;	// eng->ents[]
	Ent_Type type = ET_FREE;
	glm::vec3 position = glm::vec3(0);
	glm::vec3 rotation = glm::vec3(0);
	int model_index = 0;	// media.gamemodels[]
	const char* classname = "";

	glm::vec3 velocity = glm::vec3(0);
	glm::vec3 view_angles = glm::vec3(0.f);

	short state = 0;	// For players: Player_Movement_State
	short flags = 0;	// Entity_Flags

	float timer = 0.f;	// multipurpose timer

	int owner_index = 0;
	int health = 100;
	Game_Inventory inv;

	int physics = EPHYS_NONE;
	glm::vec3 col_size;	// for characters, .x=radius,.y=height; for zones, it is an aabb
	float col_radius = 0.f;
	float col_height = 0;
	int ground_index = 0;

	// for interpolating entities
	glm::vec3 interp_pos;
	glm::vec3 interp_rot;
	float interp_remaining;
	float interp_time;

	int target_ent = -1;
	float in_air_time = 0.f;

	int force_angles = 0;	// 1=force, 2=add
	glm::vec3 diff_angles = glm::vec3(0.f);

	void(*update)(Entity* me) = nullptr;
	void(*damage)(Entity* me, Entity* attacker, int amount, int flags) = nullptr;
	void(*hit_wall)(Entity* me, glm::vec3 normal) = nullptr;
	bool(*touch)(Entity* me, Entity* other) = nullptr;
	void(*timer_callback)(Entity* me) = nullptr;

	Animator anim;
	Model* model = nullptr;

	// used for 1 frame interpolation on the local server host for PLAYERS
	bool using_interpolated_pos_and_rot = false;
	glm::vec3 local_sv_interpolated_pos;
	glm::vec3 local_sv_interpolated_rot;
	struct Transform_Hist { bool used; glm::vec3 o; glm::vec3 r; };
	Transform_Hist last[3];
	void add_to_last();
	void shift_last();

	void set_inactive() { type = ET_FREE; }
	bool active() { return type != ET_FREE; }
	void set_model(const char* model);

	void clear_pointers();

	void physics_update();
	void projectile_physics();
	void gravity_physics();
	void mover_physics();
};



#endif // !GAMETYPE_H
