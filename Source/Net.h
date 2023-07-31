#pragma once
#include "Socket.h"
#include "Bytepacker.h"
#include "Connection.h"
#include "MoveCommand.h"

const int SERVER_PORT = 24352;
const int MAX_CLIENTS = 16;
const int MAX_NET_STRING = 256;
const unsigned CONNECTIONLESS_SEQUENCE = 0xffffffff;
const int MAX_CONNECT_ATTEMPTS = 10;
const float CONNECT_RETRY_TIME = 2.f;
const double MAX_TIME_OUT = 200.f;
const int CLIENT_MOVE_HISTORY = 16;

// Messages
enum ServerToClient
{
	SvNop = 0,
	SvMessageInitial,	// first message to send back to client
	SvMessageSnapshot,	
	SvMessagePlayerState,
	SvMessageDisconnect,
	SvMessageText,
	SvMessageTick,
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

// State that is transmitted to clients
struct EntityState
{
	int type = 0;
	glm::vec3 position=glm::vec3(0.f);
	glm::vec3 angles=glm::vec3(0.f);	// for players, these are view angles
	bool ducking = false;
};
// State specific to the client's player that is transmitted
struct PlayerState
{
	glm::vec3 velocity;
};



void NetDebugPrintf(const char* fmt, ...);