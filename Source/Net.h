#ifndef NET_H
#define NET_H

#include "Socket.h"
#include "Bytepacker.h"
#include "Connection.h"
#include "MoveCommand.h"
#include <queue>

const int CLIENT_SNAPSHOT_HISTORY = 16;	// buffer last 16 snapshots

const int MAX_PAYLOAD_SIZE = 1400;
const int PACKET_HEADER_SIZE = 8;

const int SERVER_PORT = 24352;
const int MAX_CLIENTS = 16;
const int MAX_NET_STRING = 256;
const unsigned CONNECTIONLESS_SEQUENCE = 0xffffffff;
const int MAX_CONNECT_ATTEMPTS = 10;
const float CONNECT_RETRY_TIME = 2.f;
const double MAX_TIME_OUT = 5.f;
const int CLIENT_MOVE_HISTORY = 36;

const double DEFAULT_UPDATE_RATE = 66.66;	// server+client ticks 66 times a second
const int DEFAULT_MOVECMD_RATE = 60;	// send inputs (multiple) 60 times a second
const int DEFAULT_SNAPSHOT_RATE = 30;	// send 30 snapshots a second

const double DEFAULT_INTERP_TIME = 0.1;	// how far in the past to interpolate for lag compensation


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
	ClMessageTick,
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
	Ent_Dummy,
	Ent_Free = 0xff,
};

// State that is transmitted to clients
struct EntityState
{
	int type = Ent_Free;
	glm::vec3 position=glm::vec3(0.f);
	glm::vec3 angles=glm::vec3(0.f);	// for players, these are view angles
	bool ducking = false;
};
// State specific to the client's player that is transmitted
struct PlayerState
{
	glm::vec3 velocity;
	bool on_ground = false;
};

void NetDebugPrintf(const char* fmt, ...);


#endif