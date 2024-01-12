#include "Server.h"
#include "Util.h"
#include "Game_Engine.h"
#include "Model.h"
#include <cstdarg>

#include "Config.h"

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

void Server::init()
{
	printf("initializing server\n");

	cfg_tick_rate		= cfg.get_var("tick_rate", std::to_string(DEFAULT_UPDATE_RATE).c_str());
	cfg_snapshot_rate	= cfg.get_var("snapshot_rate", "30.0");
	cfg_max_time_out	= cfg.get_var("max_time_out", "10.f");
	cfg_sv_port			= cfg.get_var("host_port", std::to_string(DEFAULT_SERVER_PORT).c_str());

	frames.clear();
	frames.resize(MAX_FRAME_HIST);

	for (int i = 0; i < MAX_CLIENTS; i++)
		clients.push_back(RemoteClient(this, i));

	if (cfg_tick_rate->real < 30)
		cfg.set_var("tick_rate", "30.0");
	else if (cfg_tick_rate->real > 150)
		cfg.set_var("tick_rate", "150.0");

	// initialize tick_interval here
	engine.tick_interval = 1.0 / cfg_tick_rate->real;

}
void Server::end()
{
	DebugOut("ending server\n");
	for (int i = 0; i < clients.size(); i++)
		clients[i].Disconnect();
	socket.Shutdown();
	initialized = false;
}

void Server::start()
{
	DebugOut("starting server with map %s\n", engine.mapname.c_str());
	if(initialized)
		end();
	socket.Init(cfg_sv_port->integer);
	initialized = true;
	engine.tick_interval = 1.0 / cfg_tick_rate->real;
}


Frame* Server::GetSnapshotFrame()
{
	return &frames.at(engine.tick % MAX_FRAME_HIST);
}

void Server::BuildSnapshotFrame()
{
	Frame* frame = GetSnapshotFrame();
	frame->tick = engine.tick;
	for (int i = 0; i < MAX_CLIENTS; i++) {
		frame->ps_states[i] = engine.ents[i].ToPlayerState();
	}
	for (int i = 0; i < Frame::MAX_FRAME_ENTS; i++) {
		frame->states[i] = engine.ents[i].to_entity_state();
	}
}

void Server::connect_local_client()
{
	printf("putting local client in server\n");

	if (clients[0].IsConnected()) {
		printf(__FUNCTION__": clients[0] is taken??\n");
		clients[0].Disconnect();
	}
	clients[0].client_num = 0;
	clients[0].state = SCS_SPAWNED;
	IPAndPort ip;
	ip.set("localhost");
	clients[0].connection.Init(&socket, ip);
	clients[0].local_client = true;

	engine.make_client(0);
}

int Server::FindClient(IPAndPort addr) const {
	for (int i = 0; i < clients.size(); i++) {
		if (clients[i].IsConnected() && clients[i].connection.remote_addr == addr)
			return i;
	}
	return -1;
}


void Server::ConnectNewClient(ByteReader& buf, IPAndPort addr)
{
	int spot = 0;
	bool already_connected = false;
	for (; spot < clients.size(); spot++) {
		// if the "accept" response was dropped, client might send a connect again
		if (clients[spot].state >= SCS_CONNECTED && clients[spot].connection.remote_addr == addr) {
			already_connected = true;
			break;
		}
		if (clients[spot].state == SCS_DEAD)
			break;
	}
	if (spot == clients.size()) {
		uint8_t buffer[64];
		ByteWriter writer(buffer, 64);
		writer.WriteLong(CONNECTIONLESS_SEQUENCE);
		writer.write_string("reject");
		writer.EndWrite();
		socket.Send(buffer, writer.BytesWritten(), addr);

		return;
	}
	if (!already_connected)
		console_printf("New client connected %s\n", addr.ToString().c_str());

	uint8_t accept_buf[256];
	ByteWriter writer(accept_buf, 256);
	writer.WriteLong(CONNECTIONLESS_SEQUENCE);
	writer.write_string("accept");
	writer.WriteByte(spot);
	writer.write_string(engine.mapname);
	writer.WriteLong(engine.tick);
	writer.WriteFloat(cfg_tick_rate->real);
	writer.EndWrite();
	socket.Send(accept_buf, writer.BytesWritten(), addr);

	RemoteClient& new_client = clients[spot];
	new_client.init(addr);
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
			ByteReader buf(inbuffer + 4, recv_len - 4);
			
			DebugOut("Connectionless packet recieved from %s\n", from.ToString().c_str());
			string msg;
			buf.read_string(msg);
			if (msg == "connect") {
				ConnectNewClient(buf, from);
			}
			else {
				DebugOut("Unknown connectionless packet\n");
			}

			continue;
		}

		int cl_index = FindClient(from);
		if (cl_index == -1) {
			console_printf("Packet recieved from unknown source: %s\n", from.ToString().c_str());
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
		if (!cl.IsConnected()||cl.local_client)
			continue;
		if (GetTime() - cl.LastRecieved() > cfg_max_time_out->real) {
			console_printf("Client %d %s timed out\n", i, cl.GetIPStr().c_str());
			cl.Disconnect();
		}
	}
}
