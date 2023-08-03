#include "Connection.h"
#include "Util.h"
#include "CoreTypes.h"


static unsigned ReadInt(const uint8_t* data)
{
	unsigned i = 0;
	i |= data[0];
	i |= data[1] << 8;
	i |= data[2] << 16;
	i |= data[3] << 24;
	return i;
}
static void WriteInt(uint8_t* data, unsigned i)
{
	data[0] = i & 0xff;
	data[1] = (i>>8) & 0xff;
	data[2] = (i>>16) & 0xff;
	data[3] = (i>>24) & 0xff;
}


Connection::Connection()
{
	reliable_out.resize(MAX_PAYLOAD_SIZE);
	reliable_unacked.resize(MAX_PAYLOAD_SIZE);
}

void Connection::Init(Socket* sock, IPAndPort addr)
{
	this->sock = sock;
	remote_addr = addr;
	out_sequence = 0;
	out_sequence_ak = -1;
	in_sequence = -1;
	last_recieved = GetTime();
	reliable_out_len = 0;
	reliable_unacked_len = 0;
}
void Connection::Clear()
{
	sock = nullptr;
	remote_addr = IPAndPort();
}

int Connection::NewPacket(const uint8_t* data, int length)
{
	if (length < PACKET_HEADER_SIZE)
		return -1;

	unsigned new_seq = ReadInt(data);
	unsigned new_seq_ak = ReadInt(data + 4);

	int dropped = new_seq - (in_sequence + 1);
	if (new_seq <= in_sequence && in_sequence!=-1) {
		printf("duplicate or out of order packets\n");
		return -1;
	}

	if (dropped > 0)
		printf("dropped packets\n");

	in_sequence = new_seq;
	out_sequence_ak = new_seq_ak;
	last_recieved = GetTime();
	//last_recieved = host.realtime;
	//last_packet_size = inmsg.size_in_bytes();

	//SendHistory::Entry& entry = seq_history.GetEntry(new_seq_ak);
	//if (entry.sequence == new_seq_ak)
	//	seq_history.AccumulateRtt(host.realtime - entry.time_sent);

	static int timer = 0;
	if (timer == 0) {
		//printf("rtt avg: %f\n", seq_history.rtt_avg);
		timer = 10;
	}
	timer--;

	return PACKET_HEADER_SIZE;
}

void Connection::Send(const uint8_t* data, int len)
{
	ASSERT(sock);

	// until reliable msgs are implmented
	if (data == nullptr)
		return;

	uint8_t msg_buffer[MAX_PAYLOAD_SIZE + PACKET_HEADER_SIZE];
	WriteInt(msg_buffer, out_sequence);
	WriteInt(msg_buffer+4, in_sequence);

	// Write reliable data here, for now skip and just copy unreliable
	if (len < MAX_PAYLOAD_SIZE) {
		memcpy(msg_buffer + PACKET_HEADER_SIZE, data, len);
	}
	else {
		printf("Couldn't write unreliable\n");
		return;
	}

	bool good = sock->Send(msg_buffer, len + PACKET_HEADER_SIZE, remote_addr);
	out_sequence++;
}