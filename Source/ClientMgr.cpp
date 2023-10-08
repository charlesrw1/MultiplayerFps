#include "Client.h"

#define DebugOut(fmt, ...) NetDebugPrintf("client: " fmt, __VA_ARGS__)
//#define DebugOut(fmt, ...)

static const int MAX_CONNECT_ATTEMPTS = 10;
static const float CONNECT_RETRY = 1.f;


ClServerMgr::ClServerMgr()// : sock_emulator(&sock)
{
	DisableLag();
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
	if (delta < CONNECT_RETRY)
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
	ASSERT(myclient->initialized);
	
	// check cfg var updates
	if (*myclient->cfg_fake_lag == 0 && *myclient->cfg_fake_loss == 0)
		DisableLag();
	else
		EnableLag(*myclient->cfg_fake_lag, 0, *myclient->cfg_fake_loss);
	
	
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
		if (GetTime() - server.last_recieved > *myclient->cfg_cl_time_out) {
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

void ClServerMgr::OnServerInit(ByteReader& buf)
{
	client_num = buf.ReadByte();

	int map_len = buf.ReadByte();
	std::string mapname(map_len, 0);
	buf.ReadBytes((uint8_t*)&mapname[0], map_len);

	float sv_tick_rate = buf.ReadFloat();
	myclient->SetNewTickRate(sv_tick_rate);

	DebugOut("Player num: %d, Map: %s, tickrate %f\n", client_num, mapname.c_str(), sv_tick_rate);

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
			OnServerInit(buf);
			break;
		case SvMessageSnapshot:
			if (state == Connected) {
				state = Spawned;
			}
			OnEntSnapshot(buf);
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
	writer.WriteLong(client.tick);
	writer.WriteFloat(lastmove.forward_move);
	writer.WriteFloat(lastmove.lateral_move);
	writer.WriteFloat(lastmove.up_move);
	writer.WriteFloat(lastmove.view_angles.x);
	writer.WriteFloat(lastmove.view_angles.y);
	writer.WriteLong(lastmove.button_mask);

	server.Send(buffer, writer.BytesWritten());
}

void ClServerMgr::OnEntSnapshot(ByteReader& msg)
{
	Snapshot* snapshot = myclient->GetCurrentSnapshot();
	*snapshot = Snapshot();

	int delta_frame_index = msg.ReadLong();		// unused rn
	ASSERT(delta_frame_index == -1);
	for (;;) {
		uint8_t index = msg.ReadByte();
		if (index == 0xff)
			break;
		if (msg.HasFailed())
			break;
		if (index >= Snapshot::MAX_ENTS)
			break;
		EntityState* es = &snapshot->entities[index];
		*es = EntityState();
		ReadDeltaEntState(es, msg);
	}

	int val = msg.ReadLong();
	ASSERT(val == 0xabababab);

	PlayerState* ps = &snapshot->pstate;
	*ps = PlayerState();
	ReadDeltaPState(ps, msg);

	myclient->SetupSnapshot(snapshot);
}