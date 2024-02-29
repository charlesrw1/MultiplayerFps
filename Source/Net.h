#ifndef NET_H
#define NET_H
#include "Util.h"
#include "Bytepacker.h"
#include "Connection.h"
#include "Animation.h"

const int CLIENT_SNAPSHOT_HISTORY = 16;	// buffer last 16 snapshots

const int MAX_PAYLOAD_SIZE = 1400;
const int PACKET_HEADER_SIZE = 8;

const int DEFAULT_SERVER_PORT = 47000;

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

class Entity;
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