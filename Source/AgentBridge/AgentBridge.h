#pragma once
#include <string>
#include <functional>
#include <json.hpp>

// A small, self-contained local TCP bridge that lets external tools/scripts (the Blender
// export/viewer addon today; potentially other automation/AI-agent clients later) query the
// running engine over a loopback socket, using a line-delimited JSON request/response protocol.
//
// This module is intentionally additive and non-invasive: nothing else in the engine depends on
// it, it never touches the currently-loaded Level/editor state, and it can be deleted wholesale
// without affecting anything else. It works in both editor and game builds.
//
// Disabled/enabled and configured via ConfigVars (agentbridge.enabled, agentbridge.port), settable
// in vars.txt or from the command line like any other cvar (e.g. `-agentbridge.enabled 0`).
//
// Wire protocol: client connects, sends one line of JSON `{"cmd":"name","args":{...}}\n`, engine
// replies with one line of JSON `{"ok":true,"result":{...}}\n` or `{"ok":false,"error":"..."}\n`,
// then closes the connection. One request per connection; client reconnects for the next call.

using AgentBridgeHandler = std::function<nlohmann::json(const nlohmann::json& args)>;

// Called once during engine init.
void agent_bridge_init();
// Called once per real frame (both editor and game loop); non-blocking, cheap when idle.
void agent_bridge_update();
// Called once during engine cleanup.
void agent_bridge_shutdown();

// Registers a named command handler. Throwing from a handler is caught and turned into an
// {"ok":false,"error":<what()>} response, so handlers don't need their own top-level try/catch.
void agent_bridge_register_command(const std::string& name, AgentBridgeHandler handler);

// Static self-registration helper, mirrors the engine's existing Auto_Engine_Cmd pattern.
struct AutoAgentBridgeCommand
{
	AutoAgentBridgeCommand(const char* name, AgentBridgeHandler handler);
};
#define AGENT_BRIDGE_COMMAND(name, handler) \
	static AutoAgentBridgeCommand agent_bridge_auto_##name(#name, handler);
