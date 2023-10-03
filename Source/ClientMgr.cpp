#include "Client.h"

#define DebugOut(fmt, ...) NetDebugPrintf("client: " fmt, __VA_ARGS__)
//#define DebugOut(fmt, ...)

ClServerMgr::ClServerMgr()// : sock_emulator(&sock)
{
	EnableLag(150, 0, 0);
}


void ClServerMgr::DisableLag()
{
	sock.enabled = false;
}
void ClServerMgr::EnableLag(int lag, int jitter, int loss)
{
	sock.enabled = true;
	sock.lag = lag;
	sock.jitter = jitter;
	sock.loss = loss;
}

void ClServerMgr::Init(Client* parent)
{
	sock.Init(0);
	myclient = parent;
}
void ClServerMgr::Connect(const IPAndPort& where)
{
	DebugOut("connecting to server: %s\n", where.ToString().c_str());
	server.remote_addr = where;
	connect_attempts = 0;
	attempt_time = -1000.f;
	state = TryingConnect;
	TrySendingConnect();
}

void ClServerMgr::TrySendingConnect()
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
	writer.WriteLong(CONNECTIONLESS_SEQUENCE);
	writer.WriteByte(Msg_ConnectRequest);
	const char name[] = "Charlie";
	writer.WriteByte(sizeof(name) - 1);
	writer.WriteBytes((uint8_t*)name, sizeof(name) - 1);
	sock.Send(buffer, writer.BytesWritten(), server.remote_addr);
}
void ClServerMgr::Disconnect()
{
	if (state == Disconnected)
		return;
	if (state != TryingConnect) {
		uint8_t buffer[8];
		ByteWriter write(buffer, 8);
		write.WriteByte(ClMessageQuit);
		server.Send(buffer, write.BytesWritten());
	}
	state = Disconnected;

	client_num = -1;
	server.remote_addr = IPAndPort();
}

void ClServerMgr::ReadPackets()
{
	new_packet_arrived = false;
	ASSERT(myclient->initialized);
	uint8_t inbuffer[MAX_PAYLOAD_SIZE + PACKET_HEADER_SIZE];
	size_t recv_len = 0;
	IPAndPort from;

	while (sock.Receive(inbuffer, sizeof(inbuffer), recv_len, from))
	{
		if (recv_len < PACKET_HEADER_SIZE)
			continue;
		if (*(uint32_t*)inbuffer == CONNECTIONLESS_SEQUENCE) {
			ByteReader reader(inbuffer + 4, recv_len - 4);
			HandleUnknownPacket(from, reader);
			continue;
		}

		if ((state == Connected || state == Spawned) && from == server.remote_addr) {
			int header = server.NewPacket(inbuffer, recv_len);
			if (header != -1) {
				ByteReader reader(inbuffer + header, recv_len - header);
				HandleServerPacket(reader);
			}
		}
	}

	if (state == Connected || state == Spawned) {
		if (GetTime() - server.last_recieved > MAX_TIME_OUT) {
			printf("Server timed out\n");
			myclient->Disconnect();
		}
	}

	int last_sequence = server.out_sequence_ak;
	int tick_delta = myclient->tick - myclient->GetCommand(last_sequence)->tick;
	//printf("RTT: %f\n", (tick_delta)*core.tick_interval);

}


void ClServerMgr::HandleUnknownPacket(IPAndPort addr, ByteReader& buf)
{
	uint8_t type = buf.ReadByte();
	if (type == Msg_AcceptConnection) {
		if (state != TryingConnect)
			return;
		StartConnection();
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
		if (!buf.HasFailed())
			printf("Connection rejected: %s\n", reason);
		myclient->Disconnect();
	}
	else {
		DebugOut("Unknown connectionless packet type\n");
	}
}

void ClServerMgr::StartConnection()
{
	// We now have a valid connection with the server
	server.Init(&sock, server.remote_addr);
	state = Connected;
	// Request the next step in the setup
	uint8_t buffer[256];
	ByteWriter writer(buffer, 256);
	writer.WriteByte(ClMessageText);
	const char init_str[] = "init";
	writer.WriteByte(sizeof(init_str) - 1);
	writer.WriteBytes((uint8_t*)init_str, sizeof(init_str) - 1);

	// FIXME!!: this must go through reliable, do this later
	server.Send(buffer, writer.BytesWritten());
}

