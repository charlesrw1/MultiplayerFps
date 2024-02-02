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
};

enum Item_Use_State
{
	ITEM_IDLE,
	ITEM_IN_FIRE,
	ITEM_RELOAD,
	ITEM_SCOPED,
	ITEM_RAISING,
	ITEM_LOWERING,
};

struct Item_State
{
	Item_State() {
		memset(ammo, 0, sizeof(ammo));
		memset(clip, 0, sizeof(clip));
	}

	const static int MAX_ITEMS = 32;
	const static int EMPTY_SLOT = -1;

	int item_bitmask = 0;
	int active_item = 0;
	short item_id[MAX_ITEMS];
	short ammo[MAX_ITEMS];
	short clip[MAX_ITEMS];
	float timer = 0.f;

	char inventory[MAX_ITEMS];

	Item_Use_State state = ITEM_IDLE;
};

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
	int index = 0;	// engine.ents[]
	Ent_Type type = ET_FREE;
	glm::vec3 position = glm::vec3(0);
	glm::vec3 rotation = glm::vec3(0);
	int model_index = 0;	// media.gamemodels[]

	glm::vec3 velocity = glm::vec3(0);
	glm::vec3 view_angles = glm::vec3(0.f);

	short state = 0;	// For players: Player_Movement_State
	short flags = 0;	// Entity_Flags

	float timer = 0.f;	// multipurpose timer

	int owner_index = 0;
	int health = 100;
	Item_State items;

	int physics = EPHYS_NONE;
	float col_radius = 0.f;
	float col_height = 0;
	int ground_index = 0;

	// for interpolating entities
	glm::vec3 interp_pos;
	glm::vec3 interp_rot;
	float interp_remaining;
	float interp_time;

	int item = 0;
	int solid = 0;

	float in_air_time = 0.f;

	void(*update)(Entity* me) = nullptr;
	void(*damage)(Entity* me, Entity* attacker, int amount, int flags) = nullptr;
	void(*hit_wall)(Entity* me, glm::vec3 normal) = nullptr;
	bool(*touch)(Entity* me, Entity* other) = nullptr;
	void(*timer_callback)(Entity* me) = nullptr;


	Animator anim;
	const Model* model = nullptr;

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