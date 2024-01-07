#include "Net.h"
#include "Util.h"
#include "Client.h"
#include "Game_Engine.h"
#include "Movement.h"
#include "MeshBuilder.h"
#include "Media.h"
#include "Config.h"

#define DebugOut(fmt, ...) NetDebugPrintf("client: " fmt, __VA_ARGS__)
//#define DebugOut(fmt, ...)

void Client::Init()
{
	cfg_interp_time = cfg.get_var("interp_time", "0.1");
	cfg_fake_lag = cfg.get_var("fake_lag", "0");
	cfg_fake_loss = cfg.get_var("fake_loss", "0");
	cfg_cl_time_out = cfg.get_var("cl_time_out", "5.f");
	cfg_mouse_sensitivity = cfg.get_var("mouse_sensitivity", "0.01");

	server_mgr.Init(this);
	//out_commands.resize(CLIENT_MOVE_HISTORY);

	snapshots.resize(CLIENT_SNAPSHOT_HISTORY);
	commands.resize(CLIENT_MOVE_HISTORY);

	last_recieved_server_tick = -1;
	cur_snapshot_idx = 0;
}

void Client::Disconnect()
{
	DebugOut("disconnecting\n");
	if (get_state() == CS_DISCONNECTED)
		return;
	server_mgr.Disconnect();
	//cl_game.ClearState();
}
void Client::Reconnect()
{
	DebugOut("reconnecting\n");
	//IPAndPort addr = server_mgr.GetCurrentServerAddr();
	Disconnect();
	connect(server_mgr.serveraddr);
}
void Client::connect(string address)
{
	DebugOut("connecting to %s", address.c_str());
	server_mgr.connect(address);
}
Client_State Client::get_state() const
{
	return server_mgr.GetState();
}
int Client::GetPlayerNum() const
{
	return server_mgr.ClientNum();
}

MoveCommand& Client::get_command(int sequence) {
	return commands.at(sequence % CLIENT_MOVE_HISTORY);
}

int Client::GetCurrentSequence() const
{
	return server_mgr.OutSequence();
}
int Client::GetLastSequenceAcked() const
{
	return server_mgr.OutSequenceAk();
}

void PlayerStateToClEntState(EntityState* entstate, PlayerState* state)
{
	entstate->position = state->position;
	entstate->angles = state->angles;
	entstate->ducking = state->ducking;
	entstate->type = Ent_Player;
}

void Client::run_prediction()
{
	if (get_state() != CS_SPAWNED)
		return;
	// predict commands from outgoing ack'ed to current outgoing
	// TODO: dont repeat commands unless a new snapshot arrived
	int start = server_mgr.OutSequenceAk();	// start at the new cmd
	int end = server_mgr.OutSequence();
	int commands_to_run = end - start;
	if (commands_to_run > CLIENT_MOVE_HISTORY)	// overflow
		return;
	// restore state to last authoritative snapshot 
	int incoming_seq = server_mgr.InSequence();
	Snapshot* last_auth_state = &snapshots.at(incoming_seq % CLIENT_SNAPSHOT_HISTORY);

	engine.build_physics_world(0.f);

	// FIXME:
	EntityState last_estate = last_auth_state->entities[GetPlayerNum()];
	PlayerState predicted_player = last_auth_state->pstate;

	for (int i = start + 1; i < end; i++) {
		MoveCommand& cmd = get_command(i);
		bool run_fx = i == (end - 1);

		//cl_game.RunCommand(&pred_state, &next_state, cmd, i==(end-1));

		MeshBuilder b;
		PlayerMovement move;
		move.cmd = cmd;
		move.deltat = engine.tick_interval;
		move.phys_debug = &b;
		move.player = predicted_player;
		move.max_ground_speed = cfg.find_var("max_ground_speed")->real;
		move.simtime = cmd.tick * engine.tick_interval;
		move.isclient = true;
		move.phys = &engine.phys;
		move.entindex = engine.cl->GetPlayerNum();
		move.Run();

		predicted_player = move.player;
	}

	Entity& ent = engine.local_player();

	PlayerStateToClEntState(&last_estate, &predicted_player);
	ent.from_entity_state(last_estate);
	lastpredicted = predicted_player;
}
#if 0
void Client::FixedUpdateInput(double dt)
{
	if (GetConState() >= Connected) {
		CreateMoveCmd();
	}
	server_mgr.SendMovesAndMessages();
	CheckLocalServerIsRunning();
	server_mgr.TrySendingConnect();
}
void Client::FixedUpdateRead(double dt)
{
	if (GetConState() == Spawned) {
		client.tick += 1;
		client.time = client.tick * engine.tick_interval;
	}
	server_mgr.ReadPackets();
	RunPrediction();
}
void Client::PreRenderUpdate(double frametime)
{
	if (IsClientActive() && IsInGame()) {
		// interpoalte entities for rendering
		cl_game.InterpolateEntStates();
		cl_game.ComputeAnimationMatricies();

		cl_game.UpdateViewModelOffsets();
		cl_game.UpdateViewmodelAnimation();
	}
	cl_game.UpdateCamera();
}
#endif

