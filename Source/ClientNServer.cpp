#include "ClientNServer.h"
#include "Util.h"
#include <cstdarg>

static void NetDebugPrintf(const char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	
	char buffer[2048];
	float time_since_start = TimeSinceStart();
	int l = sprintf_s(buffer, "%8.2f: ", time_since_start);
	vsnprintf(buffer + l, 2048 - l, fmt, ap);
	printf(buffer);
	
	va_end(ap);
}

#define DebugOut(fmt, ...) NetDebugPrintf(fmt, __VA_ARGS__)

void Server::Start()
{
	printf("Starting server...\n");
	socket.Init(SERVER_PORT);
	clients.resize(MAX_CLIENTS);
	initialized = true;
}
void Server::Quit()
{
	// Alert clients
	socket.Shutdown();
	initialized = false;
}
int Server::FindClient(const IPAndPort& addr) const
{
	for (int i = 0; i < clients.size(); i++) {
		if (clients[i].state != RemoteClient::Dead && clients[i].connection.remote_addr == addr)
			return i;
	}
	return -1;
}

static void RejectConnectRequest(Socket* sock, IPAndPort addr, const char* why)
{
	uint8_t buffer[512];
	ByteWriter writer(buffer, 512);
	writer.WriteDword(CONNECTIONLESS_SEQUENCE);
	writer.WriteByte(Msg_RejectConnection);
	int len = strlen(why);
	if (len >= 256) {
		ASSERT(0);
		return;
	}
	writer.WriteByte(len);
	for (int i = 0; i < len; i++)
		writer.WriteByte(why[i]);
	int pad_bytes = 8 - writer.BytesWritten();
	for (int i = 0; i < pad_bytes; i++)
		writer.WriteByte(0);
	sock->Send(buffer, writer.BytesWritten(), addr);
}

void Server::HandleUnknownPacket(const IPAndPort& addr, ByteReader& buf)
{
	DebugOut("Connectionless packet recieved from %s\n", addr.ToString().c_str());
	uint8_t command = buf.ReadByte();
	if (command == Msg_ConnectRequest) {
		ConnectNewClient(addr, buf);
	}
	else {
		DebugOut("Unknown connectionless packet\n");
	}
}
void Server::ConnectNewClient(const IPAndPort& addr, ByteReader& buf)
{
	int spot = 0;
	bool already_connected = false;
	for (; spot < clients.size(); spot++) {
		// if the "accept" response was dropped, client might send a connect again
		if (clients[spot].state == RemoteClient::Connected && clients[spot].connection.remote_addr == addr) {
			already_connected = true;
			break;
		}
		if (clients[spot].state == RemoteClient::Dead)
			break;
	}
	if (spot == clients.size()) {
		RejectConnectRequest(&socket, addr, "Server is full");
		return;
	}
	int name_len = buf.ReadByte();
	char name_str[256 + 1];
	for (int i = 0; i < name_len; i++) {
		name_str[i] = buf.ReadByte();
	}
	name_str[name_len] = 0;
	if(!already_connected)
		DebugOut("Connected new client, %s; IP: %s\n", name_str, addr.ToString().c_str());

	uint8_t accept_buf[32];
	ByteWriter writer(accept_buf, 32);
	writer.WriteDword(CONNECTIONLESS_SEQUENCE);
	writer.WriteByte(Msg_AcceptConnection);
	int pad_bytes = 8 - writer.BytesWritten();
	for (int i = 0; i < pad_bytes; i++)
		writer.WriteByte(0);
	socket.Send(accept_buf, writer.BytesWritten(), addr);

	RemoteClient& new_client = clients[spot];
	new_client.state = RemoteClient::Connected;
	new_client.connection.Init(&socket, addr);
	// Further communication is done with sequenced packets
}

void Server::DisconnectClient(RemoteClient& client)
{
	ASSERT(client.state != RemoteClient::Dead);
	DebugOut("Disconnecting client %s\n", client.connection.remote_addr.ToString().c_str());
	if (client.state == RemoteClient::Spawned) {
		// remove entitiy from game, call game logic ...
	}
	client.state = RemoteClient::Dead;
	client.connection.Clear();
}

