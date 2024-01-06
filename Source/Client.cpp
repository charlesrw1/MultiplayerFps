#include "Net.h"
#include "Util.h"
#include "Client.h"
#include "CoreTypes.h"
#include "Movement.h"
#include "MeshBuilder.h"
#include "Media.h"
#include "Config.h"

Client client;

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
	cl_game.Init();
	out_commands.resize(CLIENT_MOVE_HISTORY);
	time = 0.0;
	tick = 0;

	snapshots.resize(CLIENT_SNAPSHOT_HISTORY);

	last_recieved_server_tick = -1;
	cur_snapshot_idx = 0;

	initialized = true;

}

void Client::Disconnect()
{
	DebugOut("disconnecting\n");
	if (GetConState() == Disconnected)
		return;
	server_mgr.Disconnect();
	cl_game.ClearState();
	tick = 0;
	time = 0.0;
}
void Client::Reconnect()
{
	DebugOut("reconnecting\n");
	IPAndPort addr = server_mgr.GetCurrentServerAddr();
	Disconnect();
	Connect(addr);
}
void Client::Connect(IPAndPort addr)
{
	DebugOut("connecting to %s", addr.ToString().c_str());
	server_mgr.Connect(addr);
}
ClientConnectionState Client::GetConState() const
{
	return server_mgr.GetState();
}
int Client::GetPlayerNum() const
{
	return server_mgr.ClientNum();
}

MoveCommand* Client::GetCommand(int sequence) {
	return &out_commands.at(sequence % out_commands.size());
}

int Client::GetCurrentSequence() const
{
	return server_mgr.OutSequence();
}
int Client::GetLastSequenceAcked() const
{
	return server_mgr.OutSequenceAk();
}
void Client::CheckLocalServerIsRunning()
{
	if (GetConState() == Disconnected && IsServerActive()) {
		// connect to the local server
		IPAndPort serv_addr;
		serv_addr.SetIp(127, 0, 0, 1);
		serv_addr.port = cfg.find_var("host_port")->integer;
		server_mgr.Connect(serv_addr);
	}
}
void PlayerStateToClEntState(EntityState* entstate, PlayerState* state)
{
	entstate->position = state->position;
	entstate->angles = state->angles;
	entstate->ducking = state->ducking;
	entstate->type = Ent_Player;
}


void Client::DoViewAngleUpdate()
{
	float x_off = core.input.mouse_delta_x;
	float y_off = core.input.mouse_delta_y;
	x_off *= cfg_mouse_sensitivity->real;
	y_off *= cfg_mouse_sensitivity->real;

	glm::vec3 view_angles = this->view_angles;
	view_angles.x -= y_off;	// pitch
	view_angles.y += x_off;	// yaw
	view_angles.x = glm::clamp(view_angles.x, -HALFPI + 0.01f, HALFPI - 0.01f);
	view_angles.y = fmod(view_angles.y, TWOPI);
	this->view_angles = view_angles;
}

void Client::CreateMoveCmd()
{
	if (core.mouse_grabbed) {
		DoViewAngleUpdate();
	}

	MoveCommand new_cmd{};
	new_cmd.view_angles = view_angles;

	bool* keys = core.input.keyboard;
	if (keys[SDL_SCANCODE_W])
		new_cmd.forward_move += 1.f;
	if (keys[SDL_SCANCODE_S])
		new_cmd.forward_move -= 1.f;
	if (keys[SDL_SCANCODE_A])
		new_cmd.lateral_move += 1.f;
	if (keys[SDL_SCANCODE_D])
		new_cmd.lateral_move -= 1.f;
	if (keys[SDL_SCANCODE_SPACE])
		new_cmd.button_mask |= CmdBtn_Jump;
	if (keys[SDL_SCANCODE_LSHIFT])
		new_cmd.button_mask |= CmdBtn_Duck;
	if (keys[SDL_SCANCODE_Q])
		new_cmd.button_mask |= CmdBtn_PFire;
	if (keys[SDL_SCANCODE_E])
		new_cmd.button_mask |= CmdBtn_Reload;
	if (keys[SDL_SCANCODE_Z])
		new_cmd.up_move += 1.f;
	if (keys[SDL_SCANCODE_X])
		new_cmd.up_move -= 1.f;

	new_cmd.tick = tick;

	// quantize and unquantize for local prediction
	new_cmd.forward_move = MoveCommand::unquantize(MoveCommand::quantize(new_cmd.forward_move));
	new_cmd.lateral_move = MoveCommand::unquantize(MoveCommand::quantize(new_cmd.lateral_move));
	new_cmd.up_move = MoveCommand::unquantize(MoveCommand::quantize(new_cmd.up_move));

	*GetCommand(GetCurrentSequence()) = new_cmd;
}