static int NegModulo(int a, int b)
{
	return (b + (a % b)) % b;
}
static float MidLerp(float min, float max, float mid_val)
{
	return (mid_val - min) / (max - min);
}

StateEntry* ClientEntity::GetLastState()
{
	return &hist.at(NegModulo(hist_index - 1, NUM_STORED_STATES));
}
bool IsDistanceATeleport(vec3 a, vec3 b, float maxspeed, float dt)
{
	float len = glm::length(a - b);
	float t = len / maxspeed;
	return dt < t;
}

StateEntry* ClientEntity::GetStateFromIndex(int index)
{
	return &hist.at(NegModulo(index, NUM_STORED_STATES));
}

float InterpolateAnimation(Animation* a, float start, float end, float alpha)
{
	// check distances
	float clip_len = a->total_duration;
	float d1 = glm::abs(end - start);
	float d2 = clip_len - d1;


	if (d1 <= d2) {
		return glm::mix(start, end, alpha);
	}
	else {
		if (start >= end)
			return fmod(start + (alpha * d2), clip_len);
		else
			return fmod(end + ((1 - alpha) * d2), clip_len);
	}
}


void Client::interpolate_states()
{
	auto cl = engine.cl;
	double rendering_time = engine.tick * engine.tick_interval - (engine.cl->cfg_interp_time->real);
	for (int i = 0; i < MAX_GAME_ENTS; i++) {
		if (!engine.ents[i].active() || i == engine.player_num())
			continue;

		ClientEntity& ce = interpolation_data[i];
		Entity& ent = engine.ents[i];

		int entry_index = 0;
		int last_entry = NegModulo(ce.hist_index - 1, ClientEntity::NUM_STORED_STATES);
		for (; entry_index < ClientEntity::NUM_STORED_STATES; entry_index++) {
			auto e = ce.GetStateFromIndex(ce.hist_index - 1 - entry_index);
			if (rendering_time >= e->tick * engine.tick_interval) {
				break;
			}
		}

		// could extrpolate here, or just set it to last state
		if (entry_index == 0 || entry_index == ClientEntity::NUM_STORED_STATES) {
			//interpstate = GetLastState()->state;

			ent.from_entity_state(ce.GetLastState()->state);
			printf("extrapolate\n");
			//double abc = GetStateFromIndex(last_entry - entry_index)->tick * tickinterval;
			return;
		}
		// else, interpolate
		StateEntry* s1 = ce.GetStateFromIndex(last_entry - entry_index);
		StateEntry* s2 = ce.GetStateFromIndex(last_entry - entry_index + 1);

		if (s1->tick == -1) {
			ent.from_entity_state(EntityState{});
			return;
		}
		else if (s2->tick == -1 || s2->tick <= s1->tick) {
			ent.from_entity_state(s1->state);
			printf("no next frame\n");
			return;
		}


		ASSERT(s1->tick < s2->tick);
		ASSERT(s1->tick * engine.tick_interval <= rendering_time && s2->tick * engine.tick_interval >= rendering_time);

		float midlerp = MidLerp(s1->tick * engine.tick_interval, s2->tick * engine.tick_interval, rendering_time);


		EntityState interpstate = s1->state;
		//if(!IsDistanceATeleport(s1->state.position,s2->state.position, 20.0, tickinterval))
		interpstate.position = glm::mix(s1->state.position, s2->state.position, midlerp);


		if (ent.model && ent.model->animations && s1->state.leganim == s2->state.leganim) {
			//interpstate.leganim_frame = glm::mix(s1->state.leganim_frame, s2->state.leganim_frame, midlerp);


			int anim = s1->state.leganim;
			if (anim >= 0 && anim < ent.model->animations->clips.size()) {

				interpstate.leganim_frame = InterpolateAnimation(&ent.model->animations->clips[s1->state.leganim],
					s1->state.leganim_frame, s2->state.leganim_frame, midlerp);
			}

		}
		if (s1->state.mainanim == s2->state.mainanim) {
			// fixme
		}

		ent.from_entity_state(interpstate);
	}

	//double rendering_time_client = cl->tick * engine.tick_interval - engine.frame_remainder;
	//ClientEntity* local = cl->GetLocalPlayer();
	//local->interpstate = local->GetLastState()->state;
}


