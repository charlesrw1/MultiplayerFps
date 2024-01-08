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

void Client::init()
{
	cfg_interp_time = cfg.get_var("interp_time", "0.1");
	cfg_fake_lag = cfg.get_var("fake_lag", "0");
	cfg_fake_loss = cfg.get_var("fake_loss", "0");
	cfg_cl_time_out = cfg.get_var("cl_time_out", "5.f");
	interpolate = cfg.get_var("interpolate", "1");

	sock.Init(0);
}

void Client::connect(string address)
{
	DebugOut("connecting to server: %s\n", address.c_str());

	serveraddr = address;
	IPAndPort ip;
	ip.set(address);
	if (ip.port == 0)ip.port = DEFAULT_SERVER_PORT;
	server.remote_addr = ip;
	connect_attempts = 0;
	attempt_time = -1000.f;
	state = CS_TRYINGCONNECT;
	client_num = -1;
	force_full_update = false;

	snapshots.clear();
	commands.clear();
	snapshots.resize(CLIENT_SNAPSHOT_HISTORY);
	commands.resize(CLIENT_MOVE_HISTORY);

	last_recieved_server_tick = -1;
	cur_snapshot_idx = 0;
	for (int i = 0; i < MAX_GAME_ENTS; i++)
		interpolation_data[i] = Entity_Interp();
	lastpredicted = PlayerState();

	TrySendingConnect();
}

void Client::TrySendingConnect()
{
	const int MAX_CONNECT_ATTEMPTS = 10;
	const float CONNECT_RETRY = 1.f;

	if (state != CS_TRYINGCONNECT)
		return;
	if (connect_attempts >= MAX_CONNECT_ATTEMPTS) {
		DebugOut("Unable to connect to server\n");
		state = CS_DISCONNECTED;
		return;
	}
	double delta = GetTime() - attempt_time;
	if (delta < CONNECT_RETRY)
		return;
	attempt_time = GetTime();
	connect_attempts++;
	DebugOut("Sending connection request\n");

	uint8_t buffer[256];
	ByteWriter writer(buffer, 256);
	writer.WriteLong(CONNECTIONLESS_SEQUENCE);
	writer.write_string("connect");
	writer.EndWrite();
	sock.Send(buffer, writer.BytesWritten(), server.remote_addr);
}
void Client::Disconnect()
{
	DebugOut("Disconnecting\n");
	if (state == CS_DISCONNECTED)
		return;
	if (state != CS_TRYINGCONNECT) {
		uint8_t buffer[8];
		ByteWriter write(buffer, 8);
		write.WriteByte(CL_QUIT);
		write.EndWrite();
		server.Send(buffer, write.BytesWritten());
	}

	state = CS_DISCONNECTED;
	serveraddr = "";
	client_num = -1;
	server = Connection();
	cur_snapshot_idx = 0;
}


void Client::Reconnect()
{
	DebugOut("Reconnecting\n");

	string address = std::move(serveraddr);
	Disconnect();
	connect(address);
}

int Client::GetPlayerNum() const
{
	return client_num;
}

Move_Command& Client::get_command(int sequence) {
	return commands.at(sequence % CLIENT_MOVE_HISTORY);
}

void PlayerStateToClEntState(EntityState* entstate, PlayerState* state)
{
	entstate->position = state->position;
	entstate->angles = state->angles;
	entstate->solid = state->ducking;
	entstate->type = Ent_Player;
}