void Client::RunPrediction()
{
	if (GetConState() != Spawned)
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

	cl_game.BuildPhysicsWorld();

	// FIXME:
	EntityState last_estate = last_auth_state->entities[GetPlayerNum()];
	PlayerState pred_state = last_auth_state->pstate;

	for (int i = start + 1; i < end; i++) {
		MoveCommand* cmd = GetCommand(i);
		PlayerState next_state;
		cl_game.RunCommand(&pred_state, &next_state, *cmd, i==(end-1));
		pred_state = next_state;
	}

	ClientEntity* ent = GetLocalPlayer();
	PlayerStateToClEntState(&last_estate, &pred_state);
	ent->OnRecieveUpdate(&last_estate, tick);
	lastpredicted = pred_state;
}

void Client::FixedUpdateInput(double dt)
{
	if (!initialized)
		return;
	if (GetConState() >= Connected) {
		CreateMoveCmd();
	}
	server_mgr.SendMovesAndMessages();
	CheckLocalServerIsRunning();
	server_mgr.TrySendingConnect();
}
void Client::FixedUpdateRead(double dt)
{
	if (!initialized)
		return;
	if (GetConState() == Spawned) {
		client.tick += 1;
		client.time = client.tick * core.tick_interval;
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
	return &hist.at(NegModulo(current_hist_index - 1, NUM_STORED_STATES));
}

void ClientEntity::OnRecieveUpdate(const EntityState* state, int tick)
{
	hist.at(current_hist_index) = { tick, *state };
	current_hist_index = (current_hist_index + 1) % hist.size();

	if (state->model_idx != interpstate.model_idx || model==nullptr) {
		if (state->model_idx >= 0 && state->model_idx < media.gamemodels.size()) {
			model = media.gamemodels.at(state->model_idx);
		}
		else {
			printf("client: recieved invalid model_index (%d)\n", state->model_idx);
		}
	}
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


// Called every graphics frame
void ClientEntity::InterpolateState(double time, double tickinterval) {
	if (!active)
		return;
	
	//interpstate = GetLastState()->state;
	//return;

	int i = 0;
	int last_entry = NegModulo(current_hist_index - 1, NUM_STORED_STATES);
	for (; i < NUM_STORED_STATES; i++) {
		auto e = GetStateFromIndex(current_hist_index - 1 - i);
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

void Client::SetNewTickRate(float tick_rate)
{
	if (!IsServerActive()) {
		core.tick_interval = 1.0 / tick_rate;
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

void Client::SetupSnapshot(Snapshot* s)
{
	Snapshot* snapshot = s;
	ClientGame* game = &cl_game;
	for (int i = 0; i < Snapshot::MAX_ENTS; i++) {
		ClientEntity* ce = &game->entities[i];


		// local player doesnt fill interp history here
		if (i == GetPlayerNum()) {
			ce->active = true;
			continue;
		}

		if (snapshot->entities[i].type == Ent_Free) {
			ce->active = false;
			continue;
		}
		ce->active = true;

		StateEntry* lastentry = ce->GetLastState();
		if (i == MAX_CLIENTS) {
			printf("slot2: %d\n", lastentry->state.model_idx);
		}
		if (lastentry->state.type == Ent_Free) {
			ce->ClearState();
		}
		ce->OnRecieveUpdate(&snapshot->entities[i], tick);
	}

	ClearEntsThatDidntUpdate(tick);
}

void Client::ClearEntsThatDidntUpdate(int what_tick)
{
	ClientGame* game = &cl_game;
	for (int i = 0; i < game->entities.size(); i++) {
		ClientEntity* ce = &game->entities[i];
		if (i == GetPlayerNum()) continue;
		if (ce->active && ce->GetLastState()->tick != what_tick) {
			game->entities[i].active = false;
		}


	}
}