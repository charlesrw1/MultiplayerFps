#include "Server.h"
#include "Game_Engine.h"
#define DebugOut(fmt, ...) NetDebugPrintf("server: " fmt, __VA_ARGS__)

RemoteClient::RemoteClient(Server* sv, int slot)
{
	myserver = sv;
	client_num = slot;
}

/*
client sends input to server
server queues it in a buffer

server goes to update clients player and uses the inputs that have buffered up
if the buffer is empty for X ticks, then start duplicating last command
when updating character


*/

void RemoteClient::init(IPAndPort address)
{
	state = SCS_CONNECTED;
	connection = Connection();
	connection.Init(&myserver->socket, address);
	local_client = false;
	baseline = -1;
	next_snapshot_time = 0.f;

	commands.resize(5);
}

void RemoteClient::Disconnect()
{
	if (state == SCS_DEAD)
		return;
	DebugOut("Disconnecting client %s\n", connection.remote_addr.ToString().c_str());

	if (state == SCS_SPAWNED) {
		engine.client_leave(client_num);
	}

	uint8_t buffer[8];
	ByteWriter writer(buffer, 8);
	writer.WriteByte(SV_DISCONNECT);
	writer.EndWrite();
	connection.Send(buffer, writer.BytesWritten());

	state = SCS_DEAD;
	connection.Clear();
}

extern void read_delta_move_command(ByteReader& msg, Move_Command& to);	// defined in clientmgr.cpp

void RemoteClient::OnMoveCmd(ByteReader& msg)
{
	Move_Command commands[16];
	int tick = msg.ReadLong();
	int num_commands = msg.ReadByte();
	if (num_commands > 16 || num_commands <= 0) {
		printf("Invalid command count \n");
		return;
	}
	Move_Command last = Move_Command();
	for (int i = 0; i < num_commands; i++) {
		commands[i] = last;
		commands[i].tick = tick - i;
		read_delta_move_command(msg, commands[i]);
		last = commands[i];
	}

	// Not ready to run the input yet
	if (state != SCS_SPAWNED)
		return;

	int commands_to_run = 1 + connection.dropped;	// FIXME: have to run more than 1 command if client send rate is less than tick rate
	commands_to_run = glm::min(commands_to_run, num_commands);

	if (commands_to_run > 1) {
		printf("simming dropped %d cmds\n", commands_to_run - 1);
	}
	// FIXME: exploit to send inputs at increased rate
	for (int i = commands_to_run - 1; i >= 0; i--) {
		Move_Command c = commands[i];
		engine.execute_player_move(client_num, c);
	}
}

void RemoteClient::OnTextCommand(ByteReader& msg)
{
	uint8_t cmd_len = msg.ReadByte();
	std::string cmd;
	cmd.resize(cmd_len);
	msg.ReadBytes((uint8_t*)&cmd[0], cmd_len);

	if (msg.HasFailed())
		Disconnect();
}

void RemoteClient::Update()
{
	if (!IsConnected() || local_client)
		return;

	if (!IsSpawned()) {
		connection.Send(nullptr, 0);
		return;
	}

	next_snapshot_time -= engine.tick_interval;
	if (next_snapshot_time > 0.f)
		return;

	next_snapshot_time += (1.0 / myserver->cfg_snapshot_rate->real);

	uint8_t buffer[MAX_PAYLOAD_SIZE];
	ByteWriter writer(buffer, MAX_PAYLOAD_SIZE);

	writer.WriteByte(SV_TICK);
	writer.WriteLong(engine.tick);

	writer.WriteByte(SV_SNAPSHOT);
	myserver->WriteDeltaSnapshot(writer, baseline, client_num);
	writer.EndWrite();
	connection.Send(buffer, writer.BytesWritten());
}