void ClServerMgr::ParseServerInit(ByteReader& buf)
{
	client_num = buf.ReadByte();
	int map_len = buf.ReadByte();
	std::string mapname(map_len, 0);
	buf.ReadBytes((uint8_t*)&mapname[0], map_len);
	DebugOut("Player num: %d, Map: %s\n", client_num, mapname.c_str());

	// Load map and game data here
	DebugOut("loading map...\n");
	myclient->cl_game.NewMap(mapname.c_str());

	// Tell server to spawn us in
	uint8_t buffer[64];
	ByteWriter writer(buffer, 64);
	writer.WriteByte(ClMessageText);
	const char spawn_str[] = "spawn";
	writer.WriteByte(sizeof(spawn_str) - 1);
	writer.WriteBytes((uint8_t*)spawn_str, sizeof(spawn_str) - 1);
	server.Send(buffer, writer.BytesWritten());
}

void ClServerMgr::HandleServerPacket(ByteReader& buf)
{
	while (!buf.IsEof())
	{
		uint8_t command = buf.ReadByte();
		if (buf.HasFailed()) {
			myclient->Disconnect();
			return;
		}
		switch (command)
		{
		case SvNop:
			break;
		case SvMessageInitial:
			ParseServerInit(buf);
			break;
		case SvMessageSnapshot:
			if (state == Connected) {
				state = Spawned;
			}
			ParseEntSnapshot(buf);
			break;
		case SvMessagePlayerState:
			ParsePlayerData(buf);
			break;
		case SvMessageDisconnect:
			DebugOut("disconnected by server\n");
			myclient->Disconnect();
			break;
		case SvMessageText:
			break;
		case SvMessageTick:
		{
			// just force it for now
			int server_tick = buf.ReadLong();
			if (abs(server_tick - myclient->tick) > 1) {
				DebugOut("delta tick %d\n", server_tick - myclient->tick);
				myclient->tick = server_tick;
			}
		}break;
		default:
			DebugOut("Unknown message, disconnecting\n");
			myclient->Disconnect();
			return;
			break;
		}
	}
}

void ClServerMgr::SendMovesAndMessages()
{
	if (state < Connected)
		return;

	//DebugOut("Wrote move to server\n");

	// Send move
	MoveCommand lastmove = *myclient->GetCommand(server.out_sequence);

	uint8_t buffer[128];
	ByteWriter writer(buffer, 128);
	writer.WriteByte(ClMessageInput);
	writer.WriteFloat(lastmove.forward_move);
	writer.WriteFloat(lastmove.lateral_move);
	writer.WriteFloat(lastmove.up_move);
	writer.WriteFloat(lastmove.view_angles.x);
	writer.WriteFloat(lastmove.view_angles.y);
	writer.WriteLong(lastmove.button_mask);

	server.Send(buffer, writer.BytesWritten());
}

void ClServerMgr::ParseEntSnapshot(ByteReader& msg)
{
	// TEMPORARY!!
	ClientGame* game = &myclient->cl_game;

	Snapshot* snapshot = &game->snapshots.at(server.in_sequence % CLIENT_SNAPSHOT_HISTORY);
	for (int i = 0; i < 8; i++) {
		int index = msg.ReadWord();
		ASSERT(index < 16);
		EntityState* state = &snapshot->entities[index];
		state->type = msg.ReadByte();
		state->position.x = msg.ReadFloat();
		state->position.y = msg.ReadFloat();
		state->position.z = msg.ReadFloat();
		state->ducking = msg.ReadByte();
	}
	for (int i = 0; i < 8; i++) {
		game->entities[i].prev_state = game->entities[i].state;
		game->entities[i].state = snapshot->entities[i];
	}
	new_packet_arrived = true;
}

void ClServerMgr::ParsePlayerData(ByteReader& buf)
{
	Snapshot* snapshot = &myclient->cl_game.snapshots.at(server.in_sequence % CLIENT_SNAPSHOT_HISTORY);
	PlayerState* pstate = &snapshot->pstate;
	pstate->velocity.x = buf.ReadFloat();
	pstate->velocity.y = buf.ReadFloat();
	pstate->velocity.z = buf.ReadFloat();
	pstate->on_ground = buf.ReadByte();
}