#ifndef LAGEMULATOR_H
#define LAGEMULATOR_H
#include <queue>
#include <ctime>
#include "Net.h"
#include "MathLib.h"
#include "Util.h"

class EmulatedSocket : public Socket
{
public:
	EmulatedSocket() : rand_device(time(NULL)) {}
	int lag = 0;	// ms of lag
	int loss = 0;	// % of packets to drop
	int jitter = 0;	// ms of + of packet jitter
	bool enabled = false;
	bool Send(void* data, size_t len, const IPAndPort& to) override {
		if (!enabled) {
			return Socket::Send(data, len, to);
		}

		if (len >= MAX_PAYLOAD_SIZE + PACKET_HEADER_SIZE)
			return false;
		if (!ShouldDropThis())
		{
			LagPacket packet;
			memcpy(packet.data, data, len);
			packet.size = len;
			packet.addr = to;
			packet.arrival_time = ComputeArrivalTime();
			outgoing.push(packet);
		}
		while (!outgoing.empty() && outgoing.front().arrival_time < GetTime())
		{
			LagPacket& pack = outgoing.front();
			Socket::Send(pack.data, pack.size, pack.addr);
			outgoing.pop();
		}
		return true;
	}
	bool Receive(void* buffer, size_t buffer_size, size_t& len, IPAndPort& from) override {
		if (!enabled) {
			return Socket::Receive(buffer, buffer_size, len, from);
		}

		for (;;) {
			LagPacket packet;
			bool good = Socket::Receive(packet.data, MAX_PAYLOAD_SIZE+PACKET_HEADER_SIZE, packet.size, packet.addr);
			if (packet.size == 0 || !good)
				break;
			if (ShouldDropThis() || packet.size > buffer_size)
				continue;
			packet.arrival_time = ComputeArrivalTime();
			incoming.push(packet);
		}
		if (!incoming.empty() && incoming.front().arrival_time < GetTime()) {
			auto& packet = incoming.front();
			from = packet.addr;
			len = packet.size;
			memcpy(buffer, packet.data, packet.size);
			incoming.pop();
			return true;
		}
		else {
			len = 0;
			return false;
		}
	}
private:
	bool ShouldDropThis() {
		return rand_device.RandI(0, 99) < loss;
	}
	double ComputeArrivalTime() {
		int jitteramt = rand_device.RandI(0, jitter);
		double time_added = (lag + jitteramt) / 1000.0;
		return GetTime() + time_added;
	}
	struct LagPacket {
		uint8_t data[MAX_PAYLOAD_SIZE + PACKET_HEADER_SIZE];
		size_t size = 0;
		IPAndPort addr;
		Socket* sock = nullptr;
		double arrival_time = 0.0;
	};
	Random rand_device;
	std::queue<LagPacket> outgoing;
	std::queue<LagPacket> incoming;
};
#endif // !LAGEMULATOR_H