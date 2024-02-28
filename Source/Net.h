#ifndef NET_H
#define NET_H
#include "Bytepacker.h"
#include "Connection.h"
#include "MoveCommand.h"
#include "Animation.h"
#include <queue>

const int CLIENT_SNAPSHOT_HISTORY = 16;	// buffer last 16 snapshots

const int MAX_PAYLOAD_SIZE = 1400;
const int PACKET_HEADER_SIZE = 8;

const int DEFAULT_SERVER_PORT = 47000;

const int MAX_CLIENTS = 16;
const int MAX_NET_STRING = 256;
const unsigned CONNECTIONLESS_SEQUENCE = 0xffffffff;
//const int MAX_CONNECT_ATTEMPTS = 10;
//const float CONNECT_RETRY_TIME = 2.f;
//const double MAX_TIME_OUT = 5.f;
const int CLIENT_MOVE_HISTORY = 36;

const int ENTITY_BITS = 8;
const int NUM_GAME_ENTS = 1 << ENTITY_BITS;
const int ENTITY_SENTINAL = NUM_GAME_ENTS - 1;

const double DEFAULT_UPDATE_RATE = 66.66;	// server+client ticks 66 times a second
const int DEFAULT_MOVECMD_RATE = 60;	// send inputs (multiple) 60 times a second
const int DEFAULT_SNAPSHOT_RATE = 30;	// send x snapshots a second

// Messages
enum Server_To_Client
{
	SV_NOP = 0,
	SV_INITIAL,	// first message to send back to client
	SV_TICK,
	SV_SNAPSHOT,	
	SV_DISCONNECT,
	SV_UPDATE_VIEW,
	SV_TEXT,
};
enum Client_To_Server
{
	CL_NOP = 0,
	CL_INPUT,
	CL_QUIT,
	CL_TEXT,
	CL_DELTA,
	CL_SET_BASELINE,
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

	float fire_rate=1.f;
	float reload_time=0.0;
	float holster_time=0.0;
	float draw_time=0.0;
	int damage=0;
	int clip_size=0;
	int start_ammo=0;
	float spread=0.f;
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
	const char* classname="";

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
	glm::vec3 diff_angles=glm::vec3(0.f);

	void(*update)(Entity* me) = nullptr;
	void(*damage)(Entity* me, Entity* attacker, int amount, int flags) = nullptr;
	void(*hit_wall)(Entity* me, glm::vec3 normal) = nullptr;
	bool(*touch)(Entity* me, Entity* other) = nullptr;
	void(*timer_callback)(Entity* me) = nullptr;

	Animator anim;
	const Model* model = nullptr;

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

struct Net_Prop
{
	const char* name;
	int offset;
	int input_bits;
	int output_bits = -1; 
	float quantize = 1.f;
	short condition = DEFAULT_PROP;

	// -1 = same as input, 0 = float, 
	//if output is given in bits but input is float, output is quantized

	enum Conditions
	{
		DEFAULT_PROP = 1,		// rep to both owning/non-owning
		PLAYER_PROP = 2,		// only rep to owning player
		NON_PLAYER_PROP = 4,	// only rep to non-owning player
		ANIM_PROP = 8,

		PLAYER_PROP_MASK = PLAYER_PROP | DEFAULT_PROP | ANIM_PROP,
		NON_PLAYER_PROP_MASK = NON_PLAYER_PROP | DEFAULT_PROP | ANIM_PROP,

		ALL_PROP_MASK = DEFAULT_PROP | PLAYER_PROP | NON_PLAYER_PROP | ANIM_PROP
	};
};


struct Frame;
struct Packed_Entity
{
	Packed_Entity(Frame* f, int offset, int length);

	int index = 0;		// index of entity
	int len = 0;		// length of entity data in bytes
	int buf_offset = 0;	// offset in buffer
	bool failed = false;
	Frame* f;

	ByteReader get_buf();
	void increment();
};


// stores binary data of the game state, used by client and server
struct Frame {
	int tick = 0;
	static const int MAX_FRAME_SNAPSHOT_DATA = 8000;

	int num_ents_this_frame = 0;
	uint8_t data[MAX_FRAME_SNAPSHOT_DATA];

	Packed_Entity begin();

	int player_offset = 0;	// used by clients, HACK!!!
};


void new_entity_fields_test();

// Network serialization functions

void write_full_entity(Entity* e, ByteWriter& msg);
void read_entity(Entity* e, ByteReader& msg, int condition, bool is_delta);
void set_entity_props_from_entity(Entity* from, Entity* to, int condition);
void write_delta_entity(ByteWriter& msg, ByteReader& s0, ByteReader& s1, int condition);

struct Entity_Baseline
{
	uint8_t data[1000];
	int num_bytes_in_data;			// length of data[]
	int num_bytes_including_index;	// includes the x index bits in the rounding
	ByteReader get_buf() {
		return ByteReader(data, num_bytes_in_data, sizeof(data));
	}
};

Entity_Baseline* get_entity_baseline();


#endif