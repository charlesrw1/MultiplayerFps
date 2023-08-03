#ifndef CONNECTION_H
#define CONNECTION_H
#include "Socket.h"
#include "Net.h"
#include <vector>

class Connection
{
public:
	Connection();
	void Init(Socket* sock, IPAndPort addr);
	void Clear();

	// returns the offset to where payload starts, -1 if this packet should be skipped
	int NewPacket(const uint8_t* data, int length);
	// data= unreliable data, reliable data should be added to reliable_out
	void Send(const uint8_t* data, int length);

	Socket* sock = nullptr;

	IPAndPort remote_addr;
	int out_sequence = 0;		// current local sequence
	int out_sequence_ak = -1;	// last acked sequence
	int in_sequence = -1;		// last recieved remote sequence
	double last_recieved = 0;	// time (sec) since recieved

	int reliable_out_len = 0;
	std::vector<uint8_t> reliable_out;		// "backbuffered" reliable messages to send
	int reliable_unacked_len = 0;
	std::vector<uint8_t> reliable_unacked;	// un-acked data to send each packet
};
#endif // !CONNECTION_H