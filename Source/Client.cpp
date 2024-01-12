#include "Net.h"
#include "Util.h"
#include "Client.h"
#include "Game_Engine.h"
#include "Player.h"
#include "MeshBuilder.h"
#include "Media.h"
#include "Config.h"

#define DebugOut(fmt, ...) NetDebugPrintf("client: " fmt, __VA_ARGS__)
//#define DebugOut(fmt, ...)

void Client::init()
{
	cfg_interp_time		= cfg.get_var("interp_time", "0.1");
	cfg_fake_lag		= cfg.get_var("fake_lag", "0");
	cfg_fake_loss		= cfg.get_var("fake_loss", "0");
	cfg_cl_time_out		= cfg.get_var("cl_time_out", "5.f");
	interpolate			= cfg.get_var("interpolate", "1");
	smooth_error_time	= cfg.get_var("smooth_error", "1.0");
	cfg_do_predict		= cfg.get_var("do_predict", "1");
	sock.Init(0);
}

void Client::connect(string address)
{
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
	origin_history.clear();
	snapshots.resize(CLIENT_SNAPSHOT_HISTORY);
	commands.resize(CLIENT_MOVE_HISTORY);
	origin_history.resize(36);

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
		console_printf("Couldn't connect to server\n");
		state = CS_DISCONNECTED;
		return;
	}
	double delta = GetTime() - attempt_time;
	if (delta < CONNECT_RETRY)
		return;
	attempt_time = GetTime();
	connect_attempts++;

	uint8_t buffer[256];
	ByteWriter writer(buffer, 256);
	writer.WriteLong(CONNECTIONLESS_SEQUENCE);
	writer.write_string("connect");
	writer.EndWrite();
	sock.Send(buffer, writer.BytesWritten(), server.remote_addr);
}
void Client::Disconnect()
{
	if (state == CS_DISCONNECTED)
		return;
	console_printf("Disconnecting...\n");
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
	string address = std::move(serveraddr);
	console_printf("Reconnecting to server: %s\n", address);
	Disconnect();
	connect(address);
}

Move_Command& Client::get_command(int sequence) {
	return commands.at(sequence % CLIENT_MOVE_HISTORY);
}

void Client::run_prediction()
{
	if (get_state() != CS_SPAWNED)
		return;

	if (!cfg_do_predict->integer)
		return;

	// predict commands from outgoing ack'ed to current outgoing
	// TODO: dont repeat commands unless a new snapshot arrived
	int start = OutSequenceAk();	// last command server has recieved
	int end = OutSequence();		// current frame's command
	int commands_to_run = end - start;
	if (commands_to_run > CLIENT_MOVE_HISTORY)	// overflow
		return;

	// restore local player's state to last authoritative snapshot 
	int incoming_seq = InSequence();
	Snapshot* last_auth_state = &snapshots.at(incoming_seq % CLIENT_SNAPSHOT_HISTORY);
	PlayerState predicted_player = last_auth_state->pstate;
	Entity& player = engine.ents[engine.player_num()];
	player.FromPlayerState(&predicted_player);

	// run physics code for commands yet to recieve a snapshot for
	for (int i = start + 1; i < end; i++) {
		Move_Command& cmd = get_command(i);
		cmd.first_sim = i == (end - 1);		// flag so only local effects are created once
		player_physics_update(&player, cmd);
	}
	// runs animation and item code for current frame only
	player_post_physics(&player, get_command(end - 1));
	
	// dont advance frames if animation is being controlled server side
	if(!(player.flags & EF_FORCED_ANIMATION))
		player.anim.AdvanceFrame(engine.tick_interval);

	// for prediction errors
	origin_history.at((end-1)%origin_history.size()) = engine.local_player().position;
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
bool teleport_distance(vec3 a, vec3 b, float maxspeed, float dt)
{
	float len = glm::length(a - b);
	float t = len / maxspeed;
	return dt < t;
}

Interp_Entry* Entity_Interp::GetStateFromIndex(int index)
{
	return &hist[NegModulo(index, HIST_SIZE)];
}

float interpolate_modulo(float start, float end, float mod, float alpha)
{
	float d1 = glm::abs(end - start);
	float d2 = mod - d1;


	if (d1 <= d2) {
		return glm::mix(start, end, alpha);
	}
	else {
		if (start >= end)
			return fmod(start + (alpha * d2), mod);
		else
			return fmod(end + ((1 - alpha) * d2), mod);
	}
}

float InterpolateAnimation(Animation* a, float start, float end, float alpha)
{
	// check distances
	float clip_len = a->total_duration;
	return interpolate_modulo(start, end, clip_len, alpha);
}

void set_entity_interp_vars(Entity& e, Interp_Entry& i)
{
	e.position = i.position;
	e.rotation = i.angles;
	e.anim.leg_anim = i.legs_anim;
	e.anim.anim = i.main_anim;
	e.anim.leg_frame = i.la_frame;
	e.anim.frame = i.ma_frame;
}


void Client::interpolate_states()
{
	if (!interpolate->integer)
		return;

	auto cl = engine.cl;
	// FIXME
	double rendering_time = engine.tick * engine.tick_interval - (engine.cl->cfg_interp_time->real);
	
	// interpolate local player error
	if (smooth_time > 0) {
		if (smooth_time == smooth_error_time->real)
			printf("starting smooth\n");

		vec3 delta = engine.local_player().position - last_origin;
		vec3 new_position = last_origin + delta * (1 - (smooth_time / smooth_error_time->real));
		engine.local_player().position = new_position;
		smooth_time -= engine.frame_time;

		if (smooth_time <= 0)
			printf("end smooth\n");
	}
	last_origin = engine.local_player().position;

	// interpolate other entities
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
			set_entity_interp_vars(ent, *ce.GetLastState());
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
			set_entity_interp_vars(ent, *s2);
			continue;
		}
		else if (s2->tick == -1 || s2->tick <= s1->tick) {
			set_entity_interp_vars(ent, *s1);
			continue;
		}

		ASSERT(s1->tick < s2->tick);
		ASSERT(s1->tick * engine.tick_interval <= rendering_time && s2->tick * engine.tick_interval >= rendering_time);

		if (teleport_distance(s1->position, s2->position, 20.0, (s2->tick-s1->tick)*engine.tick_interval)) {
			set_entity_interp_vars(ent, *s1);
			printf("teleport\n");
			continue;
		}

		float midlerp = MidLerp(s1->tick * engine.tick_interval, s2->tick * engine.tick_interval, rendering_time);
		Interp_Entry interpstate = *s1;
		interpstate.position = glm::mix(s1->position, s2->position, midlerp);
		
		for (int i = 0; i < 3; i++) {
			float d = s1->angles[i] - s2->angles[i];
			interpstate.angles[i] = interpolate_modulo(s1->angles[i], s2->angles[i], TWOPI, midlerp);
		}

		if (ent.model && ent.model->animations && s1->legs_anim == s2->legs_anim) {
			int anim = s1->legs_anim;
			if (anim >= 0 && anim < ent.model->animations->clips.size()) {

				interpstate.la_frame = InterpolateAnimation(&ent.model->animations->clips[s1->legs_anim],
					s1->la_frame, s2->la_frame, midlerp);
			}

		}
		if (ent.model && ent.model->animations && s1->main_anim == s2->main_anim) {
			int anim = s1->main_anim;
			if (anim >= 0 && anim < ent.model->animations->clips.size()) {

				interpstate.ma_frame = InterpolateAnimation(&ent.model->animations->clips[s1->main_anim],
					s1->ma_frame, s2->ma_frame, midlerp);
			}
		}

		set_entity_interp_vars(ent, interpstate);
	}
}

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

