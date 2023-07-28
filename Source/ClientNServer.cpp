#include "ClientNServer.h"
#include "Util.h"
void Server::Start()
{
	printf("Starting server...\n");
	socket.Init(SERVER_PORT);
	clients.resize(MAX_CLIENTS);
	active = true;
}
void Server::Quit()
{
	// Alert clients
	socket.Shutdown();
	active = false;
}
int Server::FindClient(const IPAndPort& addr) const
{
	for (int i = 0; i < clients.size(); i++) {
		if (clients[i].state == RemoteClient::Connected && clients[i].connection.remote_addr == addr)
			return i;
	}
	return -1;
}
void Server::HandleUnknownPacket(const IPAndPort& addr, ByteReader& buf)
{
	printf("Connectionless packet recieved from %s\n", addr.ToString().c_str());
	int str_len = buf.ReadByte();
	char str_buffer[64+1];
	if (str_len >= 64)
		return;
	buf.ReadBytes((uint8_t*)str_buffer, str_len);
	str_buffer[str_len] = '\0';
	printf("Message: %s\n", str_buffer);
}
void Server::HandlePacket(RemoteClient& client, ByteReader& buf)
{
}
void Server::ReadMessages()
{
	ASSERT(active);
	uint8_t inbuffer[MAX_DATAGRAM_SIZE + PACKET_HEADER_SIZE];
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
			printf("Packet from unknown source: %s\n", from.ToString().c_str());
			continue;
		}

		RemoteClient& client = clients.at(cl_index);
		// handle packet sequencing
		int header = client.connection.NewPacket(inbuffer, recv_len);
		if (header != -1) {
			ByteReader reader(inbuffer + header, recv_len - header);
			HandlePacket(client, reader);
		}
	}
}

void Client::Start()
{
	printf("Starting client...\n");
	socket.Init(0);
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
		printf("Unable to connect to server\n");
		state = Disconnected;
		return;
	}
	double delta = GetTime() - attempt_time;
	if (delta < CONNECT_RETRY_TIME)
		return;
	attempt_time = GetTime();
	connect_attempts++;
	printf("Sending connection request\n");
	uint8_t buffer[64];
	ByteWriter buf(buffer, 64);
	buf.WriteDword(CONNECTIONLESS_SEQUENCE);
	const char connect_str[] = "connect";
	buf.WriteByte(sizeof(connect_str) - 1);
	buf.WriteBytes((uint8_t*)connect_str, sizeof(connect_str) - 1);
	socket.Send(buffer, buf.BytesWritten(), server.remote_addr);
}