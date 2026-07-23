#pragma once
#include <json.hpp>
#include <string>

// One JSON-RPC round trip to a running engine's AgentBridge (see
// Source/AgentBridge/AgentBridge.h for the wire protocol this speaks).
struct BridgeResult
{
	// False if we never got a well-formed response at all (couldn't connect, send failed, recv
	// timed out, or the response wasn't valid JSON) - a transport failure, not a command failure.
	// connect_error explains why. When true, `response` is the raw {"ok":...,"result"/"error":...}
	// blob the bridge sent back, regardless of whether the command itself succeeded.
	bool connected = false;
	std::string connect_error;
	nlohmann::json response;
};

// Connects to 127.0.0.1:port, sends {"cmd":cmd,"args":args} as one line, reads one line back.
// timeout_seconds bounds both the connect attempt and the response read.
BridgeResult bridge_call(int port, const std::string& cmd, const nlohmann::json& args, int timeout_seconds);

// Quick liveness check used by Discovery to filter dead lockfiles: true if `ping` gets an ok
// response back within timeout_seconds.
bool bridge_ping(int port, int timeout_seconds);
