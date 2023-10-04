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
	time = 0.0;
	map_name = {};
}
void Server::Spawn(const char* mapname)
{
	if (IsActive()) {
		End();
	}
	tick = 0;
	time = 0.0;
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

const float fall_speed_threshold = -0.05f;
const float grnd_speed_threshold = 0.1f;

void PlayerUpdateAnimations(double dt, Entity* ent)
{
	auto playeranims = ent->model->animations.get();
	float groundspeed = glm::length(glm::vec2(ent->velocity.x, ent->velocity.z));
	bool falling = ent->velocity.y < fall_speed_threshold;

	int leg_anim = 0;
	if (groundspeed > grnd_speed_threshold) {
		if (ent->ducking)
			leg_anim = playeranims->FindClipFromName("act_crouch_walk");
		else
			leg_anim = playeranims->FindClipFromName("act_run");
	}
	else {
		leg_anim = playeranims->FindClipFromName("act_idle");
	}

	if (leg_anim != ent->anim.leganim) {
		ent->anim.SetLegAnim(leg_anim, 1.f);
	}
}

void PlayerUpdate(double dt, Entity* ent)
{
	if(ent->alive)
		PlayerUpdateAnimations(dt, ent);
}

void DoGameUpdate(double dt)
{
	Game* game = &server.sv_game;
	for (int i = 0; i < game->ents.size(); i++) {
		if (game->ents[i].type == Ent_Player) {
			PlayerUpdate(dt,&game->ents[i]);
		}



		if (game->ents[i].type != Ent_Free) {
			game->ents[i].anim.AdvanceFrame(dt);
		}
	}


}
void Server::FixedUpdate(double dt)
{
	if (!IsActive())
		return;
	client_mgr.ReadPackets();
	DoGameUpdate(dt);
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