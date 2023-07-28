#pragma once
#include "Socket.h"
#include "Serialize.h"
#include "Connection.h"

const int SERVER_PORT = 24352;
const int MAX_CLIENTS = 16;
const int MAX_NET_STRING = 256;
const unsigned CONNECTIONLESS_SEQUENCE = 0xffffffff;
const int MAX_CONNECT_ATTEMPTS = 10;
const float CONNECT_RETRY_TIME = 2.f;

// Messages
enum ServerToClient
{
	SvMessageStart,
	SvMessageSnapshot,
	SvMessageDisconnect,
	SvMessageText,
	SvMessageTick,
};
enum ClientToServer
{
	ClMessageInput,
	ClMessageQuit,
	ClMessageText,
	ClMessageTick,
};

enum SharedMessages
{
	NetMessageEnd = 0xFF,
};

class Server
{
public:
	struct RemoteClient {
		enum ConnectionState {
			Dead,
			Connecting,
			Connected,
			Linger,
		};
		ConnectionState state;
		Connection connection;
	};
	void Start();
	void Quit();
	void ChangeLevel(const char* map);
	void ReadMessages();

	int FindClient(const IPAndPort& addr) const;
	void HandleUnknownPacket(const IPAndPort& addr, ByteReader& buf);
	void HandlePacket(RemoteClient& client, ByteReader& buf);
	void ConnectNewClient(const IPAndPort& addr);
	void DisconnectClient(RemoteClient& client);

	bool active = false;
	Socket socket;
	std::vector<RemoteClient> clients;
};

class Client
{
public:
	enum ConnectionState {
		Disconnected,
		TryingConnect,
		Connecting,
		Connected,
	};

	void Start();
	void Quit();
	void ReadMessages();

	void HandlePacket(ByteReader& buf);
	void HandleUnknownPacket(const IPAndPort& addr, ByteReader& buf);
	void Disconnect();

	void Connect(const IPAndPort& who);
	void TrySendingConnect();
	int connect_attempts = 0;
	double attempt_time = 0.f;

	bool active = false;
	ConnectionState state=Disconnected;
	Socket socket;
	Connection server;
};