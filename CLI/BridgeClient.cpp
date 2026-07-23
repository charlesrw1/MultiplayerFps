#include "BridgeClient.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")

namespace {

// WSAStartup/WSACleanup once per process, on first use - cscli only ever makes one call per
// invocation today, but this keeps bridge_call safe to call more than once regardless.
struct WinsockGuard
{
	WinsockGuard() {
		WSADATA wsa;
		WSAStartup(MAKEWORD(2, 2), &wsa);
	}
	~WinsockGuard() { WSACleanup(); }
};

SOCKET connect_loopback(int port, int timeout_seconds, std::string& err) {
	static WinsockGuard guard;

	SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (s == INVALID_SOCKET) {
		err = "socket() failed";
		return INVALID_SOCKET;
	}

	const DWORD timeout_ms = (DWORD)(timeout_seconds > 0 ? timeout_seconds * 1000 : 10000);
	setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout_ms, sizeof(timeout_ms));
	setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout_ms, sizeof(timeout_ms));

	sockaddr_in addr = {};
	addr.sin_family = AF_INET;
	addr.sin_port = htons((u_short)port);
	inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

	if (connect(s, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
		err = "could not connect to 127.0.0.1:" + std::to_string(port) +
			  " - is the editor/game running with agentbridge.enabled 1?";
		closesocket(s);
		return INVALID_SOCKET;
	}
	return s;
}

} // namespace

BridgeResult bridge_call(int port, const std::string& cmd, const nlohmann::json& args, int timeout_seconds) {
	BridgeResult result;

	std::string err;
	SOCKET s = connect_loopback(port, timeout_seconds, err);
	if (s == INVALID_SOCKET) {
		result.connect_error = err;
		return result;
	}

	nlohmann::json request;
	request["cmd"] = cmd;
	request["args"] = args;
	const std::string line = request.dump() + "\n";
	if (send(s, line.data(), (int)line.size(), 0) == SOCKET_ERROR) {
		result.connect_error = "send() failed";
		closesocket(s);
		return result;
	}

	std::string recv_buf;
	char buf[4096];
	for (;;) {
		int n = recv(s, buf, sizeof(buf), 0);
		if (n > 0) {
			recv_buf.append(buf, n);
			size_t nl = recv_buf.find('\n');
			if (nl != std::string::npos) {
				recv_buf.resize(nl);
				break;
			}
		} else if (n == 0) {
			result.connect_error = "connection closed before a full response was received";
			closesocket(s);
			return result;
		} else {
			result.connect_error = "timed out waiting for a response (try a larger --timeout)";
			closesocket(s);
			return result;
		}
	}
	closesocket(s);

	try {
		result.response = nlohmann::json::parse(recv_buf);
		result.connected = true;
	} catch (const std::exception& e) {
		result.connect_error = std::string("bad JSON response: ") + e.what();
	}
	return result;
}

bool bridge_ping(int port, int timeout_seconds) {
	BridgeResult r = bridge_call(port, "ping", nlohmann::json::object(), timeout_seconds);
	return r.connected && r.response.value("ok", false);
}
