#ifndef NET_H
#define NET_H
#include "Framework/Util.h"
#include "Connection.h"
#include "Animation/Runtime/Animation.h"

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

		PLAYER_PROP_MASK = PLAYER_PROP | DEFAULT_PROP,
		NON_PLAYER_PROP_MASK = NON_PLAYER_PROP | DEFAULT_PROP,
	};
};

typedef uint32_t entityguid;
typedef uint32_t entitytype;

struct Obj_Net_State
{
	/* filled out by system */
	entityguid handle;
	entitytype type;

	/* shared props */
	glm::vec3 pos;
	glm::vec3 rot;
	int model;
	int item;
	int state;
	int flags;
	/* player props below */
	glm::vec3 vel;
	int clip;
	int ammo;
};

struct Frame2 {
	int start=0;
	int count=0;
	int tick=0;
	int localplayerindex = 0;
};

class Frame_Storage
{
public:
	Frame_Storage(int num_obj_size, int num_frames) 
		: objs(num_obj_size), frames(num_frames) {
	}
	Frame2& get_frame(int index_thats_moduloed) {
		return frames.at(index_thats_moduloed % frames.size());
	}
	Frame2* get_frame_for_tick(int tick) {
		for (int i = 0; i < frames.size(); i++) 
			if (frames[i].tick == tick) 
				return &frames[i];
		return nullptr;
	}
	void allocate_space(Frame2& frame, int obj_count) {
		frame.start = objhead;
		frame.count = obj_count;
		int last = objhead;
		objhead = (objhead + obj_count) % objs.size();
		bool wraparound = objhead < frame.start;
		for (int i = 0; i < frames.size(); i++) {
			if (&frames[i] != &frame &&
				(frames[i].start >= frame.start || frames[i].start < objhead)) {
				frames[i].tick = frames[i].localplayerindex = frames[i].start = -1;
				frames[i].count = 0;
			}
		}
	}
	Obj_Net_State& get_state(const Frame2& frame, int index) {
		assert(index < frame.count);
		return objs.at((frame.start + index) % objs.size());
	}
private:
	std::vector<Obj_Net_State> objs;
	std::vector<Frame2> frames;
	int frameidx = 0;
	int objhead = 0;
};

void new_entity_fields_test();

// Network serialization functions

class Entity;

class ByteWriter;
class ByteReader;

void write_full_entity(Entity* e, ByteWriter& msg);
void read_entity(Entity* e, ByteReader& msg, int condition, bool is_delta);
void set_entity_props_from_entity(Entity* from, Entity* to, int condition);
void write_delta_entity(ByteWriter& msg, ByteReader& s0, ByteReader& s1, int condition);

void write_delta_entity(const Obj_Net_State& from, const Obj_Net_State& to, ByteWriter& msg, int condition);
void read_delta_entity(Obj_Net_State& to, ByteReader& reader, int condition);


#endif