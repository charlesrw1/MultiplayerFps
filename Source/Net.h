#pragma once
#include "Socket.h"
#include "Serialize.h"
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

enum InitialMessageTypes
{
	Msg_ConnectRequest = 'c',
	Msg_AcceptConnection = 'a',
	Msg_RejectConnection = 'r'
};

// Connection initilization
// client sends "connect" msg until given a response or times out
// server sends back "accepted" or "rejected"
// Now communication happens through the sequenced 'Connection' class
// client sends "init" cmd
// server sends back inital server data to client
// client then sends "spawn" cmd and server sends regular snapshots

class Server
{
public:
	void Start();
	void Quit();
	void ChangeLevel(const char* map);
	void ReadPackets();
	void SendSnapshots();


	bool active = false;
private:
	struct RemoteClient {
		enum ConnectionState {
			Dead,		// unused slot
			Connected,	// connected and sending initial state
			Spawned		// spawned and sending snapshots
		};
		ConnectionState state=Dead;
		Connection connection;
	};

	int GetClientIndex(RemoteClient& client) const;

	int FindClient(const IPAndPort& addr) const;

	void SendInitData(RemoteClient& client);

	void ReadClientText(RemoteClient& client, ByteReader& buf);
	void ReadClientInput(RemoteClient& client, ByteReader& buf);

	void HandleUnknownPacket(const IPAndPort& addr, ByteReader& buf);
	void ReadClientPacket(RemoteClient& client, ByteReader& buf);
	void ConnectNewClient(const IPAndPort& addr, ByteReader& buf);
	void DisconnectClient(RemoteClient& client);

	Socket socket;
	std::vector<RemoteClient> clients;
};

class Client
{
public:
	enum ConnectionState {
		Disconnected,
		TryingConnect,	// trying to connect to server
		Connected,		// connected and receiving inital state
		Spawned,		// in server as normal
	};

	void Start();
	void Quit();
	void ReadPackets();
	void SendCommands();

	void HandleUnknownPacket(const IPAndPort& addr, ByteReader& buf);
	void ReadServerPacket(ByteReader& buf);
	void Disconnect();

	// Message handlers
	void ReadInitData(ByteReader& buf);

	void Connect(const IPAndPort& who);
	void TrySendingConnect();
	void CheckTimeout();
	void DoConnect();

	int connect_attempts = 0;
	double attempt_time = 0.f;

	int GetOutSequence() const { return server.out_sequence; }
	MoveCommand commands[CLIENT_MOVE_HISTORY];
	glm::vec3 view_angles=glm::vec3(0.f);		// local view angles

	bool initialized = false;
	int player_num = -1;	// what index are we
	ConnectionState state=Disconnected;
	Socket socket;
	Connection server;
};

void NetDebugPrintf(const char* fmt, ...);