void Client::run_prediction()
{
	if (get_state() != CS_SPAWNED)
		return;
	// predict commands from outgoing ack'ed to current outgoing
	// TODO: dont repeat commands unless a new snapshot arrived
	int start = OutSequenceAk();	// start at the new cmd
	int end = OutSequence();
	int commands_to_run = end - start;
	if (commands_to_run > CLIENT_MOVE_HISTORY)	// overflow
		return;
	// restore state to last authoritative snapshot 
	int incoming_seq = InSequence();
	Snapshot* last_auth_state = &snapshots.at(incoming_seq % CLIENT_SNAPSHOT_HISTORY);

	engine.build_physics_world(0.f);

	// FIXME:
	EntityState last_estate = last_auth_state->entities[GetPlayerNum()];
	PlayerState predicted_player = last_auth_state->pstate;

	for (int i = start + 1; i < end; i++) {
		Move_Command& cmd = get_command(i);
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

static int NegModulo(int a, int b)
{
	return (b + (a % b)) % b;
}
static float MidLerp(float min, float max, float mid_val)
{
	return (mid_val - min) / (max - min);
}

Interp_Entry* Entity_Interp::GetLastState()
{
	return &hist[NegModulo(hist_index - 1, HIST_SIZE)];
}
bool IsDistanceATeleport(vec3 a, vec3 b, float maxspeed, float dt)
{
	float len = glm::length(a - b);
	float t = len / maxspeed;
	return dt < t;
}

Interp_Entry* Entity_Interp::GetStateFromIndex(int index)
{
	return &hist[NegModulo(index, HIST_SIZE)];
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

void set_entity_interp_vars(Entity& e, Interp_Entry& i)
{
	e.position = i.position;
	e.rotation = i.angles;
	e.anim.leganim = i.legs_anim;
	e.anim.mainanim = i.main_anim;
	e.anim.leganim_frame = i.la_frame;
	e.anim.mainanim_frame = i.ma_frame;
}


void Client::interpolate_states()
{
	if (!interpolate->integer)
		return;

	auto cl = engine.cl;
	double rendering_time = engine.tick * engine.tick_interval - (engine.cl->cfg_interp_time->real);
	
	for (int i = 0; i < MAX_GAME_ENTS; i++) {
		if (!engine.ents[i].active() || i == engine.player_num())
			continue;

		Entity_Interp& ce = interpolation_data[i];
		Entity& ent = engine.ents[i];

		int entry_index = 0;
		int last_entry = NegModulo(ce.hist_index - 1, Entity_Interp::HIST_SIZE);
		for (; entry_index < Entity_Interp::HIST_SIZE; entry_index++) {
			auto e = ce.GetStateFromIndex(ce.hist_index - 1 - entry_index);
			if (rendering_time >= e->tick * engine.tick_interval) {
				break;
			}
		}

		// could extrpolate here, or just set it to last state
		if (entry_index == 0 || entry_index == Entity_Interp::HIST_SIZE) {
			//ent.from_entity_state(ce.GetLastState()->state);
			set_entity_interp_vars(ent, *ce.GetLastState());
			//printf("extrapolate\n");
			continue;
		}
		// else, interpolate
		Interp_Entry* s1 = ce.GetStateFromIndex(last_entry - entry_index);
		Interp_Entry* s2 = ce.GetStateFromIndex(last_entry - entry_index + 1);


		if (s1->tick == -1 && s2->tick == -1) {
			//ent.from_entity_state(EntityState{});
			printf("no state to use\n");
			continue;
		}
		else if (s1->tick == -1) {
			//ent.from_entity_state(s2->state);
			set_entity_interp_vars(ent, *s2);
			continue;
		}
		else if (s2->tick == -1 || s2->tick <= s1->tick) {
			set_entity_interp_vars(ent, *s1);
			//ent.from_entity_state(s1->state);
			continue;
		}

		ASSERT(s1->tick < s2->tick);
		ASSERT(s1->tick * engine.tick_interval <= rendering_time && s2->tick * engine.tick_interval >= rendering_time);

		float midlerp = MidLerp(s1->tick * engine.tick_interval, s2->tick * engine.tick_interval, rendering_time);


		Interp_Entry interpstate = *s1;
		//if(!IsDistanceATeleport(s1->state.position,s2->state.position, 20.0, tickinterval))
		interpstate.position = glm::mix(s1->position, s2->position, midlerp);

		if (ent.model && ent.model->animations && s1->legs_anim == s2->legs_anim) {

			int anim = s1->legs_anim;
			if (anim >= 0 && anim < ent.model->animations->clips.size()) {

				interpstate.la_frame = InterpolateAnimation(&ent.model->animations->clips[s1->legs_anim],
					s1->la_frame, s2->la_frame, midlerp);
			}

		}
		//if (s1->state.mainanim == s2->state.mainanim) {
		//	// fixme
		//}

		set_entity_interp_vars(ent, interpstate);
		//ent.from_entity_state(interpstate);
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

Snapshot* Client::GetCurrentSnapshot()
{
	return &snapshots.at(InSequence() % CLIENT_SNAPSHOT_HISTORY);
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
		Entity_Interp& interp = interpolation_data[i];
		Entity& e = engine.ents[i];
		EntityState& state = snapshot->entities[i];

		int lasttype = e.type;

		// local player doesnt fill interp history here
		if (i == engine.player_num()) {
			// FIXME
			e.type = Ent_Player;
			continue;
		}

		if (state.type == Ent_Free) {
			e.type = Ent_Free;
			continue;
		}

		if (lasttype == Ent_Free) {
			e = Entity();
			interp = Entity_Interp();
		}
		// replicate variables to entity
		e.from_entity_state(state);
		// save off some vars for rendering interpolation
		Interp_Entry& entry = interp.hist[interp.hist_index];
		entry.tick = engine.tick;
		entry.position = state.position;
		entry.angles = state.angles;
		entry.main_anim = state.mainanim;
		entry.legs_anim = state.leganim;
		entry.ma_frame = state.mainanim_frame;
		entry.la_frame = state.leganim_frame;
		interp.hist_index = (interp.hist_index + 1) % Entity_Interp::HIST_SIZE;
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