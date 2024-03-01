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

Server::Server() :
	tick_rate("sv.tick_rate", (float)DEFAULT_UPDATE_RATE),
	snapshot_rate("sv.snapshot_rate", 30.f),
	max_time_out("sv.time_out", 10.f)
{

}

void Server::init()
{
	frames.clear();
	frames.resize(MAX_FRAME_HIST);

	for (int i = 0; i < MAX_CLIENTS; i++)
		clients.push_back(RemoteClient(this, i));

	if (tick_rate.real() < 30)
		tick_rate.real() = 30.f;
	else if (tick_rate.real() > 150)
		tick_rate.real() = 150.f;

	// initialize tick_interval here
	eng->tick_interval = 1.0 / tick_rate.real();
}
void Server::end(const char* log_reason)
{
	sys_print("Ending server because %s\n", log_reason);
	for (int i = 0; i < clients.size(); i++)
		clients[i].Disconnect("server is ending");
	socket.Shutdown();
	initialized = false;
}

void Server::start()
{
	if (initialized)
		end("restarting server");;
	sys_print("Starting server...\n");

	int host_port = Var_Manager::get()->get_var("net.hostport")->integer;

	socket.Init(host_port);
	initialized = true;
	eng->tick_interval = 1.0 / tick_rate.real();
}


Frame* Server::GetSnapshotFrame()
{
	return &frames.at(eng->tick % MAX_FRAME_HIST);
}


void Server::connect_local_client()
{
	sys_print("Putting local client in server\n");

	if (clients[0].IsConnected()) {
		sys_print(__FUNCTION__": clients[0] is taken??\n");
		clients[0].Disconnect("client[0] shouldn't be here");
	}
	clients[0].client_num = 0;
	clients[0].state = SCS_SPAWNED;
	IPAndPort ip;
	ip.set("localhost");
	clients[0].connection.Init(&socket, ip);
	clients[0].local_client = true;

	eng->make_client(0);
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
		sys_print("New client connected %s\n", addr.ToString().c_str());

	uint8_t accept_buf[256];
	ByteWriter writer(accept_buf, 256);
	writer.WriteLong(CONNECTIONLESS_SEQUENCE);
	writer.write_string("accept");
	writer.WriteByte(spot);
	writer.write_string(eng->mapname);
	writer.WriteLong(eng->tick);
	writer.WriteFloat(tick_rate.real());
	writer.EndWrite();
	socket.Send(accept_buf, writer.BytesWritten(), addr);

	RemoteClient& new_client = clients[spot];
	if (!already_connected) {
		new_client.init(addr);
		new_client.state = SCS_SPAWNED;
		sys_print("Spawning client %d : %s\n", spot, addr.ToString().c_str());
		eng->make_client(spot);
	}
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
			ByteReader buf(inbuffer, recv_len, sizeof(inbuffer));
			buf.ReadLong();	// skip header
			
			sys_print("Connectionless packet recieved from %s\n", from.ToString().c_str());
			string msg;
			buf.read_string(msg);
			if (msg == "connect") {
				ConnectNewClient(buf, from);
			}
			else {
				sys_print("Unknown connectionless packet\n");
			}

			continue;
		}

		int cl_index = FindClient(from);
		if (cl_index == -1) {
			sys_print("Packet recieved from unknown source: %s\n", from.ToString().c_str());
			continue;
		}

		RemoteClient* client = &clients.at(cl_index);
		// handle packet sequencing
		int header_len = client->connection.NewPacket(inbuffer, recv_len);
		if (header_len != -1) {
			ByteReader reader(inbuffer, recv_len, sizeof(inbuffer));
			reader.AdvanceBytes(header_len);	// skip packet header

			client->OnPacket(reader);
		}
	}

	// check timeouts
	for (int i = 0; i < clients.size(); i++) {
		auto& cl = clients[i];
		if (!cl.IsConnected()||cl.local_client)
			continue;
		if (GetTime() - cl.LastRecieved() > max_time_out.real()) {
			cl.Disconnect("client timed out");
		}
	}
}
