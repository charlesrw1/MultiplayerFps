#include "Server.h"
#include "Util.h"
#include "CoreTypes.h"
#include <cstdarg>

Server server;

void NetDebugPrintf(const char* fmt, ...)
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

#define DebugOut(fmt, ...) NetDebugPrintf("server: " fmt, __VA_ARGS__)

void Server::Init()
{
	printf("initializing server\n");
	client_mgr.Init();
	sv_game.Init();
}
void Server::End()
{
	if (!IsActive())
		return;
	DebugOut("ending server\n");
	client_mgr.ShutdownServer();
	sv_game.ClearState();
	active = false;
	tick = 0;
	time = 0.0;
	map_name = {};
}
void Server::Spawn(const char* mapname)
{
	if (IsActive()) {
		End();
	}
	tick = 0;
	time = 0.0;
	DebugOut("spawning with map %s\n", mapname);
	bool good = sv_game.DoNewMap(mapname);
	if (!good)
		return;
	map_name = mapname;
	active = true;
}
bool Server::IsActive() const
{
	return active;
}

ClientMgr::ClientMgr() //: sock_emulator(&socket)
{

}

void ClientMgr::Init()
{
	socket.Init(SERVER_PORT);
	clients.resize(MAX_CLIENTS);
}

int ClientMgr::FindClient(IPAndPort addr) const
{
	for (int i = 0; i < clients.size(); i++) {
		if (clients[i].state != RemoteClient::Dead && clients[i].connection.remote_addr == addr)
			return i;
	}
	return -1;
}
int ClientMgr::GetClientIndex(const RemoteClient& client) const
{
	return (&client) - clients.data();
}

static void RejectConnectRequest(Socket* sock, IPAndPort addr, const char* why)
{
	uint8_t buffer[512];
	ByteWriter writer(buffer, 512);
	writer.WriteLong(CONNECTIONLESS_SEQUENCE);
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

void ClientMgr::HandleUnknownPacket(IPAndPort addr, ByteReader& buf)
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
void ClientMgr::ConnectNewClient(IPAndPort addr, ByteReader& buf)
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
	writer.WriteLong(CONNECTIONLESS_SEQUENCE);
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

void ClientMgr::DisconnectClient(RemoteClient& client)
{
	ASSERT(client.state != RemoteClient::Dead);
	DebugOut("Disconnecting client %s\n", client.connection.remote_addr.ToString().c_str());
	if (client.state == RemoteClient::Spawned) {
		server.sv_game.OnClientLeave(GetClientIndex(client));
		// remove entitiy from game, call game logic ...
	}
	uint8_t buffer[8];
	ByteWriter writer(buffer, 8);
	writer.WriteByte(SvMessageDisconnect);
	client.connection.Send(buffer, writer.BytesWritten());

	client.state = RemoteClient::Dead;
	client.connection.Clear();

}

void ClientMgr::ParseClientText(RemoteClient& client, ByteReader& buf)
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
	else if (cmd == "spawn") {
		if (client.state != RemoteClient::Spawned) {
			client.state = RemoteClient::Spawned;

			server.sv_game.SpawnNewClient(GetClientIndex(client));

			DebugOut("Spawned client, %s\n", client.connection.remote_addr.ToString().c_str());
		}
	}
}

void ClientMgr::ParseClientMove(RemoteClient& client, ByteReader& buf)
{
	//DebugOut("Recieved client input from %s\n", client.connection.remote_addr.ToString().c_str());

	MoveCommand cmd{};
	cmd.forward_move = buf.ReadFloat();
	cmd.lateral_move = buf.ReadFloat();
	cmd.up_move = buf.ReadFloat();
	cmd.view_angles.x = buf.ReadFloat();
	cmd.view_angles.y = buf.ReadFloat();
	cmd.button_mask = buf.ReadLong();

	// Not ready to run the input yet
	if (client.state != RemoteClient::Spawned)
		return;

	Entity* ent = ServerEntForIndex(GetClientIndex(client));
	ASSERT(ent && ent->type == Ent_Player);
	server.sv_game.ExecutePlayerMove(ent, cmd);
}

// Send initial data to the client such as the map, client index, so on
void ClientMgr::SendInitData(RemoteClient& client)
{
	if (client.state != RemoteClient::Connected)
		return;

	uint8_t buffer[MAX_PAYLOAD_SIZE];
	ByteWriter writer(buffer, MAX_PAYLOAD_SIZE);
	writer.WriteByte(SvMessageInitial);
	int client_index = (&client) - clients.data();
	writer.WriteByte(client_index);
	writer.WriteByte(server.map_name.size());
	writer.WriteBytes((uint8_t*)server.map_name.data(), server.map_name.size());

	// FIXME: must be reliable!
	client.connection.Send(buffer, writer.BytesWritten());
}


void ClientMgr::HandleClientPacket(RemoteClient& client, ByteReader& buf)
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
			ParseClientMove(client, buf);
			break;
		case ClMessageQuit:
			DisconnectClient(client);
			break;
		case ClMessageTick:
			break;
		case ClMessageText:
			ParseClientText(client, buf);
			break;
		default:
			DebugOut("Unknown message\n");
			DisconnectClient(client);
			return;
			break;
		}
	}
}

void ClientMgr::ReadPackets()
{
	uint8_t inbuffer[MAX_PAYLOAD_SIZE + PACKET_HEADER_SIZE];
	size_t recv_len = 0;
	IPAndPort from;

	while (socket.Receive(inbuffer, sizeof(inbuffer), recv_len, from))
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
			HandleClientPacket(client, reader);
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

void ClientMgr::SendSnapshots()
{
	for (int i = 0; i < MAX_CLIENTS; i++) {
		if (clients[i].state < RemoteClient::Connected)
			continue;

		if (clients[i].state == RemoteClient::Spawned)
			SendSnapshotUpdate(clients[i]);
		else
			clients[i].connection.Send(nullptr, 0);
	}
}

void ClientMgr::SendSnapshotUpdate(RemoteClient& client)
{
	ASSERT(client.state == RemoteClient::Spawned);

	Game& game = server.sv_game;

	uint8_t buffer[MAX_PAYLOAD_SIZE];
	ByteWriter writer(buffer, MAX_PAYLOAD_SIZE);
	writer.WriteByte(SvMessageTick);
	writer.WriteLong(server.tick);
	writer.WriteByte(SvMessageSnapshot);
	// for now, just dump the entities
	// entity states
	for (int i = 0; i < 8; i++) {
		Entity& ent = game.ents[i];
		int index = i;
	
		writer.WriteWord(index);
		//if (ent.type == Ent_Free)
		//	continue;

		writer.WriteByte(ent.type);
		writer.WriteFloat(ent.position.x);
		writer.WriteFloat(ent.position.y);
		writer.WriteFloat(ent.position.z);
		writer.WriteByte(ent.ducking);
	}
	// local player specific state
	Entity& ent = game.ents[GetClientIndex(client)];
	writer.WriteByte(SvMessagePlayerState);
	writer.WriteFloat(ent.velocity.x);
	writer.WriteFloat(ent.velocity.y);
	writer.WriteFloat(ent.velocity.z);
	writer.WriteByte(ent.on_ground);

	client.connection.Send(buffer, writer.BytesWritten());
	//DebugOut("sent %d bytes to %s\n", writer.BytesWritten(), client.connection.remote_addr.ToString().c_str());
}

void ClientMgr::ShutdownServer()
{
	for (int i = 0; i < clients.size(); i++) {
		if (clients[i].state >= RemoteClient::Connected)
			DisconnectClient(clients[i]);
	}
	//socket.Shutdown();
}