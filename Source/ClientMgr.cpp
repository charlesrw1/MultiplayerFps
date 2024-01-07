#include "Client.h"
#include "Game_Engine.h"

#define DebugOut(fmt, ...) NetDebugPrintf("client: " fmt, __VA_ARGS__)
//#define DebugOut(fmt, ...)

static const int MAX_CONNECT_ATTEMPTS = 10;
static const float CONNECT_RETRY = 1.f;


ClServerMgr::ClServerMgr()// : sock_emulator(&sock)
{
	sock.enabled = false;
}

void ClServerMgr::Init(Client* parent)
{
	sock.Init(0);
	myclient = parent;
}
void ClServerMgr::connect(string address)
{
	DebugOut("connecting to server: %s\n", address.c_str());

	serveraddr = address;
	IPAndPort ip;
	ip.set(address);
	if (ip.port == 0)ip.port = DEFAULT_SERVER_PORT;
	server.remote_addr = ip;
	connect_attempts = 0;
	attempt_time = -1000.f;
	state = CS_TRYINGCONNECT;
	client_num = -1;

	TrySendingConnect();
}

void ClServerMgr::TrySendingConnect()
{
	if (state != CS_TRYINGCONNECT)
		return;
	if (connect_attempts >= MAX_CONNECT_ATTEMPTS) {
		DebugOut("Unable to connect to server\n");
		state = CS_DISCONNECTED;
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
	writer.write_string("connect");
	writer.EndWrite();
	sock.Send(buffer, writer.BytesWritten(), server.remote_addr);
}
void ClServerMgr::Disconnect()
{
	if (state == CS_DISCONNECTED)
		return;
	if (state != CS_TRYINGCONNECT) {
		uint8_t buffer[8];
		ByteWriter write(buffer, 8);
		write.WriteByte(CL_QUIT);
		write.EndWrite();
		server.Send(buffer, write.BytesWritten());
	}
	state = CS_DISCONNECTED;

	client_num = -1;
	server.remote_addr = IPAndPort();
}


void ClServerMgr::ReadPackets()
{
	//ASSERT(myclient->initialized);
	
	// check cfg var updates
	sock.loss = myclient->cfg_fake_loss->integer;
	sock.lag = myclient->cfg_fake_lag->integer;
	sock.jitter = 0;
	sock.enabled = !(sock.loss == 0 && sock.lag == 0 && sock.jitter == 0);

	uint8_t inbuffer[MAX_PAYLOAD_SIZE + PACKET_HEADER_SIZE];
	size_t recv_len = 0;
	IPAndPort from;

	while (sock.Receive(inbuffer, sizeof(inbuffer), recv_len, from))
	{
		if (recv_len < PACKET_HEADER_SIZE)
			continue;

		// connectionless packet
		if (*(uint32_t*)inbuffer == CONNECTIONLESS_SEQUENCE) {
			ByteReader buf(inbuffer + 4, recv_len - 4);
			
			string msg;
			buf.read_string(msg);

			if (msg == "accept" && state == CS_TRYINGCONNECT) {
				int client_num_ = buf.ReadByte();
				string mapname;
				buf.read_string(mapname);
				int tick = buf.ReadLong();
				float tickrate = buf.ReadFloat();


				DebugOut("Player num: %d, Map: %s, tickrate %f\n", client_num, mapname.c_str(), tickrate);
				DebugOut("loading map...\n");

				// We now have a valid connection with the server
				server.Init(&sock, server.remote_addr);
				state = CS_CONNECTED;
				engine.start_map(mapname, true);
				engine.set_tick_rate(tickrate);
				engine.tick = tick;
				client_num = client_num_;
			}
			else if (msg == "reject" && state == CS_TRYINGCONNECT) {
				printf("Connection rejected\n");
				myclient->Disconnect();
			}
			else {
				DebugOut("Unknown connectionless packet type\n");
			}

			continue;
		}

		if (state >= CS_CONNECTED && from == server.remote_addr) {
			int header = server.NewPacket(inbuffer, recv_len);
			if (header != -1) {
				ByteReader reader(inbuffer + header, recv_len - header);
				HandleServerPacket(reader);
			}
		}
	}

	if (state >= CS_CONNECTED) {
		if (GetTime() - server.last_recieved > myclient->cfg_cl_time_out->real) {
			printf("Server timed out\n");
			myclient->Disconnect();
		}
	}

	//int last_sequence = server.out_sequence_ak;
	//int tick_delta = myclient->tick - myclient->GetCommand(last_sequence)->tick;
	//printf("RTT: %f\n", (tick_delta)*core.tick_interval);

}

#if 0

void ClServerMgr::StartConnection()
{
	// We now have a valid connection with the server
	server.Init(&sock, server.remote_addr);
	state = Connected;
	// Request the next step in the setup
	uint8_t buffer[256];
	ByteWriter writer(buffer, 256);
	writer.WriteByte(CL_TEXT);
	const char init_str[] = "init";
	writer.WriteByte(sizeof(init_str) - 1);
	writer.WriteBytes((uint8_t*)init_str, sizeof(init_str) - 1);
	writer.EndWrite();
	// FIXME!!: this must go through reliable, do this later
	server.Send(buffer, writer.BytesWritten());
}

void ClServerMgr::OnServerInit(ByteReader& buf)
{
	ASSERT(0);
	client_num = buf.ReadByte();

	int map_len = buf.ReadByte();
	std::string mapname(map_len, 0);
	buf.ReadBytes((uint8_t*)&mapname[0], map_len);

	float sv_tick_rate = buf.ReadFloat();
	myclient->SetNewTickRate(sv_tick_rate);

	DebugOut("Player num: %d, Map: %s, tickrate %f\n", client_num, mapname.c_str(), sv_tick_rate);

	// Load map and game data here
	DebugOut("loading map...\n");
	//myclient->cl_game.NewMap(mapname.c_str());
	engine.start_map(mapname.c_str(), true);

	// Tell server to spawn us in
	uint8_t buffer[64];
	ByteWriter writer(buffer, 64);
	writer.WriteByte(CL_TEXT);
	const char spawn_str[] = "spawn";
	writer.WriteByte(sizeof(spawn_str) - 1);
	writer.WriteBytes((uint8_t*)spawn_str, sizeof(spawn_str) - 1);
	writer.EndWrite();
	server.Send(buffer, writer.BytesWritten());
}

#endif

void ClServerMgr::HandleServerPacket(ByteReader& buf)
{
	for (;;)
	{
		buf.AlignToByteBoundary();
		if (buf.IsEof())
			return;
		if (buf.HasFailed()) {
			myclient->Disconnect();
			return;
		}
		
		uint8_t command = buf.ReadByte();
		switch (command)
		{
		case SV_NOP:
			break;
		case SV_INITIAL:
			ASSERT(0);
			//OnServerInit(buf);
			break;
		case SV_SNAPSHOT: {
			if (state == CS_CONNECTED) {
				state = CS_SPAWNED;
			}
			bool ignore_packet = OnEntSnapshot(buf);
			if (ignore_packet)
				return;
		}break;
		case SV_DISCONNECT:
			DebugOut("disconnected by server\n");
			myclient->Disconnect();
			break;
		case SV_TEXT:
			break;
		case SV_TICK:
		{
			// just force it for now
			int server_tick = buf.ReadLong();
			if (abs(server_tick - engine.tick) > 1) {
				DebugOut("delta tick %d\n", server_tick - engine.tick);
				
				engine.tick = server_tick;
			}
			myclient->last_recieved_server_tick = server_tick;
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
	if (state < CS_CONNECTED)
		return;

	// Send move
	MoveCommand lastmove = myclient->get_command(server.out_sequence);

	uint8_t buffer[128];
	ByteWriter writer(buffer, 128);
	writer.WriteByte(CL_INPUT);
	writer.WriteLong(engine.tick);
	writer.WriteByte(MoveCommand::quantize(lastmove.forward_move));
	writer.WriteByte(MoveCommand::quantize(lastmove.lateral_move));
	writer.WriteByte(MoveCommand::quantize(lastmove.up_move));

	writer.WriteFloat(lastmove.view_angles.x);
	writer.WriteFloat(lastmove.view_angles.y);
	writer.WriteLong(lastmove.button_mask);

	writer.WriteByte(CL_SET_BASELINE);
	if (force_full_update)
		writer.WriteLong(-1);
	else
		writer.WriteLong(myclient->last_recieved_server_tick);

	writer.EndWrite();
	server.Send(buffer, writer.BytesWritten());
}

static EntityState null_estate;
static PlayerState null_pstate;

bool ClServerMgr::OnEntSnapshot(ByteReader& msg)
{
	int delta_tick = msg.ReadLong();

	if (delta_tick == -1) {
		printf("client: recieved full update\n");
		if (force_full_update)
			force_full_update = false;
	}
	
	
	Snapshot* nextsnapshot = myclient->GetCurrentSnapshot();
	nextsnapshot->tick = myclient->last_recieved_server_tick;
	Snapshot* from = nullptr;

	if (delta_tick != -1) {
		from = myclient->FindSnapshotForTick(delta_tick);
		if (!from) {
			printf("client: delta snapshot not found! (snapshot: %d, current: %d)", delta_tick, engine.tick);
			return true;
		}
	}

	// set new snapshot to old snapshot
	for (int i = 0; i < Snapshot::MAX_ENTS; i++) {
		EntityState* old = &null_estate;
		if (from)
			old = &from->entities[i];
		nextsnapshot->entities[i] = *old;
	}


	for (;;) {
		uint8_t index = msg.ReadByte();
		if (index == 0xff)
			break;
		if (msg.HasFailed())
			break;
		if (index >= Snapshot::MAX_ENTS)
			break;

		EntityState* es = &nextsnapshot->entities[index];
		ReadDeltaEntState(es, msg);
	}

	int val = msg.ReadLong();
	ASSERT(val == 0xabababab);

	PlayerState* ps = &nextsnapshot->pstate;
	if (from)
		*ps = from->pstate;
	else
		*ps = null_pstate;

	ReadDeltaPState(ps, msg);

	myclient->read_snapshot(nextsnapshot);

	return false;
}