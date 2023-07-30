#include "Net.h"
#include "Util.h"

//#define DebugOut(fmt, ...) NetDebugPrintf(fmt, __VA_ARGS__)
#define DebugOut(fmt, ...)
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
	writer.WriteLong(CONNECTIONLESS_SEQUENCE);
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

		if ((state == Connected || state == Spawned) && from == server.remote_addr) {
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
	if (state != TryingConnect) {
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
		if (!buf.HasFailed())
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
	writer.WriteByte(ClMessageText);
	const char init_str[] = "init";
	writer.WriteByte(sizeof(init_str) - 1);
	writer.WriteBytes((uint8_t*)init_str, sizeof(init_str) - 1);

	// FIXME!!: this must go through reliable, do this later
	server.Send(buffer, writer.BytesWritten());
}

void Client::ReadInitData(ByteReader& buf)
{
	player_num = buf.ReadByte();
	int map_len = buf.ReadByte();
	std::string mapname(map_len, 0);
	buf.ReadBytes((uint8_t*)&mapname[0], map_len);
	printf("Player num: %d, Map: %s\n", player_num, mapname.c_str());

	// Load map and game data here

	// Tell server to spawn us in
	uint8_t buffer[64];
	ByteWriter writer(buffer, 64);
	writer.WriteByte(ClMessageText);
	const char spawn_str[] = "spawn";
	writer.WriteByte(sizeof(spawn_str) - 1);
	writer.WriteBytes((uint8_t*)spawn_str, sizeof(spawn_str) - 1);
	server.Send(buffer, writer.BytesWritten());
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

void Client::SendCommands()
{
	if (state < Connected)
		return;

	DebugOut("Wrote move to server\n");

	// Send move
	const MoveCommand& lastmove = commands[server.out_sequence % CLIENT_MOVE_HISTORY];

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