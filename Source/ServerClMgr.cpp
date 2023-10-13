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

	cmd.forward_move = MoveCommand::unquantize(msg.ReadByte());
	cmd.lateral_move = MoveCommand::unquantize(msg.ReadByte());
	cmd.up_move = MoveCommand::unquantize(msg.ReadByte());

	cmd.view_angles.x = msg.ReadFloat();
	cmd.view_angles.y = msg.ReadFloat();
	cmd.button_mask = msg.ReadLong();

	// Not ready to run the input yet
	if (!IsSpawned())
		return;

	// FIXME vulnerable to 
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

	next_snapshot_time += (1.0 / *myserver->cfg_snapshot_rate);

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

	if (from->type == to->type && from->type == Ent_Free)
		return;

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
	
	if (from->model_idx != to->model_idx)
		mask |= (1 << 6);

	if (from->leganim != to->leganim) {
		mask |= 1 << 7;
	}
	if (from->leganim_frame != to->leganim_frame) {
		mask |= 1 << 8;
	}

	if (to->ducking)
		mask |= 1 << 9;

	if (from->mainanim != to->mainanim) {
		mask |= 1 << 10;
	}
	if (from->mainanim_frame != to->mainanim_frame) {
		mask |= 1 << 11;
	}

	if (mask == 0)
		return;

	msg.WriteByte(index);
	msg.WriteShort(mask);

	if (mask & 1)
		msg.WriteByte(to->type);
	if (mask & (1 << 1)) {
		short pos[3];
		pos[0] = to->position.x * 20.f;
		pos[1] = to->position.y * 20.f;
		pos[2] = to->position.z * 20.f;

		msg.WriteShort(pos[0]);
		msg.WriteShort(pos[1]);
		msg.WriteShort(pos[2]);

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
	if (mask & (1 << 6))
		msg.WriteByte(to->model_idx);

	if (mask & (1 << 7))
		msg.WriteByte(to->leganim);
	if (mask & (1 << 8)) {
		int quantized_frame = to->leganim_frame * 100;
		msg.WriteShort(quantized_frame);
	}
	if (mask & (1 << 10))
		msg.WriteByte(to->mainanim);
	if (mask & (1 << 11)) {
		int quantized_frame = to->mainanim_frame * 100;
		msg.WriteShort(quantized_frame);
	}
	
}

void ReadDeltaEntState(EntityState* to, ByteReader& msg)
{
	uint16_t mask = msg.ReadShort();

	to->ducking = (mask & (1 << 9));

	if (mask & 1)
		to->type = msg.ReadByte();
	if (mask & (1 << 1)) {
		short pos[3];
		pos[0] = msg.ReadShort();
		pos[1] = msg.ReadShort();
		pos[2] = msg.ReadShort();

		to->position.x = pos[0] / 20.f;
		to->position.y = pos[1] / 20.f;
		to->position.z = pos[2] / 20.f;

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

	if (mask & (1 << 6))
		to->model_idx = msg.ReadByte();

	if (mask & (1 << 7))
		to->leganim = msg.ReadByte();
	if (mask & (1 << 8)) {
		int quantized_frame = msg.ReadShort();
		to->leganim_frame = quantized_frame / 100.0;
	}
	if (mask & (1 << 10))
		to->mainanim = msg.ReadByte();
	if (mask & (1 << 11)) {
		int quantized_frame = msg.ReadShort();
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
		to->velocity.x = msg.ReadFloat();
		to->velocity.y = msg.ReadFloat();
		to->velocity.z = msg.ReadFloat();
	}


	// items >>>

	to->items.active_item = msg.ReadByte();
	to->items.item_bitmask = msg.ReadLong();

	for (int i = 0; i < ItemState::MAX_ITEMS; i++) {
		if (!(to->items.item_bitmask & (1 << i)))
			continue;
		to->items.item_id[i] = msg.ReadByte();
		to->items.ammo[i] = msg.ReadShort();
		to->items.clip[i] = msg.ReadShort();
	}
	to->items.gun_timer = msg.ReadFloat();
	to->items.state = (ItemUseState)msg.ReadByte();

	// items <<<

	to->ducking = mask & (1 << 3);
	to->on_ground = mask & (1 << 4);
	to->alive = mask & (1 << 5);
	to->in_jump = mask & (1 << 6);
}

void WriteDeltaPState(PlayerState* from, PlayerState* to, ByteWriter& msg)
{
	uint8_t mask_and_flags = 0;
	if (from->position != to->position) {
		mask_and_flags |= 1;
	}
	if (from->angles != to->angles) {
		mask_and_flags |= (1 << 1);
	}
	if (from->velocity != to->velocity) {
		mask_and_flags |= (1 << 2);
	}

	if (to->ducking)
		mask_and_flags |= (1 << 3);
	if (to->on_ground)
		mask_and_flags |= (1 << 4);
	if (to->alive)
		mask_and_flags |= (1 << 5);
	if (to->in_jump)
		mask_and_flags |= (1 << 6);



	msg.WriteByte(mask_and_flags);
	if (mask_and_flags & 1) {
		msg.WriteFloat(to->position.x);
		msg.WriteFloat(to->position.y);
		msg.WriteFloat(to->position.z);
	}
	if (mask_and_flags & (1 << 1)) {
		msg.WriteFloat(to->angles.x);
		msg.WriteFloat(to->angles.y);
		msg.WriteFloat(to->angles.z);
	}
	if (mask_and_flags & (1 << 2)) {
		msg.WriteFloat(to->velocity.x);
		msg.WriteFloat(to->velocity.y);
		msg.WriteFloat(to->velocity.z);
	}

	// items

	msg.WriteByte(to->items.active_item);
	msg.WriteLong(to->items.item_bitmask);

	for (int i = 0; i < ItemState::MAX_ITEMS; i++) {
		if (!(to->items.item_bitmask & (1 << i)))
			continue;
		msg.WriteByte(to->items.item_id[i]);
		msg.WriteShort(to->items.ammo[i]);
		msg.WriteShort(to->items.clip[i]);
	}

	msg.WriteFloat(to->items.gun_timer);
	msg.WriteByte(to->items.state);

}

void Server::WriteDeltaSnapshot(ByteWriter& msg, int deltatick, int client_idx)
{
	Frame* to = GetLastSnapshotFrame();
	Frame* from = &nullframe;

	for (int i = 0; i < Frame::MAX_FRAME_ENTS; i++) {
		WriteDeltaEntState(&from->states[i], &to->states[i], msg, i);
	}
	msg.WriteByte(-1);	// sentinal

	msg.WriteLong(0xabababab);

	WriteDeltaPState(&from->ps_states[client_idx], &to->ps_states[client_idx], msg);
}
