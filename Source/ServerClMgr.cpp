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
	myserver->write_delta_entities_to_client(writer, baseline, client_num);
	//myserver->WriteDeltaSnapshot(writer, baseline, client_num);
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

void new_entity_fields_test()
{
	// test 1: 
	{
		Entity e1 = Entity();
		e1.model_index = 10;
		e1.rotation.x = PI;
		e1.flags = EF_DEAD;
		Entity e2 = Entity();
		e2.flags = EF_FORCED_ANIMATION;
		e2.position.x = 10.f;
		e2.model_index = 9;

		Frame* f = new Frame;
		ByteWriter writer(f->data, Frame::MAX_FRAME_SNAPSHOT_DATA);
		writer.WriteBits(0, 10);
		write_full_entity(&e1, writer);
		writer.AlignToByteBoundary();
		writer.WriteBits(1, 10);
		write_full_entity(&e2, writer);
		writer.EndWrite();

		ByteReader reader(f->data, Frame::MAX_FRAME_SNAPSHOT_DATA);
		reader.ReadBits(10);
		Entity e1r = Entity();
		read_entity(&e1r, reader, Net_Prop::ALL, false);
		reader.AlignToByteBoundary();
		Entity e2r = Entity();
		e2r.index = reader.ReadBits(10);
		read_entity(&e2r, reader, Net_Prop::ALL, false);
		reader.AlignToByteBoundary();


		if (e2r.index != 1 || e1r.model_index != e1.model_index || e1r.flags != e1.flags
			|| abs(e1r.rotation.x - e1.rotation.x) > 0.01 || e2r.flags != e2.flags || e2r.model_index != e2.model_index) {
			printf("TEST 1 FAILED\n");
		}
	}
	// test 2: delta encoding
	{
		Entity e1 = Entity();
		e1.model_index = 5;
		e1.flags = EF_DEAD;

		Entity e2 = Entity();
		e2.model_index = 9;
		e2.flags = EF_HIDDEN;


		Frame* f = new Frame;
		ByteWriter writer(f->data, Frame::MAX_FRAME_SNAPSHOT_DATA);
		write_full_entity(&e1, writer);
		writer.EndWrite();

		Frame* f2 = new Frame;
		ByteWriter w2(f2->data, Frame::MAX_FRAME_SNAPSHOT_DATA);
		write_full_entity(&e2, w2);
		w2.EndWrite();

		uint8_t* buffer = new uint8_t[1000];
		ByteWriter wd(buffer, 1000);
		ByteReader r1(f->data, 1000);
		ByteReader r2(f2->data, 1000);

		write_delta_entity(wd, r1, r2, Net_Prop::ALL);
		wd.EndWrite();

		ByteReader rd(buffer, 1000);
		read_entity(&e1, rd, Net_Prop::ALL, true);


		if (e1.model_index != e2.model_index || e1.flags != e2.flags) {
			printf("TEST 2 FAILED\n");
		}
	}

}

void Server::make_snapshot()
{
	Frame* f = GetSnapshotFrame();
	f->tick = engine.tick;

	ByteWriter writer(f->data, Frame::MAX_FRAME_SNAPSHOT_DATA);
	f->num_ents_this_frame = 0;
	for (int i = 0; i < MAX_GAME_ENTS; i++) {
		Entity& e = engine.ents[i];
		if (!e.active())
			continue;	// unactive ents dont go in snapshot

		// TODO: hidden ents dont go in snapshot either

		writer.WriteByte(i);
		write_full_entity(&e, writer);
		writer.AlignToByteBoundary();
		f->num_ents_this_frame++;
	}
	writer.WriteByte(0xff);	// sentinal index

	if (writer.HasFailed())
		printf("make_snapshot buffer failed\n");
}
Packed_Entity::Packed_Entity(Frame* f, int offset) : f(f)
{
	buf_offset = offset;
	ByteReader r(f->data, Frame::MAX_FRAME_SNAPSHOT_DATA);
	r.AdvanceBytes(buf_offset);
	index = r.ReadByte();
	if (index != 0xff) {
		len = r.ReadBits(10);
	}
}


ByteReader Packed_Entity::operator*()
{
	ByteReader r(f->data, Frame::MAX_FRAME_SNAPSHOT_DATA);
	r.AdvanceBytes(buf_offset);
}
Packed_Entity& Packed_Entity::operator++()
{
	*this = Packed_Entity(f, buf_offset + len);
	return *this;
}

Packed_Entity Frame::begin()
{
	return Packed_Entity(this, 0);
}
Packed_Entity Frame::end()
{
	Packed_Entity pe(this, 0);
	pe.index = 0xff;	// FIXME: ent index
	return pe;
}

void Server::write_delta_entities_to_client(ByteWriter& msg, int deltatick, int client_idx)
{
	Entity_Baseline* baseline = get_entity_baseline();


	Frame* to = GetSnapshotFrame();
	Frame* from = nullptr;
	if (deltatick > 0)
		from = GetSnapshotFrameForTick(deltatick);

	if (!from)
		msg.WriteLong(-1);
	else
		msg.WriteLong(deltatick);
		
	ByteReader s1(to->data, Frame::MAX_FRAME_SNAPSHOT_DATA);
	int index1 = s1.ReadByte();
	
	if (from) {
		ByteReader s0(from->data, Frame::MAX_FRAME_SNAPSHOT_DATA);
		int index0 = s0.ReadByte();
		while (index0 != 0xff && index1 != 0xff && !s1.HasFailed() && !s0.HasFailed()) {
			int condition = (index1 == client_idx) ? Net_Prop::ONLY_PLAYER : Net_Prop::NOT_PLAYER;
			if (index0 < index1) {
				// entities that are deleted now, skip them
				s0.AdvanceBytes(null_entity_state_bytes);
				index0 = s0.ReadByte();
			}
			else if (index0 > index1) {
				// new entity, delta from null
				ByteReader nullstate(null_entity_state, null_entity_state_bytes);
				msg.WriteByte(index1);
				write_delta_entity(msg, nullstate, s1, condition);
				msg.AlignToByteBoundary();
				index1 = s1.ReadByte();
			}
			else {
				// delta entity
				msg.WriteByte(index1);
				write_delta_entity(msg, s0, s1, condition);
				msg.AlignToByteBoundary();
				index0 = s0.ReadByte();
				index1 = s1.ReadByte();
			}

		}
	}
	msg.WriteLong(0xabababab);
	// now: there may be more new entities, so finish the list
	while (index1 != 0xff && !s1.HasFailed())
	{
		int condition = (index1 == client_idx) ? Net_Prop::ONLY_PLAYER : Net_Prop::NOT_PLAYER;
		ByteReader nullstate(null_entity_state, null_entity_state_bytes);
		msg.WriteByte(index1);
		write_delta_entity(msg, nullstate, s1, condition);
		msg.AlignToByteBoundary();
		index1 = s1.ReadByte();
	}
}