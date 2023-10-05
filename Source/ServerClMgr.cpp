#include "Server.h"
#include "CoreTypes.h"
#define DebugOut(fmt, ...) NetDebugPrintf("server: " fmt, __VA_ARGS__)
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
	if (!already_connected)
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
	cmd.tick = buf.ReadLong();
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

		clients[i].next_snapshot_time -= core.tick_interval;
		if (clients[i].state == RemoteClient::Spawned && clients[i].next_snapshot_time < 0.f) {
			SendSnapshotUpdate(clients[i]);
			clients[i].next_snapshot_time += (1 / 30.0);	// 20hz update rate
		}
		else
			clients[i].connection.Send(nullptr, 0);	// update reliable
	}
}
// horrid
void WriteDeltaEntState(EntityState* from, EntityState* to, ByteWriter& msg, uint8_t index)
{
	uint16_t mask = 0;
	if (from->type != to->type) {
		mask |= 1;
	}
	if (from->position != to->position) {
		mask |= (1 << 1);
	}
	if (from->angles != to->angles) {
		mask |= (1 << 2);
	}
	
	if (from->leganim != to->leganim) {
		mask |= 1 << 7;
	}
	if (from->leganim_frame != to->leganim_frame) {
		mask |= 1 << 8;
	}
	if (from->ducking != to->ducking) {
		mask |= 1 << 9;
	}
	if (from->mainanim != to->mainanim) {
		mask |= 1 << 10;
	}
	if (from->mainanim_frame != to->mainanim_frame) {
		mask |= 1 << 11;
	}

	if (mask == 0)
		return;

	msg.WriteByte(index);
	msg.WriteWord(mask);

	if (mask & 1)
		msg.WriteByte(to->type);
	if (mask & (1 << 1)) {
		msg.WriteFloat(to->position.x);
		msg.WriteFloat(to->position.y);
		msg.WriteFloat(to->position.z);
	}
	if (mask & (1 << 2)) {
		char rot[3];
		rot[0] = to->angles.x / (2 * PI) * 256;
		rot[1] = to->angles.y / (2 * PI) * 256;
		rot[2] = to->angles.z / (2 * PI) * 256;
		msg.WriteByte(rot[0]);
		msg.WriteByte(rot[1]);
		msg.WriteByte(rot[2]);
	}
	if (mask & (1 << 7))
		msg.WriteWord(to->leganim);
	if (mask & (1 << 8)) {
		int quantized_frame = to->leganim_frame * 100;
		msg.WriteWord(quantized_frame);
	}
	if (mask & (1 << 9))
		msg.WriteByte(to->ducking);
	if (mask & (1 << 10))
		msg.WriteWord(to->mainanim);
	if (mask & (1 << 11)) {
		int quantized_frame = to->mainanim_frame * 100;
		msg.WriteWord(quantized_frame);
	}
	
}

void ReadDeltaEntState(EntityState* to, ByteReader& msg)
{
	uint16_t mask = msg.ReadWord();

	static uint16_t last_mask = 0;
	if (last_mask != mask && to->type == Ent_Player) {
		printf("mask: %d\n", mask);
		last_mask = mask;
	}	
	if (mask & 1)
		to->type = msg.ReadByte();
	if (mask & (1 << 1)) {
		to->position.x = msg.ReadFloat();
		to->position.y = msg.ReadFloat();
		to->position.z = msg.ReadFloat();
	}
	if (mask & (1 << 2)) {
		char rot[3];
		rot[0] = msg.ReadByte();
		rot[1] = msg.ReadByte();
		rot[2] = msg.ReadByte();
		to->angles.x = (float)rot[0] * (2 * PI) / 256.0;
		to->angles.y = (float)rot[1] * (2 * PI) / 256.0;
		to->angles.z = (float)rot[2] * (2 * PI) / 256.0;
	}
	if (mask & (1 << 7))
		to->leganim = msg.ReadWord();
	if (mask & (1 << 8)) {
		int quantized_frame = msg.ReadWord();
		to->leganim_frame = quantized_frame / 100.0;
	}
	if (mask & (1 << 9))
		to->ducking = msg.ReadByte();
	if (mask & (1 << 10))
		to->mainanim = msg.ReadWord();
	if (mask & (1 << 11)) {
		int quantized_frame = msg.ReadWord();
		to->mainanim_frame = quantized_frame / 100.0;
	}
}

void ReadDeltaPState(PlayerState* to, ByteReader& msg)
{
	uint8_t mask = msg.ReadByte();
	if (mask & 1) {
		to->position.x = msg.ReadFloat();
		to->position.y = msg.ReadFloat();
		to->position.z= msg.ReadFloat();

	}
	if (mask & (1 << 1)) {
		to->angles.x = msg.ReadFloat();
		to->angles.y = msg.ReadFloat();
		to->angles.z = msg.ReadFloat();
	}
	if (mask & (1 << 2)) {
		to->ducking = msg.ReadByte();
	}
	if (mask & (1 << 3)) {
		to->on_ground = msg.ReadByte();
	}
	if (mask & (1 << 4)) {
		to->velocity.x = msg.ReadFloat();
		to->velocity.y = msg.ReadFloat();
		to->velocity.z = msg.ReadFloat();
	}

}

void WriteDeltaPState(PlayerState* from, PlayerState* to, ByteWriter& msg)
{
	uint8_t mask = 0;
	if (from->position != to->position) {
		mask |= 1;
	}
	if (from->angles != to->angles) {
		mask |= (1 << 1);
	}
	if (from->ducking != to->ducking) {
		mask |= (1 << 2);
	}
	if (from->on_ground != to->on_ground) {
		mask |= (1 << 3);
	}
	if (from->velocity != to->velocity) {
		mask |= (1 << 4);
	}

	msg.WriteByte(mask);
	if (mask & 1) {
		msg.WriteFloat(to->position.x);
		msg.WriteFloat(to->position.y);
		msg.WriteFloat(to->position.z);
	}
	if (mask & (1 << 1)) {
		msg.WriteFloat(to->angles.x);
		msg.WriteFloat(to->angles.y);
		msg.WriteFloat(to->angles.z);
	}
	if (mask & (1 << 2)) {
		msg.WriteByte(to->ducking);
	}
	if (mask & (1 << 3)) {
		msg.WriteByte(to->on_ground);
	}
	if (mask & (1 << 4)) {
		msg.WriteFloat(to->velocity.x);
		msg.WriteFloat(to->velocity.y);
		msg.WriteFloat(to->velocity.z);
	}
}


void ClientMgr::WriteDeltaSnapshot(Frame* from, Frame* to, ByteWriter& msg, int client_idx)
{
	for (int i = 0; i < 24; i++) {
		WriteDeltaEntState(&from->states[i], &to->states[i], msg, i);
	}
	msg.WriteByte(-1);	// sentinal

	msg.WriteLong(0xabababab);

	WriteDeltaPState(&from->ps_states[client_idx], &to->ps_states[client_idx], msg);
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

	writer.WriteLong(-1);
	WriteDeltaSnapshot(&nullframe, server.GetLastSnapshotFrame(), writer, GetClientIndex(client));

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