#ifndef NET_H
#define NET_H

#include "Bytepacker.h"
#include "Connection.h"
#include "MoveCommand.h"
#include <queue>

const int CLIENT_SNAPSHOT_HISTORY = 16;	// buffer last 16 snapshots

const int MAX_PAYLOAD_SIZE = 1400;
const int PACKET_HEADER_SIZE = 8;

const int DEFAULT_SERVER_PORT = 24352;

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
enum ServerToClient
{
	SvNop = 0,
	SvMessageInitial,	// first message to send back to client
	SvMessageTick,
	SvMessageSnapshot,	
	SvMessagePlayerState,
	SvMessageDisconnect,
	SvMessageText,
};
enum ClientToServer
{
	ClNop = 0,
	ClMessageInput,
	ClMessageQuit,
	ClMessageText,
	ClMessageDelta,
	ClMessageSetBaseline,
};

// Connection initilization
// client sends "connect" msg until given a response or times out
// server sends back "accepted" or "rejected"
// Now communication happens through the sequenced 'Connection' class
// client sends "init" cmd
// server sends back inital server data to client
// client then sends "spawn" cmd and server sends regular snapshots
enum InitialMessageTypes
{
	Msg_ConnectRequest = 'c',
	Msg_AcceptConnection = 'a',
	Msg_RejectConnection = 'r'
};

enum EntType
{
	Ent_Player,
	Ent_Item,
	Ent_Grenade,
	Ent_Dummy,
	Ent_Free = 0xff,
};

// General state that is transmitted to clients
struct EntityState
{
	int type = Ent_Free;
	glm::vec3 position=glm::vec3(0.f);
	glm::vec3 angles=glm::vec3(0.f);	// for players, these are view angles

	int model_idx = 0;
	int mainanim = 0;
	float mainanim_frame = 0.f;	// frames quantized to 16 bits
	int leganim = 0;
	float leganim_frame = 0.f;


	int item = 0;	// FIXME: haven't added it to state

	bool ducking = false;
};


enum ItemUseState
{
	Item_Idle,
	Item_InFire,
	Item_InSecFire,
	Item_Reload,
	Item_Scoped,
	Item_Raising,
	Item_Lowering,
};

struct ItemState
{
	ItemState() {
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

	bool reloading = false;
	float gun_timer = 0.f;
	ItemUseState state = Item_Idle;
};

// State specific to the client's player that is transmitted
struct PlayerState
{
	glm::vec3 position=glm::vec3(0.f);
	glm::vec3 angles = glm::vec3(0.f);
	glm::vec3 velocity = glm::vec3(0.f);
	
	bool on_ground = false;
	bool ducking = false;
	bool alive = false;
	bool in_jump = false;

	ItemState items;
};

// taken from quake 3, a nice idea
enum EntityEvent
{
	Ev_FirePrimary,
	Ev_FireSecondary,
	Ev_Reload,
	Ev_Jump,
	Ev_HardLanding,

	Ev_Footstep,

	Ev_Hurt,
	Ev_Death,

	Ev_Sound,
	Ev_Explode
};

struct GameEvent
{
	const static int MAX_PARAMS = 32;
	uint8_t num_params = 0;
	uint8_t params[MAX_PARAMS];
};


// in serverclmgr for now
bool WriteDeltaEntState(EntityState* from, EntityState* to, ByteWriter& msg);
void ReadDeltaEntState(EntityState* to, ByteReader& msg);
void ReadDeltaPState(PlayerState* to, ByteReader& msg);

void NetDebugPrintf(const char* fmt, ...);


#endif