#if 0
// Called every graphics frame
void ClientEntity::InterpolateState(double time, double tickinterval) {
	if (!active)
		return;
	
	//interpstate = GetLastState()->state;
	//return;

	int i = 0;
	int last_entry = NegModulo(hist_index - 1, NUM_STORED_STATES);
	for (; i < NUM_STORED_STATES; i++) {
		auto e = GetStateFromIndex(hist_index - 1 - i);
		if (time >= e->tick*tickinterval) {
			break;
		}
	}

	// could extrpolate here, or just set it to last state
	if (i == 0 || i == NUM_STORED_STATES) {
		interpstate = GetLastState()->state;
		printf("extrapolate\n");
		double abc = GetStateFromIndex(last_entry - i)->tick * tickinterval;
		return;
	}
	// else, interpolate
	StateEntry* s1 = GetStateFromIndex(last_entry - i);
	StateEntry* s2 = GetStateFromIndex(last_entry - i + 1);

	if (s1->tick == -1) {
		interpstate = EntityState{};
		return;
	}
	else if (s2->tick == -1 || s2->tick <= s1->tick) {
		interpstate = s1->state;
		printf("no next frame\n");
		return;
	}


	ASSERT(s1->tick < s2->tick);
	ASSERT(s1->tick * tickinterval <= time && s2->tick * tickinterval >= time);

	float midlerp = MidLerp(s1->tick * tickinterval, s2->tick * tickinterval, time);

	//printf("interpolating properly\n");
	interpstate = s1->state;
	//if(!IsDistanceATeleport(s1->state.position,s2->state.position, 20.0, tickinterval))
	interpstate.position = glm::mix(s1->state.position, s2->state.position, midlerp);

	const Model* m = model;
	if (!m || !m->animations)
		return;

	if (s1->state.leganim == s2->state.leganim) {
		//interpstate.leganim_frame = glm::mix(s1->state.leganim_frame, s2->state.leganim_frame, midlerp);
	

		int anim = s1->state.leganim;
		if (anim >= 0 && anim < m->animations->clips.size()) {

			interpstate.leganim_frame = InterpolateAnimation(&m->animations->clips[s1->state.leganim],
				s1->state.leganim_frame, s2->state.leganim_frame, midlerp);
		}

	}
	if (s1->state.mainanim == s2->state.mainanim) {
		// fixme
	}


}
#endif

void Client::SetNewTickRate(float tick_rate)
{
	if (!IsServerActive()) {
		engine.tick_interval = 1.0 / tick_rate;
	}
}

Snapshot* Client::GetCurrentSnapshot()
{
	return &snapshots.at(server_mgr.InSequence() % CLIENT_SNAPSHOT_HISTORY);
}
Snapshot* Client::FindSnapshotForTick(int tick)
{
	for (int i = 0; i < snapshots.size(); i++) {
		if (snapshots[i].tick == tick)
			return &snapshots[i];
	}
	return nullptr;
}

/*
when clients recieve snapshot from server
they mark entities in engine.ents as active or inactive (maybe will have some more complicated initilization/destruction)
replicate standard fields over to entity
fill the network transform history in client.interpolation_history

later in frame:
interpolate_state() fills the entities position/rotation with interpolated values for rendering
*/



void Client::read_snapshot(Snapshot* s)
{
	Snapshot* snapshot = s;
	for (int i = 0; i < Snapshot::MAX_ENTS; i++) {
		ClientEntity* ce = &interpolation_data[i];
		Entity* e = &engine.ents[i];
		EntityState* state = &snapshot->entities[i];

		int lasttype = e->type;

		// local player doesnt fill interp history here
		if (i == engine.player_num()) {
			e->type = Ent_Player;
			continue;
		}

		if (state->type == Ent_Free) {
			e->type = Ent_Free;
			continue;
		}

		if (lasttype == Ent_Free) {
			*e = Entity();
			*ce = ClientEntity();
			e->type = (EntType)snapshot->entities[i].type;
		}

		ce->hist.at(ce->hist_index) = { engine.tick, *state };
		ce->hist_index = (ce->hist_index + 1) % ce->hist.size();

		if (state->model_idx >= 0 && state->model_idx < media.gamemodels.size())
			e->model = media.gamemodels.at(state->model_idx);
	}

	ClearEntsThatDidntUpdate(engine.tick);
}

void Client::ClearEntsThatDidntUpdate(int what_tick)
{
#if 0
	ClientGame* game = &cl_game;
	for (int i = 0; i < game->entities.size(); i++) {
		ClientEntity* ce = &game->entities[i];
		if (i == GetPlayerNum()) continue;
		if (ce->active && ce->GetLastState()->tick != what_tick) {
			game->entities[i].active = false;
		}


	}
#endif
}