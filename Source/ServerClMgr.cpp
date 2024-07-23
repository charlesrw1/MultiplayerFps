#include "Server.h"
#include "GameEnginePublic.h"
#include "Framework/Bytepacker.h"

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

void RemoteClient::Disconnect(const char* debug_reason)
{
	if (state == SCS_DEAD)
		return;
	sys_print("Disconnecting client %d because %s\n", client_num, debug_reason);

	if (state == SCS_SPAWNED) {
		eng->logout_player(client_num);
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

bool RemoteClient::OnMoveCmd(ByteReader& msg)
{
	Move_Command commands[16];
	int tick = msg.ReadLong();
	int num_commands = msg.ReadByte();
	if (num_commands > 16 || num_commands <= 0) {
		Disconnect("invalid command count");
		return false;
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
		return true;

	int commands_to_run = 1 + connection.dropped;	// FIXME: have to run more than 1 command if client send rate is less than tick rate
	commands_to_run = glm::min(commands_to_run, num_commands);

	if (commands_to_run > 1) {
		printf("simming dropped %d cmds\n", commands_to_run - 1);
	}

	//Entity& e = *eng->get_ent(client_num);
	ASSERT(0);
	//// FIXME: exploit to send inputs at increased rate
	//for (int i = commands_to_run - 1; i >= 0; i--) {
	//	Move_Command c = commands[i];
	////	eng->execute_player_move(client_num, c);
	//
	//	//e.add_to_last();
	//}

	return true;
}

void RemoteClient::OnTextCommand(ByteReader& msg)
{
	uint8_t cmd_len = msg.ReadByte();
	std::string cmd;
	cmd.resize(cmd_len);
	msg.ReadBytes((uint8_t*)&cmd[0], cmd_len);

	if (msg.HasFailed())
		Disconnect("text command failed");
}

extern ConfigVar snapshot_rate;

void RemoteClient::Update()
{
	if (!IsConnected() || local_client)
		return;

	if (!IsSpawned()) {
		connection.Send(nullptr, 0);
		return;
	}

	next_snapshot_time -= eng->get_tick_interval();
	if (next_snapshot_time > 0.f)
		return;

	next_snapshot_time += (1.0 / snapshot_rate.get_float());

	uint8_t buffer[MAX_PAYLOAD_SIZE];
	ByteWriter writer(buffer, MAX_PAYLOAD_SIZE);

	writer.WriteByte(SV_TICK);
	writer.WriteLong(eng->get_game_tick());

	writer.WriteByte(SV_SNAPSHOT);
	static ConfigVar never_delta("sv.never_delta", "0",CVAR_BOOL|CVAR_DEV);

	int delta_frame = (never_delta.get_bool()) ? -1 : baseline;
	myserver->write_delta_entities_to_client(writer, delta_frame, client_num);

	//Entity& e = *eng->get_ent(client_num);
	ASSERT(0);
	//if (e.force_angles == 1) {
	//	writer.WriteByte(SV_UPDATE_VIEW);
	//	writer.WriteVec3(e.diff_angles);
	//
	//	e.force_angles = 0;
	//}


	//myserver->WriteDeltaSnapshot(writer, baseline, client_num);
	writer.EndWrite();
	connection.Send(buffer, writer.BytesWritten());
}

void RemoteClient::OnPacket(ByteReader& buf)
{
	for(;;)
	{
		buf.AlignToByteBoundary();
		if (buf.IsEof())
			return;
		if (buf.HasFailed()) {
			Disconnect("OnPacket read fail");
			return;
		}

		uint8_t command = buf.ReadByte();
		switch (command)
		{
		case CL_NOP:
			break;
		case CL_INPUT:
			if (!OnMoveCmd(buf))
				return;
			break;
		case CL_QUIT:
			Disconnect("closed connection");
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
			Disconnect("unknown message");
			return;
			break;
		}
	}
}

void new_entity_fields_test()
{
#if 0
	{
		uint8_t accept_buf[256];
		ByteWriter writer(accept_buf, 256);
		writer.WriteLong(CONNECTIONLESS_SEQUENCE);
		writer.write_string("accept");
		writer.WriteByte(10);
		writer.write_string(eng->mapname);
		writer.WriteLong(eng->tick);
		writer.WriteFloat(66.0);
		writer.EndWrite();

		int bytes_written = writer.BytesWritten();

		ByteReader reader(accept_buf, 256);
		reader.ReadLong();
		string s;
		reader.read_string(s);
		int b = reader.ReadByte();
		reader.read_string(s);
		int t = reader.ReadLong();
		float f = reader.ReadFloat();
	}

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
		read_entity(&e1r, reader, Net_Prop::ALL_PROP_MASK, false);
		reader.AlignToByteBoundary();
		Entity e2r = Entity();
		e2r.index = reader.ReadBits(10);
		read_entity(&e2r, reader, Net_Prop::ALL_PROP_MASK, false);
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

		write_delta_entity(wd, r1, r2, Net_Prop::ALL_PROP_MASK);
		wd.EndWrite();

		ByteReader rd(buffer, 1000);
		read_entity(&e1, rd, Net_Prop::ALL_PROP_MASK, true);


		if (e1.model_index != e2.model_index || e1.flags != e2.flags) {
			printf("TEST 2 FAILED\n");
		}
	}
#endif

}

void Server::make_snapshot()
{
	ASSERT(0);
#if 0
	Frame* f = GetSnapshotFrame();
	f->tick = eng->tick;

	ByteWriter writer(f->data, Frame::MAX_FRAME_SNAPSHOT_DATA);
	f->num_ents_this_frame = 0;
	for (int i = 0; i < MAX_GAME_ENTS; i++) {
		Entity& e = eng->get_ent(i);
		if (!e.active())
			continue;	// unactive ents dont go in snapshot
		if (i >= MAX_CLIENTS && (!e.model || e.flags & EF_HIDDEN))
			continue;

		// hack!
		if (e.model && e.model->animations) {
			auto& a = e.anim;
			a.m.staging_anim = a.m.anim;
			a.m.staging_frame = a.m.frame;
			a.m.staging_loop = a.m.loop;
			a.m.staging_speed = a.m.speed;
			a.legs.staging_anim = a.legs.anim;
			a.legs.staging_frame = a.legs.frame;
			a.legs.staging_loop = a.legs.loop;
			a.legs.staging_speed = a.legs.speed;
		}
		if (i < MAX_CLIENTS) {
			// for players
			ASSERT(e.inv.active_item >= 0 && e.inv.active_item < Game_Inventory::NUM_GAME_ITEMS);
			e.inv.staging_item = e.inv.active_item;
			e.inv.staging_clip = e.inv.clip[e.inv.active_item];
			e.inv.staging_ammo = e.inv.ammo[e.inv.active_item];
		}

		writer.WriteBits(i, ENTITY_BITS);
		write_full_entity(&e, writer);
		writer.AlignToByteBoundary();
		f->num_ents_this_frame++;
	}
	writer.WriteByte(0xff);	// sentinal index

	if (writer.HasFailed())
		sys_print("make_snapshot buffer failed\n");

#endif
}

#if 0
Packed_Entity::Packed_Entity(Frame* f, int offset, int length) : f(f)
{
	buf_offset = offset;
	len = length;
	ByteReader r(f->data, Frame::MAX_FRAME_SNAPSHOT_DATA);
	r.AdvanceBytes(buf_offset);
	index = r.ReadBits(ENTITY_BITS);
	if (r.HasFailed()) failed = true;
}

ByteReader Packed_Entity::get_buf()
{
	ByteReader r(f->data, Frame::MAX_FRAME_SNAPSHOT_DATA);
	r.AdvanceBytes(buf_offset);
	// read off index bits
	r.ReadBits(ENTITY_BITS);

	if (r.HasFailed()) failed = true;
	return r;
}
void Packed_Entity::increment()
{
	*this = Packed_Entity(f, buf_offset + len, len);
}

Packed_Entity Frame::begin()
{
	int packet_ent_length = get_entity_baseline()->num_bytes_including_index;

	return Packed_Entity(this, 0, packet_ent_length);
}

#endif
void Server::write_delta_entities_to_client(ByteWriter& msg, int deltatick, int client_idx)
{
	ASSERT(0);
#if 0
	Entity_Baseline* baseline = get_entity_baseline();


	Frame* to = GetSnapshotFrame();
	Frame* from = nullptr;
	if (deltatick > 0)
		from = GetSnapshotFrameForTick(deltatick);

	if (!from)
		msg.WriteLong(-1);
	else
		msg.WriteLong(deltatick);
	
	msg.WriteLong(0xabcdabcd);

	Packed_Entity p1 = to->begin();

	if (from) {
		Packed_Entity p0 = from->begin();

		while (p0.index != ENTITY_SENTINAL && p1.index != ENTITY_SENTINAL && !p0.failed && !p1.failed) {
			int prop_mask = (p1.index == client_idx) ? Net_Prop::PLAYER_PROP_MASK : Net_Prop::NON_PLAYER_PROP_MASK;
			//prop_mask = Net_Prop::ALL_PROP_MASK;
			
			if (p0.index < p1.index) {
				// old entity that is now deleted
				// TODO: write out entity so it gets deleted properly

				p0.increment();	// advance p0 to the next Packed_Entity
			}
			else if (p0.index > p1.index) {
				// new entity, delta from null
				ByteReader nullstate = baseline->get_buf();	// FIXME: bytes not multiple of 4
				ByteReader to_state = p1.get_buf();

				msg.WriteBits(p1.index, ENTITY_BITS);
				write_delta_entity(msg, nullstate, to_state, prop_mask);	// FIXME: baseline will have more fields than Packed_Entities
				msg.AlignToByteBoundary();
				
				p1.increment();
			}
			else {
				// delta entity
				ByteReader from_state = p0.get_buf();
				ByteReader to_state = p1.get_buf();

				msg.WriteBits(p1.index, ENTITY_BITS);
				write_delta_entity(msg, from_state, to_state, prop_mask);
				msg.AlignToByteBoundary();
				
				p0.increment();
				p1.increment();
			}

		}
	}

	// now: there may be more new entities, so finish the list
	while (p1.index != ENTITY_SENTINAL && !p1.failed)
	{
		int condition = (p1.index == client_idx) ? Net_Prop::PLAYER_PROP_MASK : Net_Prop::NON_PLAYER_PROP_MASK;
		//condition = Net_Prop::ALL_PROP_MASK;
		
		ByteReader nullstate = baseline->get_buf();	// FIXME: bytes not multiple of 4
		ByteReader to_state = p1.get_buf();

		msg.AlignToByteBoundary();
		msg.WriteBits(p1.index, ENTITY_BITS);
		write_delta_entity(msg, nullstate, to_state, condition);	// FIXME: baseline will have more fields than Packed_Entities

		p1.increment();
	}


	msg.AlignToByteBoundary();
	msg.WriteBits(ENTITY_SENTINAL, ENTITY_BITS);

	msg.WriteLong(0xef12ef12);

#endif
}