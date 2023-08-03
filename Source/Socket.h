#ifndef SOCKET123_H
#define SOCKET123_H
#include <cstdint>
#include <string>

class IPAndPort
{
public:
	IPAndPort() {}
	IPAndPort(unsigned int ip, unsigned short port) : ip(ip), port(port) {}
	void SetIp(int a, int b, int c, int d) {
		ip = a << 24 | b << 16 | c << 8 | d;
	}
	std::string ToString() const;
	// Host byte order
	unsigned int ip = 0;
	unsigned short port = 0;

	bool operator==(const IPAndPort& other) const {
		return ip == other.ip && port == other.port;
	}
	bool operator!=(const IPAndPort& o) const {
		return !(o == *this);
	}
};

class Socket
{
public:
	Socket();
	void Init(int port);
	void Shutdown();
	virtual bool Send(void* data, size_t length, const IPAndPort& to);
	virtual bool Receive(void* data, size_t buffer_size, size_t& recv_len, IPAndPort& from);

	IPAndPort local_addr;
	uintptr_t handle = -1;
};

void NetworkInit();
void NetworkQuit();


#endif // !SOCKET123_H
