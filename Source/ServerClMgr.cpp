#include "Server.h"
#include "CoreTypes.h"
#define DebugOut(fmt, ...) NetDebugPrintf("server: " fmt, __VA_ARGS__)

RemoteClient::RemoteClient(Server* sv, int slot)
{
	myserver = sv;
	client_num = slot;
}

void RemoteClient::InitConnection(IPAndPort address)
{
	connection.Clear();
	state = Connected;
	connection.Init(myserver->GetSock(), address);
}

void RemoteClient::Disconnect()
{
	if (state == Dead)
		return;
	DebugOut("Disconnecting client %s\n", connection.remote_addr.ToString().c_str());

	if (state == Spawned) {
		myserver->RemoveClient(client_num);
	}

	uint8_t buffer[8];
	ByteWriter writer(buffer, 8);
	writer.WriteByte(SvMessageDisconnect);
	connection.Send(buffer, writer.BytesWritten());

	state = Dead;
	connection.Clear();
}

void RemoteClient::OnMoveCmd(ByteReader& msg)
{

	MoveCommand cmd{};
	cmd.tick = msg.ReadLong();
	cmd.forward_move = msg.ReadFloat();
	cmd.lateral_move = msg.ReadFloat();
	cmd.up_move = msg.ReadFloat();
	cmd.view_angles.x = msg.ReadFloat();
	cmd.view_angles.y = msg.ReadFloat();
	cmd.button_mask = msg.ReadLong();

	// Not ready to run the input yet
	if (!IsSpawned())
		return;

	myserver->RunMoveCmd(client_num, cmd);
}

void RemoteClient::OnTextCommand(ByteReader& msg)
{
	uint8_t cmd_len = msg.ReadByte();
	std::string cmd;
	cmd.resize(cmd_len);
	msg.ReadBytes((uint8_t*)&cmd[0], cmd_len);

	if (msg.HasFailed())
		Disconnect();
	if (cmd == "init") {
		SendInitData();
	}
	else if (cmd == "spawn") {
		if (state != RemoteClient::Spawned) {
			state = RemoteClient::Spawned;
			myserver->SpawnClientInGame(client_num);
		}
	}
}

void RemoteClient::SendInitData()
{
	if (state != Connected)
		return;

	uint8_t buffer[MAX_PAYLOAD_SIZE];
	ByteWriter msg(buffer, MAX_PAYLOAD_SIZE);

	msg.WriteByte(SvMessageInitial);
	msg.WriteByte(client_num);
	myserver->WriteServerInfo(msg);		// let server fill in data

	connection.Send(buffer, msg.BytesWritten());
}

void RemoteClient::Update()
{
	if (!IsConnected())
		return;

	if (!IsSpawned()) {
		connection.Send(nullptr, 0);
		return;
	}

	next_snapshot_time -= core.tick_interval;
	if (next_snapshot_time > 0.f)
		return;

	next_snapshot_time += (1 / 30.f);

	uint8_t buffer[MAX_PAYLOAD_SIZE];
	ByteWriter writer(buffer, MAX_PAYLOAD_SIZE);

	writer.WriteByte(SvMessageTick);
	writer.WriteLong(myserver->tick);

	writer.WriteByte(SvMessageSnapshot);
	writer.WriteLong(-1);
	myserver->WriteDeltaSnapshot(writer, -1, client_num);

	connection.Send(buffer, writer.BytesWritten());
}

