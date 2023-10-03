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
	view_mgr.Init();
	cl_game.Init();
	out_commands.resize(CLIENT_MOVE_HISTORY);
	time = 0.0;
	tick = 0;

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
	IPAndPort addr = server_mgr.server.remote_addr;
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
	return server_mgr.state;
}
int Client::GetPlayerNum() const
{
	return server_mgr.client_num;
}

MoveCommand* Client::GetCommand(int sequence) {
	return &out_commands.at(sequence % out_commands.size());
}

int Client::GetCurrentSequence() const
{
	return server_mgr.server.out_sequence;
}
int Client::GetLastSequenceAcked() const
{
	return server_mgr.server.out_sequence_ak;
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
void Client_TraceCallback(ColliderCastResult* out, PhysContainer obj, bool closest, bool double_sided)
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


void Client::CreateMoveCmd()
{
	if (core.mouse_grabbed) {
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
	Snapshot* last_auth_state = &cl_game.snapshots.at(incoming_seq % CLIENT_SNAPSHOT_HISTORY);
	PlayerState pred_state;

	// FIXME:
	EntityState* TEMP = &last_auth_state->entities[GetPlayerNum()];
	pred_state = last_auth_state->pstate;
	pred_state.position = TEMP->position;
	pred_state.angles = TEMP->angles;
	pred_state.ducking = TEMP->ducking;

	for (int i = start + 1; i < end; i++) {
		MoveCommand* cmd = GetCommand(i);
		PlayerState next_state;
		RunClientPhysics(&pred_state, &next_state, cmd);
		pred_state = next_state;
	}

	ClientEntity* ent = GetLocalPlayer();
	PlayerStateToClEntState(&ent->state, &pred_state);

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
}
void Client::PreRenderUpdate(double frametime)
{
	view_mgr.Update();
	DoClientGameUpdate(frametime);
}
