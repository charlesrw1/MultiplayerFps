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
};

// Entity state replicated to clients
struct EntityState
{
	int type = ET_FREE;
	glm::vec3 position=glm::vec3(0.f);
	glm::vec3 angles=glm::vec3(0.f);	// for players, these are view angles
	int model_idx = 0;
	int mainanim = 0;
	float mainanim_frame = 0.f;	// frames quantized to 16 bits
	int leganim = 0;
	float leganim_frame = 0.f;

	int legblend = 0;
	float legblendframe = 0.f;
	float legblendleft = 0.f;
	float legsblendtime = 0.f;
	int torsoblend = 0;
	float torsoblendframe = 0.f;
	float torsoblendleft = 0.f;
	float torsoblendtime = 0.f;

	short flags = 0;	// Entity_Flags
	int item = 0;
	int solid = 0;	// encodes physical object shape
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

// Player state replicated only to player's client for prediction
struct PlayerState
{
	glm::vec3 position=glm::vec3(0.f);
	glm::vec3 angles = glm::vec3(0.f);
	glm::vec3 velocity = glm::vec3(0.f);
	short state = 0;	// Player_Movement_State
	Item_State items;
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

	int owner_index = 0;
	int sub_type = 0;
	float death_time = 0.0;
	int health = 100;
	Item_State items;

	int item = 0;
	int solid = 0;

	float in_air_time = 0.f;

	void(*update)(Entity*) = nullptr;
	void(*damage)(Entity* me, Entity* attacker, int amount, int flags) = nullptr;


	Animator anim;
	const Model* model = nullptr;

	bool active() { return type != ET_FREE; }
	EntityState to_entity_state();
	void from_entity_state(EntityState& es);


	PlayerState ToPlayerState() const;
	void FromPlayerState(PlayerState* ps);

	void set_model(const char* model);
};

struct Net_Prop
{
	const char* name;
	int offset;
	int input_bits;
	int output_bits = -1; 
	float quantize = 1.f;
	short condition = 0;
	// -1 = same as input, vals above 65 are special types, 
	//if output is given in bits but input is float, output is quantized

	enum Conditions
	{
		ALL = 0,
		ONLY_PLAYER = 1,
		NOT_PLAYER = 2,
	};
};


enum Net_Prop_Special_Types
{
	NPROP_FLOAT = 65,
	NPROP_VEC3,
};


// in serverclmgr for now
bool WriteDeltaEntState(EntityState* from, EntityState* to, ByteWriter& msg);
void ReadDeltaEntState(EntityState* to, ByteReader& msg);
void ReadDeltaPState(PlayerState* to, ByteReader& msg);

void NetDebugPrintf(const char* fmt, ...);


#endif