void RemoteClient::OnPacket(ByteReader& buf)
{
	if (state != SCS_SPAWNED) {
		state = SCS_SPAWNED;
		DebugOut("Spawning client %d : %s\n", client_num, GetIPStr().c_str());
		engine.make_client(client_num);
	}


	for(;;)
	{
		buf.AlignToByteBoundary();
		if (buf.IsEof())
			return;
		if (buf.HasFailed()) {
			DebugOut("Read fail\n");
			Disconnect();
			return;
		}

		uint8_t command = buf.ReadByte();
		switch (command)
		{
		case CL_NOP:
			break;
		case CL_INPUT:
			OnMoveCmd(buf);
			break;
		case CL_QUIT:
			Disconnect();
			break;
		case CL_SET_BASELINE: {
			int newbaseline = buf.ReadLong();
			baseline = newbaseline;
		}
			break;
		case CL_TEXT:
			OnTextCommand(buf);
			break;
		default:
			DebugOut("Unknown message\n");
			Disconnect();
			return;
			break;
		}
	}
}

// horrid
void WriteDeltaEntState(EntityState* from, EntityState* to, ByteWriter& msg, uint8_t index)
{

	if (from->type == to->type && from->type == ET_FREE)
		return;

	msg.WriteByte(index);

	if (from->type != to->type) {
		msg.WriteBool(1);
		msg.WriteByte(to->type);
	}
	else
		msg.WriteBool(0);

	if (from->position != to->position) {
		msg.WriteBool(1);
		
		//short pos[3];
		//pos[0] = to->position.x * 50.f;
		//pos[1] = to->position.y * 50.f;
		//pos[2] = to->position.z * 50.f;
		msg.WriteVec3(to->position);
		//msg.WriteShort(pos[0]);
		//msg.WriteShort(pos[1]);
		//msg.WriteShort(pos[2]);
	}
	else
		msg.WriteBool(0);

	if (from->angles != to->angles) {
		msg.WriteBool(1);

		char rot[3];
		rot[0] = to->angles.x / (2 * PI) * 256;
		rot[1] = to->angles.y / (2 * PI) * 256;
		rot[2] = to->angles.z / (2 * PI) * 256;
		msg.WriteByte(rot[0]);
		msg.WriteByte(rot[1]);
		msg.WriteByte(rot[2]);
	}
	else
		msg.WriteBool(0);
	
	//if (from->model_idx != to->model_idx) {
		msg.WriteBool(1);

		msg.WriteByte(to->model_idx);
	//}
	//else
	//	msg.WriteBool(0);

	if (from->leganim != to->leganim) {
		msg.WriteBool(1);
		msg.WriteByte(to->leganim);
	}
	else
		msg.WriteBool(0);

	if (from->leganim_frame != to->leganim_frame) {
		msg.WriteBool(1);
		int quantized_frame = to->leganim_frame * 100;
		msg.WriteShort(quantized_frame);
	}
	else
		msg.WriteBool(0);

	if (from->mainanim != to->mainanim) {
		msg.WriteBool(1);
		msg.WriteByte(to->mainanim);
	}
	else
		msg.WriteBool(0);

	if (from->mainanim_frame != to->mainanim_frame) {
		msg.WriteBool(1);
		int quantized_frame = to->mainanim_frame * 100;
		msg.WriteShort(quantized_frame);
	}
	else
		msg.WriteBool(0);

	if (from->flags != to->flags) {
		msg.WriteBool(1);
		msg.WriteShort(to->flags);
	}
	else
		msg.WriteBool(0);

	msg.WriteLong(to->solid);
}

