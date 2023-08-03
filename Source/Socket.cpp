#include "Socket.h"
#include <Winsock.h>
#include <cstdio>
#include "Util.h"


std::string IPAndPort::ToString() const
{
	std::string out;
	for (int i = 0; i < 4; i++) {
		out += std::to_string((ip >> (3 - i) * 8) & 0xff);
		if (i != 3) out += '.';
	}
	out += ':';
	out += std::to_string(port);
	return out;
}

Socket::Socket()
{
	handle = INVALID_SOCKET;
}

static sockaddr_in ToSockaddr(const IPAndPort& ip)
{
	sockaddr_in addr{};
	addr.sin_addr.s_addr = htonl(ip.ip);
	addr.sin_port = htons(ip.port);
	addr.sin_family = AF_INET;
	return addr;
}

static SOCKET MakeSocket(int port)
{
	SOCKET handle = socket(PF_INET, SOCK_DGRAM, 0);
	if (handle == INVALID_SOCKET) {
		Fatalf("socket() failed: %d\n", WSAGetLastError());
	}
	int yes = 1;
	if (setsockopt(handle, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<char*>(&yes), sizeof(yes)) == -1) {
		Fatalf("setsockopt() failed: %d\n", WSAGetLastError());
	}

	sockaddr_in addr = ToSockaddr(IPAndPort(0, port));

	if (bind(handle, (sockaddr*)&addr, sizeof(addr)) == -1) {
		Fatalf("bind() failed: %d\n", WSAGetLastError());
	}

	return handle;
}

void Socket::Init(int port)
{
	if (handle == INVALID_SOCKET) {
		handle = MakeSocket(port);
		ASSERT(handle != INVALID_SOCKET);

		u_long blocking = 1;
		ioctlsocket(handle, static_cast<long>(FIONBIO), &blocking);

		sockaddr_in addr;
		int len = sizeof(addr);
		if (getsockname(handle, (sockaddr*)&addr, &len) == -1) {
			Fatalf("getsockname() failed: %d\n", WSAGetLastError());
		}
		local_addr.ip = ntohl(addr.sin_addr.s_addr);
		local_addr.port = ntohs(addr.sin_port);
		printf("IP Address: %s\n", local_addr.ToString().c_str());
	}
}

void Socket::Shutdown()
{
	if (handle != INVALID_SOCKET) {
		closesocket(handle);
		handle = INVALID_SOCKET;
	}
}

bool Socket::Send(void* data, size_t length, const IPAndPort& to)
{
	ASSERT(handle != INVALID_SOCKET);

	sockaddr_in addr = ToSockaddr(to);
	if (sendto(handle, (char*)data, length, 0, (sockaddr*)&addr, sizeof(addr)) == -1) {
		printf("Socket::Send error: %d\n", WSAGetLastError());
		return false;
	}
	return true;
}

bool Socket::Recieve(void* data, size_t buffer_size, size_t& recv_len, IPAndPort& from)
{
	from = {};
	recv_len = 0;
	sockaddr_in sender;
	int sender_len = sizeof(sender);
	int bytes = recvfrom(handle, (char*)data, buffer_size, 0, (sockaddr*)&sender, &sender_len);
	if (bytes == -1) {
		// usually -1 means no more data
		return false;
	}
	recv_len = bytes;
	from.ip = ntohl(sender.sin_addr.s_addr);
	from.port = ntohs(sender.sin_port);
	return true;
}

void NetworkInit()
{
	WSAData data;
	WSAStartup(MAKEWORD(2, 2), &data);
}

void NetworkQuit()
{
	WSACleanup();
}