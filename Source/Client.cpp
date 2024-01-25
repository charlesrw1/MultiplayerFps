#include "Net.h"
#include "Util.h"
#include "Client.h"
#include "Game_Engine.h"
#include "Player.h"
#include "MeshBuilder.h"
#include "Config.h"

//#define DebugOut(fmt, ...)

void Client::init()
{
	interp_time			= cfg.get_var("interp_time", "0.1");
	fake_lag			= cfg.get_var("fake_lag", "0");
	fake_loss			= cfg.get_var("fake_loss", "0");
	cl_time_out			= cfg.get_var("cl_time_out", "5.f");
	interpolate			= cfg.get_var("interpolate", "1");
	smooth_error_time	= cfg.get_var("smooth_error", "1.0");
	client_predict		= cfg.get_var("client_predict", "1");
	dont_replicate_player = cfg.get_var("dont_replicate_player", "0");
	time_reset_threshold = cfg.get_var("time_reset_threshold", "0.1");
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

	engine.set_state(ENGINE_LOADING);

	TrySendingConnect();
}

void Client::TrySendingConnect()
{
	ASSERT(engine.state == ENGINE_LOADING);

	const int MAX_CONNECT_ATTEMPTS = 10;
	const float CONNECT_RETRY = 1.f;

	if (state != CS_TRYINGCONNECT)
		return;
	if (connect_attempts >= MAX_CONNECT_ATTEMPTS) {
		sys_print("Couldn't connect to server\n");
		state = CS_DISCONNECTED;

		engine.set_state(ENGINE_MENU);
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
void Client::Disconnect(const char* debug_reason)
{
	if (state == CS_DISCONNECTED)
		return;
	sys_print("Disconnecting from server because %s\n", debug_reason);
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

	engine.set_state(ENGINE_MENU);
}


void Client::Reconnect()
{
	string address = std::move(serveraddr);
	sys_print("Reconnecting to server: %s\n", address);
	Disconnect("reconnecting");
	connect(address);
}

Move_Command& Client::get_command(int sequence) {
	return commands.at(sequence % CLIENT_MOVE_HISTORY);
}

void Client::run_prediction()
{
	if (get_state() != CS_SPAWNED)
		return;

	if (!client_predict->integer)
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
	Frame* auth = &snapshots.at(incoming_seq % CLIENT_SNAPSHOT_HISTORY);
	Entity& player = engine.local_player();
	
	if (dont_replicate_player->integer)
		start = end - 2;	// only sim current frame
	else if (auth->player_offset == -1) {
		sys_print("player state not in packet?, skipping prediction\n");
		return;
	}
	else
	{
		Packed_Entity pe(auth, auth->player_offset, get_entity_baseline()->num_bytes_including_index);	// FIXME:
		ByteReader buf = pe.get_buf();
		// sanity check
		ASSERT(pe.index == client_num);
		read_entity(&player, buf, Net_Prop::PLAYER_PROP, false);		// replicate player props
	}

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

void set_entity_from_interp(Entity& e, Interp_Entry& i)
{
	e.position = i.position;
	e.rotation = i.angles;
	//e.anim.m = i.torso;
	//e.anim.legs = i.legs;
}
void set_interp_from_entity(Interp_Entry& i, Entity& e)
{
	i.legs = e.anim.legs;
	i.torso = e.anim.m;
	i.position = e.position;
	i.angles = e.rotation;
}

// this isnt perfect, whatev
void interp_animator_layer(Animator_Layer& l1, Animator_Layer& l2, const Model* model, float midlerp, Animator_Layer& interp)
{
	if (l1.anim == l2.anim) {
		int anim = l1.anim;
		interp.anim = l1.anim;
		if (anim >= 0 && anim < model->animations->clips.size()) {
			
			interp.frame = InterpolateAnimation(&model->animations->clips[anim],
				l1.frame, l2.frame, midlerp);
		}

		interp.blend_anim = l2.blend_anim;
		interp.blend_frame = l2.blend_frame;
		interp.blend_time = l2.blend_time;
		if (l1.blend_anim == l2.blend_anim)
			interp.blend_remaining = glm::mix(l1.blend_remaining, l2.blend_remaining, midlerp);
		else
			interp.blend_remaining = l2.blend_remaining;
	}
	else
	{
		interp = l2;
	}
}


void Client::interpolate_states()
{
	static Config_Var* dbg_print = cfg.get_var("dbg_i_state", "0");

	if (!interpolate->integer)
		return;

	auto& cl = engine.cl;
	// FIXME
	double rendering_time = engine.tick * engine.tick_interval - (engine.cl->interp_time->real);
	
	// interpolate local player error
	if (smooth_time > 0 && dont_replicate_player->integer == 0) {
		if (smooth_time == smooth_error_time->real)
			sys_print("starting smooth\n");

		vec3 delta = engine.local_player().position - last_origin;
		vec3 new_position = last_origin + delta * (1 - (smooth_time / smooth_error_time->real));
		engine.local_player().position = new_position;
		smooth_time -= engine.frame_time;

		if (smooth_time <= 0)
			sys_print("end smooth\n");
	}
	last_origin = engine.local_player().position;

	// interpolate other entities
	for (int i = 0; i < MAX_GAME_ENTS; i++) {
		// local player doesn't interpolate
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
			if (dbg_print->integer)
				sys_print("using last state\n");
			set_entity_from_interp(ent, *ce.GetLastState());
			continue;
		}
		// else, interpolate
		Interp_Entry* s1 = ce.GetStateFromIndex(last_entry - entry_index);
		Interp_Entry* s2 = ce.GetStateFromIndex(last_entry - entry_index + 1);

		if (s1->tick == -1 && s2->tick == -1) {
			if (dbg_print->integer)
				sys_print("no state\n");
			continue;
		}
		else if (s1->tick == -1) {
			if (dbg_print->integer)
				sys_print("no start state\n");
			set_entity_from_interp(ent, *s2);
			continue;
		}
		else if (s2->tick == -1 || s2->tick <= s1->tick) {
			if (dbg_print->integer)
				sys_print("no end state\n");
			set_entity_from_interp(ent, *s1);
			continue;
		}

		ASSERT(s1->tick < s2->tick);
		ASSERT(s1->tick * engine.tick_interval <= rendering_time && s2->tick * engine.tick_interval >= rendering_time);

		if (teleport_distance(s1->position, s2->position, 20.0, (s2->tick-s1->tick)*engine.tick_interval)) {
			set_entity_from_interp(ent, *s1);
			if (dbg_print->integer)
				sys_print("teleport\n");
			continue;
		}

		float midlerp = MidLerp(s1->tick * engine.tick_interval, s2->tick * engine.tick_interval, rendering_time);
		Interp_Entry interpstate = *s1;

		Config_Var* interp_force = cfg.get_var("dbg_interp_force", "0");
		if (interp_force->integer == 0)
			interpstate.position = glm::mix(s1->position, s2->position, midlerp);
		else if (interp_force->integer == 1)
			interpstate.position = s1->position;
		else if (interp_force->integer == 2)
			interpstate.position = s2->position;
		else
			interpstate.position = glm::mix(s1->position, s2->position,1 - midlerp);
		
		for (int i = 0; i < 3; i++) {
			float d = s1->angles[i] - s2->angles[i];
			interpstate.angles[i] = interpolate_modulo(s1->angles[i], s2->angles[i], TWOPI, midlerp);
		}
		if (ent.model && ent.model->animations) {
			interp_animator_layer(s1->legs, s2->legs, ent.model, midlerp, interpstate.legs);
			interp_animator_layer(s1->torso, s2->torso, ent.model, midlerp, interpstate.torso);
		}

		set_entity_from_interp(ent, interpstate);
	}
}

Frame* Client::GetCurrentSnapshot()
{
	return &snapshots.at(InSequence() % CLIENT_SNAPSHOT_HISTORY);
}
Frame* Client::FindSnapshotForTick(int tick)
{
	for (int i = 0; i < snapshots.size(); i++) {
		if (snapshots[i].tick == tick)
			return &snapshots[i];
	}
	return nullptr;
}

// will add/subtract a small value to adjust the number of ticks running
float Client::adjust_time_step(int ticks_runnning)
{
	static Config_Var* cl_adjust_time = cfg.get_var("cl_adjust_time", "1");
	static Config_Var* cl_time_adjust = cfg.get_var("cl_max_adjust", "1");

	if (!cl_adjust_time->integer)
		return 0.f;

	float adjust_total = 0.f;
	float max_time_adjust = cl_time_adjust->real / 1000.f;
	
	for (int i = 0; i < ticks_runnning; i++) {
		if (abs(time_delta) < 0.0001)
			break;


		if (abs(time_delta) < max_time_adjust) {
			float adjust = time_delta;
			time_delta = 0;
			adjust_total += adjust;
		}
		else {
			float adjust = (time_delta < 0) ? -max_time_adjust : max_time_adjust;
			time_delta -= adjust;
			adjust_total += adjust;
		}
	}

	static Config_Var* dbg_adjust_neg = cfg.get_var("dbg_aj_t_neg", "1");
	if (dbg_adjust_neg->integer)
		return -adjust_total;
	else
		return adjust_total;
}

void Client::read_snapshot(Frame* snapshot)
{
	// now: build a local state packet to delta entities from
	ByteWriter wr(snapshot->data, Frame::MAX_FRAME_SNAPSHOT_DATA);

	for (int i = 0; i < NUM_GAME_ENTS; i++) {
		Entity& e = engine.ents[i];
		if (!e.active()) continue;

		if (i == client_num) {
			snapshot->player_offset = wr.BytesWritten();
		}

		wr.WriteBits(i, ENTITY_BITS);
		write_full_entity(&e, wr);
		wr.AlignToByteBoundary();

		snapshot->num_ents_this_frame++;
	}
	wr.WriteBits(ENTITY_SENTINAL, ENTITY_BITS);
	wr.EndWrite();

	for (int i = 0; i < MAX_GAME_ENTS; i++)
	{
		Entity& e = engine.ents[i];
		if (!e.active()) continue;

		Entity_Interp& interp = interpolation_data[i];
		e.index = i;	// FIXME

		const Model* next_model = nullptr;
		if (e.model_index != -1)
			next_model = media.get_game_model_from_index(e.model_index);
		if (next_model != e.model) {
			e.model = next_model;
			if (e.model && e.model->bones.size() > 0)
				e.anim.set_model(e.model);
		}


		if (i != engine.player_num()) {
			Interp_Entry& entry = interp.hist[interp.hist_index];
			entry.tick = snapshot->tick;
			set_interp_from_entity(entry, e);

			interp.hist_index = (interp.hist_index + 1) % Entity_Interp::HIST_SIZE;
		}
	}

	// update prediction error data
	int sequence_i_sent = OutSequenceAk();
	if (OutSequence() - sequence_i_sent < origin_history.size()) {
		Entity& player = engine.ents[client_num];
		vec3 delta = player.position - origin_history.at((sequence_i_sent + offset_debug) % origin_history.size());
		// FIXME check for teleport
		float len = glm::length(delta);
		if (len > 0.05 && len < 10 && smooth_time <= 0.0)
			smooth_time = smooth_error_time->real;
	}

	// build physics world for prediction updates later in frame AND subsequent frames until next packet
	engine.build_physics_world(0.f);

}