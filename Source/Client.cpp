#include "Net.h"
#include "Util.h"
#include "Client.h"
#include "CoreTypes.h"
#include "Movement.h"
#include "MeshBuilder.h"

Client client;

#define DebugOut(fmt, ...) NetDebugPrintf("client: " fmt, __VA_ARGS__)
//#define DebugOut(fmt, ...)

void Client::Init()
{
	server_mgr.Init(this);
	cl_game.Init();
	out_commands.resize(CLIENT_MOVE_HISTORY);
	time = 0.0;
	tick = 0;

	snapshots.resize(CLIENT_SNAPSHOT_HISTORY);

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
		serv_addr.port = SERVER_PORT;
		server_mgr.Connect(serv_addr);
	}
}
void Client_TraceCallback(GeomContact* out, PhysContainer obj, bool closest, bool double_sided, int ignore_ent)
{
	TraceAgainstLevel(client.cl_game.level, out, obj, closest, double_sided);
}
void RunClientPhysics(const PlayerState* in, PlayerState* out, const MoveCommand* cmd)
{
	MeshBuilder b;

	PlayerMovement move;
	move.cmd = *cmd;
	move.deltat = core.tick_interval;
	move.phys_debug = &b;
	move.in_state = *in;
	move.trace_callback = Client_TraceCallback;
	move.Run();


	*out = *move.GetOutputState();
}
void PlayerStateToClEntState(EntityState* entstate, PlayerState* state)
{
	entstate->position = state->position;
	entstate->angles = state->angles;
	entstate->ducking = state->ducking;
	entstate->type = Ent_Player;
}
void DoClientGameUpdate(double dt)
{



}


void Client::DoViewAngleUpdate()
{
	float x_off = core.input.mouse_delta_x;
	float y_off = core.input.mouse_delta_y;
	const float sensitivity = 0.01;
	x_off *= sensitivity;
	y_off *= sensitivity;

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
		new_cmd.button_mask |= CmdBtn_Misc1;
	if (keys[SDL_SCANCODE_E])
		new_cmd.button_mask |= CmdBtn_Misc2;
	if (keys[SDL_SCANCODE_Z])
		new_cmd.up_move += 1.f;
	if (keys[SDL_SCANCODE_X])
		new_cmd.up_move -= 1.f;

	new_cmd.tick = tick;

	// quantize and unquantize 
	//new_cmd.forward_move = float(char(new_cmd.forward_move * 128.f)) / 128.f;
	//new_cmd.up_move = float(char(new_cmd.up_move * 128.f)) / 128.f;
	//new_cmd.lateral_move = float(char(new_cmd.lateral_move * 128.f)) / 128.f;


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

	// FIXME:
	EntityState last_estate = last_auth_state->entities[GetPlayerNum()];
	PlayerState pred_state = last_auth_state->pstate;
	pred_state.position =last_estate.position;
	pred_state.angles = last_estate.angles;
	pred_state.ducking = last_estate.ducking;

	for (int i = start + 1; i < end; i++) {
		MoveCommand* cmd = GetCommand(i);
		PlayerState next_state;
		RunClientPhysics(&pred_state, &next_state, cmd);
		pred_state = next_state;
	}

	ClientEntity* ent = GetLocalPlayer();
	PlayerStateToClEntState(&last_estate, &pred_state);
	ent->AddStateToHist(&last_estate, tick);
	lastpredicted = pred_state;

	//ent->state = pred_state.estate;
	//ent->transform_hist.at(ent->current_hist_index % ent->transform_hist.size()).tick = client.tick;
	//ent->transform_hist.at(ent->current_hist_index % ent->transform_hist.size()).origin = ent->state.position;
	//ent->current_hist_index++;
	//ent->current_hist_index %= ent->transform_hist.size();
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
	if (GetConState() == Spawned)
		client.tick += 1;
	server_mgr.ReadPackets();
	RunPrediction();

	cl_game.UpdateViewModelOffsets();
}
void Client::PreRenderUpdate(double frametime)
{
	DoClientGameUpdate(frametime);
	
	if (IsClientActive() && IsInGame()) {
		// interpoalte entities for rendering
		ClientGame* game = &cl_game;
		const double cl_interp = 2.0/DEFAULT_SNAPSHOT_RATE;
		double rendering_time = client.tick * core.tick_interval - cl_interp;
		for (int i = 0; i < game->entities.size(); i++) {
			if (game->entities[i].active && i != GetPlayerNum()) {
				game->entities[i].InterpolateState(rendering_time, core.tick_interval);
			}
		}

		double rendering_time_client = client.tick * core.tick_interval - core.frame_remainder;
		ClientEntity* local = GetLocalPlayer();
		local->interpstate = local->GetLastState()->state;
		//local->InterpolateState(rendering_time_client, core.tick_interval);
	}

	//if(core.mouse_grabbed)
	//	DoViewAngleUpdate();	// one last time before render
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
	if (s1->state.leganim == s2->state.leganim) {
		//interpstate.leganim_frame = glm::mix(s1->state.leganim_frame, s2->state.leganim_frame, midlerp);
		
	}
	if (s1->state.mainanim == s2->state.mainanim) {
		// fixme
	}


}

void Client::OnRecieveNewSnapshot()
{
	Snapshot* snapshot = &snapshots.at(server_mgr.InSequence() % CLIENT_SNAPSHOT_HISTORY);
	ClientGame* game = &cl_game;
	for (int i = 0; i < 24; i++) {
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
		if (lastentry->state.type == Ent_Free) {
			ce->ClearState();
		}
		ce->AddStateToHist(&snapshot->entities[i], tick);
	}
}