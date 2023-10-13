#include "Server.h"
#include "Util.h"
#include "CoreTypes.h"
#include "Model.h"
#include <cstdarg>

#include "Config.h"

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

	cfg_tick_rate = cfg.MakeF("tick_rate", DEFAULT_UPDATE_RATE);
	cfg_snapshot_rate = cfg.MakeF("snapshot_rate", 30.0);
	cfg_max_time_out = cfg.MakeF("max_time_out", 10.f);
	cfg_sv_port = cfg.MakeI("host_port", DEFAULT_SERVER_PORT);

	game.Init();
	//client_mgr.Init();

	cur_frame_idx = 0;
	frames.clear();
	frames.resize(MAX_FRAME_HIST);

	socket.Init(*cfg_sv_port);
	for (int i = 0; i < MAX_CLIENTS; i++)
		clients.push_back(RemoteClient(this, i));

	if (*cfg_tick_rate < 30)
		*cfg_tick_rate = 30;
	else if (*cfg_tick_rate > 150)
		*cfg_tick_rate = 150;

	// initialize tick_interval here
	core.tick_interval = 1.0 / *cfg_tick_rate;

}
void Server::End()
{
	if (!IsActive())
		return;
	DebugOut("ending server\n");
	
	for (int i = 0; i < clients.size(); i++)
		clients[i].Disconnect();

	game.ClearState();
	active = false;
	tick = 0;
	simtime = 0.0;
	map_name = {};
}
void Server::Spawn(const char* mapname)
{
	if (IsActive()) {
		End();
	}
	tick = 0;
	simtime = 0.0;
	DebugOut("spawning with map %s\n", mapname);
	bool good = game.DoNewMap(mapname);
	if (!good)
		return;
	map_name = mapname;
	active = true;
}
bool Server::IsActive() const
{
	return active;
}
void Server::FixedUpdate(double dt)
{
	if (!IsActive())
		return;
	simtime = tick * core.tick_interval;
	ReadPackets();
	game.Update();
	BuildSnapshotFrame();
	UpdateClients();
	tick += 1;
}

void Server::RemoveClient(int client)
{
	game.OnClientLeave(client);
}

void Server::BuildSnapshotFrame()
{
	Frame* frame = &frames.at(cur_frame_idx);
	Game* g = &game;
	frame->tick = tick;
	for (int i = 0; i < MAX_CLIENTS; i++) {
		frame->ps_states[i] = g->ents[i].ToPlayerState();
	}
	for (int i = 0; i < Frame::MAX_FRAME_ENTS; i++) {
		frame->states[i] = g->ents[i].ToEntState();
	}
	cur_frame_idx = (cur_frame_idx + 1) % frames.size();
}

void Server::RunMoveCmd(int client, MoveCommand cmd)
{
	game.ExecutePlayerMove(game.EntForIndex(client), cmd);
}

void Server::SpawnClientInGame(int client)
{
	game.SpawnNewClient(client);

	DebugOut("Spawned client, %s\n", clients[client].GetIPStr().c_str());
}

void Server::WriteServerInfo(ByteWriter& msg)
{
	msg.WriteByte(map_name.size());
	msg.WriteBytes((uint8_t*)map_name.data(), map_name.size());

	msg.WriteFloat(*cfg_tick_rate);
}


void Server::UpdateClients()
{
	for (int i = 0; i < clients.size(); i++)
		clients[i].Update();
}



int Server::FindClient(IPAndPort addr) const {
	for (int i = 0; i < clients.size(); i++) {
		if (clients[i].IsConnected() && clients[i].connection.remote_addr == addr)
			return i;
	}
	return -1;
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

void Server::UnknownPacket(ByteReader& buf, IPAndPort addr)
{
	DebugOut("Connectionless packet recieved from %s\n", addr.ToString().c_str());
	uint8_t command = buf.ReadByte();
	if (command == Msg_ConnectRequest) {
		ConnectNewClient(buf, addr);
	}
	else {
		DebugOut("Unknown connectionless packet\n");
	}
}

void Server::ConnectNewClient(ByteReader& buf, IPAndPort addr)
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
	new_client.InitConnection(addr);
	// Further communication is done with sequenced packets
}

void Server::ReadPackets()
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
			UnknownPacket(reader, from);
			continue;
		}

		int cl_index = FindClient(from);
		if (cl_index == -1) {
			DebugOut("Packet from unknown source: %s\n", from.ToString().c_str());
			continue;
		}

		RemoteClient* client = &clients.at(cl_index);
		// handle packet sequencing
		int header = client->connection.NewPacket(inbuffer, recv_len);
		if (header != -1) {
			ByteReader reader(inbuffer + header, recv_len - header);
			client->OnPacket(reader);
			//HandleClientPacket(client, reader);
		}
	}

	// check timeouts
	for (int i = 0; i < clients.size(); i++) {
		auto& cl = clients[i];
		if (!cl.IsConnected())
			continue;
		if (GetTime() - cl.LastRecieved() > *cfg_max_time_out) {
			printf("Client, %s, timed out\n", cl.GetIPStr().c_str());
			cl.Disconnect();
		}
	}
}