// has extra logic to properly replicate
void local_player_on_new_entity_state(EntityState& es, Entity& p)
{
	p.type = (Ent_Type)es.type;
	p.position = es.position;
	p.rotation = es.angles;

	p.item = es.item;
	p.solid = es.solid;

	p.flags = es.flags;

	// if we already have a model, dont override the animations
	p.model_index = es.model_idx;
	const Model* next_model = nullptr;
	if (p.model_index >= 0 && p.model_index < media.gamemodels.size())
		next_model = media.gamemodels.at(p.model_index);
	if (next_model != p.model) {
		p.model = next_model;
		if (p.model && p.model->bones.size() > 0)
			p.anim.set_model(p.model);
	}
	// force animation if server is requesting it
	if (p.flags & EF_FORCED_ANIMATION) {
		p.anim.leg_anim = es.leganim;
		p.anim.leg_frame = es.leganim_frame;
		p.anim.anim = es.mainanim;
		p.anim.frame = es.mainanim_frame;
	}
}

void Client::read_snapshot(Snapshot* s)
{
	Snapshot* snapshot = s;
	for (int i = 0; i < Snapshot::MAX_ENTS; i++) {
		Entity_Interp& interp = interpolation_data[i];
		Entity& e = engine.ents[i];
		EntityState& state = snapshot->entities[i];

		int lasttype = e.type;

		if (state.type == ET_FREE) {
			e.type = ET_FREE;
			continue;
		}

		e.index = i;
		// replicate variables to entity
		if (i == engine.player_num())
			local_player_on_new_entity_state(state, e);
		else
			e.from_entity_state(state);

		// save off some vars for rendering interpolation (not for local clients player)
		if (i != engine.player_num()) {
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
	}

	// update prediction error data
	int sequence_i_sent = OutSequenceAk();
	if (OutSequence() - sequence_i_sent < origin_history.size()) {
		vec3 delta = s->pstate.position - origin_history.at((sequence_i_sent+offset_debug) % origin_history.size());
		// FIXME check for teleport
		float len = glm::length(delta);
		if(len > 0.05 && len < 10 && smooth_time <= 0.0)
			smooth_time = smooth_error_time->real;
	}

	// build physics world for prediction updates later in frame AND subsequent frames until next packet
	engine.build_physics_world(0.f);
}