void RemoteClient::OnPacket(ByteReader& buf)
{
	while (!buf.IsEof())
	{
		uint8_t command = buf.ReadByte();
		if (buf.HasFailed()) {
			DebugOut("Read fail\n");
			Disconnect();
			return;
		}
		switch (command)
		{
		case ClNop:
			break;
		case ClMessageInput:
			OnMoveCmd(buf);
			break;
		case ClMessageQuit:
			Disconnect();
			break;
		case ClMessageTick:
			break;
		case ClMessageText:
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
	uint16_t mask = 0;
	if (from->type != to->type) {
		mask |= 1;
	}
	if (from->position != to->position) {
		mask |= (1 << 1);
	}
	if (from->angles != to->angles) {
		mask |= (1 << 2);
	}
	
	if (from->leganim != to->leganim) {
		mask |= 1 << 7;
	}
	if (from->leganim_frame != to->leganim_frame) {
		mask |= 1 << 8;
	}
	if (from->ducking != to->ducking) {
		mask |= 1 << 9;
	}
	if (from->mainanim != to->mainanim) {
		mask |= 1 << 10;
	}
	if (from->mainanim_frame != to->mainanim_frame) {
		mask |= 1 << 11;
	}

	if (mask == 0)
		return;

	msg.WriteByte(index);
	msg.WriteWord(mask);

	if (mask & 1)
		msg.WriteByte(to->type);
	if (mask & (1 << 1)) {
		msg.WriteFloat(to->position.x);
		msg.WriteFloat(to->position.y);
		msg.WriteFloat(to->position.z);
	}
	if (mask & (1 << 2)) {
		char rot[3];
		rot[0] = to->angles.x / (2 * PI) * 256;
		rot[1] = to->angles.y / (2 * PI) * 256;
		rot[2] = to->angles.z / (2 * PI) * 256;
		msg.WriteByte(rot[0]);
		msg.WriteByte(rot[1]);
		msg.WriteByte(rot[2]);
	}
	if (mask & (1 << 7))
		msg.WriteWord(to->leganim);
	if (mask & (1 << 8)) {
		int quantized_frame = to->leganim_frame * 100;
		msg.WriteWord(quantized_frame);
	}
	if (mask & (1 << 9))
		msg.WriteByte(to->ducking);
	if (mask & (1 << 10))
		msg.WriteWord(to->mainanim);
	if (mask & (1 << 11)) {
		int quantized_frame = to->mainanim_frame * 100;
		msg.WriteWord(quantized_frame);
	}
	
}

void ReadDeltaEntState(EntityState* to, ByteReader& msg)
{
	uint16_t mask = msg.ReadWord();

	static uint16_t last_mask = 0;
	if (last_mask != mask && to->type == Ent_Player) {
		printf("mask: %d\n", mask);
		last_mask = mask;
	}	
	if (mask & 1)
		to->type = msg.ReadByte();
	if (mask & (1 << 1)) {
		to->position.x = msg.ReadFloat();
		to->position.y = msg.ReadFloat();
		to->position.z = msg.ReadFloat();
	}
	if (mask & (1 << 2)) {
		char rot[3];
		rot[0] = msg.ReadByte();
		rot[1] = msg.ReadByte();
		rot[2] = msg.ReadByte();
		to->angles.x = (float)rot[0] * (2 * PI) / 256.0;
		to->angles.y = (float)rot[1] * (2 * PI) / 256.0;
		to->angles.z = (float)rot[2] * (2 * PI) / 256.0;
	}
	if (mask & (1 << 7))
		to->leganim = msg.ReadWord();
	if (mask & (1 << 8)) {
		int quantized_frame = msg.ReadWord();
		to->leganim_frame = quantized_frame / 100.0;
	}
	if (mask & (1 << 9))
		to->ducking = msg.ReadByte();
	if (mask & (1 << 10))
		to->mainanim = msg.ReadWord();
	if (mask & (1 << 11)) {
		int quantized_frame = msg.ReadWord();
		to->mainanim_frame = quantized_frame / 100.0;
	}
}

void ReadDeltaPState(PlayerState* to, ByteReader& msg)
{
	uint8_t mask = msg.ReadByte();
	if (mask & 1) {
		to->position.x = msg.ReadFloat();
		to->position.y = msg.ReadFloat();
		to->position.z= msg.ReadFloat();

	}
	if (mask & (1 << 1)) {
		to->angles.x = msg.ReadFloat();
		to->angles.y = msg.ReadFloat();
		to->angles.z = msg.ReadFloat();
	}
	if (mask & (1 << 2)) {
		to->ducking = msg.ReadByte();
	}
	if (mask & (1 << 3)) {
		to->on_ground = msg.ReadByte();
	}
	if (mask & (1 << 4)) {
		to->velocity.x = msg.ReadFloat();
		to->velocity.y = msg.ReadFloat();
		to->velocity.z = msg.ReadFloat();
	}
	to->alive = msg.ReadByte();
	to->in_jump = msg.ReadByte();
}

void WriteDeltaPState(PlayerState* from, PlayerState* to, ByteWriter& msg)
{
	uint8_t mask = 0;
	if (from->position != to->position) {
		mask |= 1;
	}
	if (from->angles != to->angles) {
		mask |= (1 << 1);
	}
	if (from->ducking != to->ducking) {
		mask |= (1 << 2);
	}
	if (from->on_ground != to->on_ground) {
		mask |= (1 << 3);
	}
	if (from->velocity != to->velocity) {
		mask |= (1 << 4);
	}

	msg.WriteByte(mask);
	if (mask & 1) {
		msg.WriteFloat(to->position.x);
		msg.WriteFloat(to->position.y);
		msg.WriteFloat(to->position.z);
	}
	if (mask & (1 << 1)) {
		msg.WriteFloat(to->angles.x);
		msg.WriteFloat(to->angles.y);
		msg.WriteFloat(to->angles.z);
	}
	if (mask & (1 << 2)) {
		msg.WriteByte(to->ducking);
	}
	if (mask & (1 << 3)) {
		msg.WriteByte(to->on_ground);
	}
	if (mask & (1 << 4)) {
		msg.WriteFloat(to->velocity.x);
		msg.WriteFloat(to->velocity.y);
		msg.WriteFloat(to->velocity.z);
	}
	msg.WriteByte(to->alive);
	msg.WriteByte(to->in_jump);
}

void Server::WriteDeltaSnapshot(ByteWriter& msg, int deltatick, int client_idx)
{
	Frame* to = GetLastSnapshotFrame();
	Frame* from = &nullframe;

	for (int i = 0; i < 24; i++) {
		WriteDeltaEntState(&from->states[i], &to->states[i], msg, i);
	}
	msg.WriteByte(-1);	// sentinal

	msg.WriteLong(0xabababab);

	WriteDeltaPState(&from->ps_states[client_idx], &to->ps_states[client_idx], msg);
}