void Server::ReadClientCommand(RemoteClient& client, ByteReader& buf)
{
	uint8_t cmd_len = buf.ReadByte();
	std::string cmd;
	cmd.resize(cmd_len);
	buf.ReadBytes((uint8_t*)&cmd[0], cmd_len);
	if (buf.HasFailed())
		DisconnectClient(client);
	if (cmd == "init") {
		SendInitData(client);
	}
}

// Send initial data to the client such as the map, client index, so on
void Server::SendInitData(RemoteClient& client)
{
	if (client.state != RemoteClient::Connected)
		return;

	uint8_t buffer[MAX_PAYLOAD_SIZE];
	ByteWriter writer(buffer, MAX_PAYLOAD_SIZE);
	writer.WriteByte(SvMessageInitial);
	int client_index = (&client) - clients.data();
	writer.WriteByte(client_index);
	const char* map_name = "world0.glb";
	writer.WriteByte(strlen(map_name));
	writer.WriteBytes((uint8_t*)map_name, strlen(map_name));

	// FIXME: must be reliable!
	client.connection.Send(buffer, writer.BytesWritten());
}


void Server::ReadClientPacket(RemoteClient& client, ByteReader& buf)
{
	while (!buf.IsEof())
	{
		uint8_t command = buf.ReadByte();
		if (buf.HasFailed()) {
			DebugOut("Read fail\n");
			DisconnectClient(client);
			return;
		}
		switch (command)
		{
		case ClNop:
			break;
		case ClMessageInput:
			break;
		case ClMessageQuit:
			DisconnectClient(client);
			break;
		case ClMessageText:
			break;
		case ClMessageTick:
			break;
		case ClMessageCommand:
			ReadClientCommand(client, buf);
			break;
		default:
			DebugOut("Unknown message\n");
			DisconnectClient(client);
			return;
			break;
		}
	}
}
void Server::ReadPackets()
{
	ASSERT(initialized);
	uint8_t inbuffer[MAX_PAYLOAD_SIZE + PACKET_HEADER_SIZE];
	size_t recv_len = 0;
	IPAndPort from;

	while (socket.Recieve(inbuffer, sizeof(inbuffer), recv_len, from))
	{
		if (recv_len < PACKET_HEADER_SIZE)
			continue;
		if (*(uint32_t*)inbuffer == CONNECTIONLESS_SEQUENCE) {
			ByteReader reader(inbuffer + 4, recv_len - 4);
			HandleUnknownPacket(from, reader);
			continue;
		}

		int cl_index = FindClient(from);
		if (cl_index == -1) {
			DebugOut("Packet from unknown source: %s\n", from.ToString().c_str());
			continue;
		}

		RemoteClient& client = clients.at(cl_index);
		// handle packet sequencing
		int header = client.connection.NewPacket(inbuffer, recv_len);
		if (header != -1) {
			ByteReader reader(inbuffer + header, recv_len - header);
			ReadClientPacket(client, reader);
		}
	}

	// check timeouts
	for (int i = 0; i < clients.size(); i++) {
		auto& cl = clients[i];
		if (cl.state != RemoteClient::Spawned && cl.state != RemoteClient::Connected)
			continue;
		if (GetTime() - cl.connection.last_recieved > MAX_TIME_OUT) {
			printf("Client, %s, timed out\n", cl.connection.remote_addr.ToString().c_str());
			DisconnectClient(cl);
		}
	}
}

void Client::Start()
{
	printf("Starting client...\n");
	socket.Init(0);
	initialized = true;
}
void Client::Connect(const IPAndPort& where)
{
	// if connected, disconnect
	printf("Connecting to server: %s\n", where.ToString().c_str());
	server.remote_addr = where;
	connect_attempts = 0;
	attempt_time = -1000.f;
	state = TryingConnect;
	TrySendingConnect();
}
void Client::TrySendingConnect()
{
	if (state != TryingConnect)
		return;
	if (connect_attempts >= MAX_CONNECT_ATTEMPTS) {
		DebugOut("Unable to connect to server\n");
		state = Disconnected;
		return;
	}
	double delta = GetTime() - attempt_time;
	if (delta < CONNECT_RETRY_TIME)
		return;
	attempt_time = GetTime();
	connect_attempts++;
	DebugOut("Sending connection request\n");

	uint8_t buffer[256];
	ByteWriter writer(buffer, 256);
	writer.WriteDword(CONNECTIONLESS_SEQUENCE);
	writer.WriteByte(Msg_ConnectRequest);
	const char name[] = "Charlie";
	writer.WriteByte(sizeof(name) - 1);
	writer.WriteBytes((uint8_t*)name, sizeof(name) - 1);

	socket.Send(buffer, writer.BytesWritten(), server.remote_addr);
}