void ReadDeltaEntState(EntityState* to, ByteReader& msg)
{
	if (msg.ReadBool())
		to->type = msg.ReadByte();
	if (msg.ReadBool()) {
		short pos[3];
		//pos[0] = msg.ReadShort();
		//pos[1] = msg.ReadShort();
		//pos[2] = msg.ReadShort();
		//
		//to->position.x = pos[0] / 50.f;
		//to->position.y = pos[1] / 50.f;
		//to->position.z = pos[2] / 50.f;

		to->position = msg.ReadVec3();

	}
	if (msg.ReadBool()) {
		char rot[3];
		rot[0] = msg.ReadByte();
		rot[1] = msg.ReadByte();
		rot[2] = msg.ReadByte();
		to->angles.x = (float)rot[0] * (2 * PI) / 256.0;
		to->angles.y = (float)rot[1] * (2 * PI) / 256.0;
		to->angles.z = (float)rot[2] * (2 * PI) / 256.0;
	}

	if (msg.ReadBool())
		to->model_idx = msg.ReadByte();

	if (msg.ReadBool())
		to->leganim = msg.ReadByte();
	if (msg.ReadBool()) {
		int quantized_frame = msg.ReadShort();
		to->leganim_frame = quantized_frame / 100.0;
	}
	if (msg.ReadBool())
		to->mainanim = msg.ReadByte();
	if (msg.ReadBool()) {
		int quantized_frame = msg.ReadShort();
		to->mainanim_frame = quantized_frame / 100.0;
	}

	if (msg.ReadBool()) {
		to->flags = msg.ReadShort();
	}

	to->solid = msg.ReadLong();
}

void ReadDeltaPState(PlayerState* to, ByteReader& msg)
{
	if (msg.ReadBool()) {
		to->position = msg.ReadVec3();

	}
	if (msg.ReadBool()) {
		to->velocity = msg.ReadVec3();
	}
	if (msg.ReadBool()) {
		to->angles = msg.ReadVec3();
	}

	if (msg.ReadBool()) {
		to->state = msg.ReadShort();
	}
	// items >>>

	to->items.active_item = msg.ReadByte();
	to->items.item_bitmask = msg.ReadLong();

	for (int i = 0; i < Item_State::MAX_ITEMS; i++) {
		if (!(to->items.item_bitmask & (1 << i)))
			continue;
		to->items.item_id[i] = msg.ReadByte();
		to->items.ammo[i] = msg.ReadShort();
		to->items.clip[i] = msg.ReadShort();
	}
	to->items.timer = msg.ReadFloat();
	to->items.state = (Item_Use_State)msg.ReadByte();

	// items <<<
}

void WriteDeltaPState(PlayerState* from, PlayerState* to, ByteWriter& msg)
{
	if (from->position != to->position) {
		msg.WriteBool(1);
		msg.WriteVec3(to->position);
	}
	else
		msg.WriteBool(0);

	if (from->velocity != to->velocity) {
		msg.WriteBool(1);
		msg.WriteVec3(to->velocity);
	}
	else
		msg.WriteBool(0);

	if (from->angles != to->angles) {
		msg.WriteBool(1);
		msg.WriteVec3(to->angles);
	}
	else
		msg.WriteBool(0);

	if (from->state != to->state) {
		msg.WriteBool(1);
		msg.WriteShort(to->state);
	}
	else
		msg.WriteBool(0);

	// items

	msg.WriteByte(to->items.active_item);
	msg.WriteLong(to->items.item_bitmask);

	for (int i = 0; i < Item_State::MAX_ITEMS; i++) {
		if (!(to->items.item_bitmask & (1 << i)))
			continue;
		msg.WriteByte(to->items.item_id[i]);
		msg.WriteShort(to->items.ammo[i]);
		msg.WriteShort(to->items.clip[i]);
	}

	msg.WriteFloat(to->items.timer);
	msg.WriteByte(to->items.state);

}

static EntityState null_estate;
static PlayerState null_pstate;


void Server::WriteDeltaSnapshot(ByteWriter& msg, int deltatick, int client_idx)
{
	Frame* to = GetSnapshotFrame();
	Frame* from = nullptr;
	if (deltatick > 0)
		from = GetSnapshotFrameForTick(deltatick);
	
	if (!from)
		msg.WriteLong(-1);
	else
		msg.WriteLong(deltatick);

	for (int i = 0; i < Frame::MAX_FRAME_ENTS; i++) {
		EntityState* fromstate = (from) ? &from->states[i] : &null_estate;
		WriteDeltaEntState(fromstate, &to->states[i], msg, i);
	}
	msg.WriteByte(-1);	// sentinal

	msg.WriteLong(0xabababab);

	PlayerState* fromp = (from) ? &from->ps_states[client_idx] : &null_pstate;
	WriteDeltaPState(fromp, &to->ps_states[client_idx], msg);
}
