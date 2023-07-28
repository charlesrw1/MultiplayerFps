#include "ClientNServer.h"
#include "Util.h"

static void SendStringToAddress(const std::string& str, IPAndPort where, Socket* socket)
{
	uint8_t buffer[4+1+256];
	if (str.size() > 256)
		return;
	ByteWriter buf(buffer, 4 + 1 + 256);
	buf.WriteDword(CONNECTIONLESS_SEQUENCE);
	buf.WriteByte(str.size());
	for (auto c : str)
		buf.WriteByte(c);
	socket->Send(buffer, buf.BytesWritten(), where);
}

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
		if (clients[i].state != RemoteClient::Dead && clients[i].connection.remote_addr == addr)
			return i;
	}
	return -1;
}
void Server::HandleUnknownPacket(const IPAndPort& addr, ByteReader& buf)
{
	int does_client_exist = FindClient(addr);
	if (does_client_exist != -1)
		return;

	printf("Connectionless packet recieved from %s\n", addr.ToString().c_str());
	int str_len = buf.ReadByte();
	std::string command;
	for (int i = 0; i < str_len; i++)
		command.push_back(buf.ReadByte());
	if (buf.HasFailed())
		return;
	if (command == "connect")
		ConnectNewClient(addr);
	else
		printf("Unknown connectionless packet\n");
}
void Server::ConnectNewClient(const IPAndPort& addr)
{
	int spot = 0;
	for (; spot < clients.size(); spot++) {
		// I checked this earlier but might as well make sure
		if (clients[spot].state != RemoteClient::Dead && clients[spot].connection.remote_addr == addr)
			return;
		if (clients[spot].state == RemoteClient::Dead)
			break;
	}
	if (spot == clients.size()) {
		SendStringToAddress("response 'server is full'", addr, &socket);
		return;
	}

	SendStringToAddress("accepted", addr, &socket);
	printf("Connected new client %s", addr.ToString().c_str());
	RemoteClient& new_client = clients[spot];
	new_client.state = RemoteClient::Connecting;
	new_client.connection.Init(&socket, addr);
	// Now communicate using the Connection interface
}

void Server::DisconnectClient(RemoteClient& client)
{
	printf("Disconnecting client %s\n", client.connection.remote_addr.ToString().c_str());
	

	if (client.state == RemoteClient::Connected) {
		// remove entitiy from game, call game logic ...
	}
	client.state = RemoteClient::Dead;
	client.connection.remote_addr = IPAndPort();
}

void Server::HandlePacket(RemoteClient& client, ByteReader& buf)
{
	while (!buf.IsEof())
	{
		uint8_t command = buf.ReadByte();
		if (buf.HasFailed()) {
			DisconnectClient(client);
			return;
		}
		switch (command)
		{
		case ClMessageInput:
			break;
		case ClMessageQuit:
			break;
		case ClMessageText:
			break;
		case ClMessageTick:
			break;
		default:
			DisconnectClient(client);
			return;
			break;
		}
	}
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
	active = true;
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
	SendStringToAddress("connect", server.remote_addr, &socket);
}

void Client::ReadMessages()
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

		if (state != Disconnected && state != TryingConnect && from == server.remote_addr) {
			int header = server.NewPacket(inbuffer, recv_len);
			if (header != -1) {
				ByteReader reader(inbuffer + header, recv_len - header);
				HandlePacket(reader);
			}
		}
	}
}

void Client::Disconnect()
{

}

void Client::HandleUnknownPacket(const IPAndPort& addr, ByteReader& buf)
{

}

void Client::HandlePacket(ByteReader& buf)
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
		case SvMessageStart:
			break;
		case SvMessageSnapshot:
			break;
		case SvMessageDisconnect:
			break;
		case SvMessageText:
			break;
		case SvMessageTick:
			break;
		case NetMessageEnd:
			break;
		default:
			Disconnect();
			return;
			break;
		}
	}
}