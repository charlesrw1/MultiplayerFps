#include "Server.h"
#include "Util.h"
#include "CoreTypes.h"
#include "Model.h"
#include <cstdarg>

Server server;

void NetDebugPrintf(const char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	
	char buffer[2048];
	float time_since_start = TimeSinceStart();
	int l = sprintf_s(buffer, "%8.2f: ", time_since_start);
	vsnprintf(buffer + l, 2048 - l, fmt, ap);
	printf(buffer);
	
	va_end(ap);
}

#define DebugOut(fmt, ...) NetDebugPrintf("server: " fmt, __VA_ARGS__)

void Server::Init()
{
	printf("initializing server\n");
	client_mgr.Init();
	sv_game.Init();

	cur_frame_idx = 0;
	frames.clear();
	frames.resize(MAX_FRAME_HIST);
}
void Server::End()
{
	if (!IsActive())
		return;
	DebugOut("ending server\n");
	client_mgr.ShutdownServer();
	sv_game.ClearState();
	active = false;
	tick = 0;
	simtime = 0.0;
	map_name = {};
}
void Server::Spawn(const char* mapname)
{
	if (IsActive()) {
		End();
	}
	tick = 0;
	simtime = 0.0;
	DebugOut("spawning with map %s\n", mapname);
	bool good = sv_game.DoNewMap(mapname);
	if (!good)
		return;
	map_name = mapname;
	active = true;
}
bool Server::IsActive() const
{
	return active;
}
void Server::FixedUpdate(double dt)
{
	if (!IsActive())
		return;
	simtime = tick * core.tick_interval;
	client_mgr.ReadPackets();
	sv_game.Update();
	BuildSnapshotFrame();
	client_mgr.SendSnapshots();
	tick += 1;
}

void Server::BuildSnapshotFrame()
{
	Frame* frame = &frames.at(cur_frame_idx);
	Game* game = &sv_game;
	frame->tick = tick;
	for (int i = 0; i < MAX_CLIENTS; i++) {
		frame->ps_states[i] = game->ents[i].ToPlayerState();
	}
	for (int i = 0; i < Frame::MAX_FRAME_ENTS; i++) {
		frame->states[i] = game->ents[i].ToEntState();
	}

	cur_frame_idx = (cur_frame_idx + 1) % frames.size();
}