void Client::ReadPackets()
{
	ASSERT(initialized);
	uint8_t inbuffer[MAX_PAYLOAD_SIZE + PACKET_HEADER_SIZE];
	size_t recv_len = 0;
	IPAndPort from;

	while (socket.Recieve(inbuffer, sizeof(inbuffer), recv_len, from))
	{
		if (recv_len < PACKET_HEADER_SIZE)
			continue;
		if (*(uint32_t*)inbuffer == CONNECTIONLESS_SEQUENCE) {
			ByteReader reader(inbuffer + 4, recv_len - 4);
			HandleUnknownPacket(from, reader);
			continue;
		}

		if ((state==Connected||state==Spawned) && from == server.remote_addr) {
			int header = server.NewPacket(inbuffer, recv_len);
			if (header != -1) {
				ByteReader reader(inbuffer + header, recv_len - header);
				ReadServerPacket(reader);
			}
		}
	}

	if (state == Connected || state == Spawned) {
		if (GetTime() - server.last_recieved > MAX_TIME_OUT) {
			printf("Server timed out\n");
			Disconnect();
		}
	}

}

void Client::Disconnect()
{
	if (state == Disconnected)
		return;
	if (state!=TryingConnect) {
		uint8_t buffer[8];
		ByteWriter write(buffer, 8);
		write.WriteByte(ClMessageQuit);
		server.Send(buffer, write.BytesWritten());
	}


	state = Disconnected;
}

void Client::HandleUnknownPacket(const IPAndPort& addr, ByteReader& buf)
{
	uint8_t type = buf.ReadByte();
	if (type == Msg_AcceptConnection) {
		if (state != TryingConnect)
			return;
		DoConnect();
	}
	else if (type == Msg_RejectConnection) {
		if (state != TryingConnect)
			return;

		uint8_t len = buf.ReadByte();
		char reason[256 + 1];
		for (int i = 0; i < len; i++) {
			reason[i] = buf.ReadByte();
		}
		reason[len] = '\0';
		if(!buf.HasFailed())
			printf("Connection rejected: %s\n", reason);
		Disconnect();
	}
	else {
		DebugOut("Unknown connectionless packet type\n");
	}
}

void Client::DoConnect()
{
	// We now have a valid connection with the server
	server.Init(&socket, server.remote_addr);
	state = Connected;
	// Request the next step in the setup
	uint8_t buffer[256];
	ByteWriter writer(buffer, 256);
	writer.WriteByte(ClMessageCommand);
	const char init_str[] = "init";
	writer.WriteByte(sizeof(init_str) - 1);
	writer.WriteBytes((uint8_t*)init_str, sizeof(init_str) - 1);
	int pad_bytes = 8 - writer.BytesWritten();
	for (int i = 0; i < pad_bytes; i++)
		writer.WriteByte(0);

	// FIXME!!: this must go through reliable, do this later
	server.Send(buffer, writer.BytesWritten());
}

void Client::ReadInitData(ByteReader& buf)
{
	player_num = buf.ReadByte();
	int map_len = buf.ReadByte();
	std::string mapname(map_len,0);
	buf.ReadBytes((uint8_t*)&mapname[0], map_len);
	printf("Player num: %d, Map: %s\n", player_num, mapname.c_str());
}

void Client::ReadServerPacket(ByteReader& buf)
{
	while (!buf.IsEof())
	{
		uint8_t command = buf.ReadByte();
		if (buf.HasFailed()) {
			Disconnect();
			return;
		}
		switch (command)
		{
		case SvNop:
			break;
		case SvMessageInitial:
			ReadInitData(buf);
			break;
		case SvMessageSnapshot:
			break;
		case SvMessageDisconnect:
			break;
		case SvMessageText:
			break;
		case SvMessageTick:
			break;
		default:
			DebugOut("Unknown message, disconnecting\n");
			Disconnect();
			return;
			break;
		}
	}
}