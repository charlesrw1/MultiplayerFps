#include "Client.h"
#include "Game_Engine.h"

//#define DebugOut(fmt, ...)


void Client::ReadPackets()
{
	// check cfg var updates
	sock.loss = fake_loss->integer;
	sock.lag = fake_lag->integer;
	sock.jitter = 0;
	sock.enabled = !(sock.loss == 0 && sock.lag == 0 && sock.jitter == 0);

	uint8_t inbuffer[MAX_PAYLOAD_SIZE + PACKET_HEADER_SIZE];
	size_t recv_len = 0;
	IPAndPort from;

	while (sock.Receive(inbuffer, sizeof(inbuffer), recv_len, from))
	{
		if (recv_len < PACKET_HEADER_SIZE)
			continue;

		// connectionless packet
		if (*(uint32_t*)inbuffer == CONNECTIONLESS_SEQUENCE) {
			ByteReader buf(inbuffer, recv_len, sizeof(inbuffer));
			buf.ReadLong();	// skip header
			
			string msg;
			buf.read_string(msg);

			if (msg == "accept" && state == CS_TRYINGCONNECT) {
				int client_num_ = buf.ReadByte();
				string mapname;
				buf.read_string(mapname);
				int tick = buf.ReadLong();
				float tickrate = buf.ReadFloat();


				sys_print("Accepted into server. Player#: %d. Map: %s. Tickrate %f.\n", client_num, mapname.c_str(), tickrate);

				// We now have a valid connection with the server
				server.Init(&sock, server.remote_addr);
				state = CS_CONNECTED;
				if (engine.start_map(mapname, true)) {
					engine.set_tick_rate(tickrate);
					engine.tick = tick;
					client_num = client_num_;
				}
				else {
					Disconnect("couldn't start map");
				}
			}
			else if (msg == "reject" && state == CS_TRYINGCONNECT) {
				Disconnect("rejected by server");
			}
			else {
				sys_print("Unknown connectionless packet type\n");
			}

			continue;
		}

		if (state >= CS_CONNECTED && from == server.remote_addr) {
			int header_len = server.NewPacket(inbuffer, recv_len);
			if (header_len != -1) {
				ByteReader reader(inbuffer, recv_len, sizeof(inbuffer));
				reader.AdvanceBytes(header_len);
				HandleServerPacket(reader);
			}
		}
	}

	if (state >= CS_CONNECTED) {
		if (GetTime() - server.last_recieved > cl_time_out->real) {
			Disconnect("server timed out");
		}
	}
}

void Client::HandleServerPacket(ByteReader& buf)
{
	for (;;)
	{
		buf.AlignToByteBoundary();
		if (buf.IsEof())
			return;
		if (buf.HasFailed()) {
			Disconnect("HandleServerPacket read fail");
			return;
		}
		
		uint8_t command = buf.ReadByte();
		switch (command)
		{
		case SV_NOP:
			break;
		case SV_INITIAL:
			ASSERT(0);
			//OnServerInit(buf);
			break;
		case SV_SNAPSHOT: {
			if (state == CS_CONNECTED) {
				state = CS_SPAWNED;

				// start game state after recieving first snapshot
				engine.set_state(ENGINE_GAME);
			}
			bool ignore_packet = OnEntSnapshot(buf);
			if (ignore_packet)
				return;
		}break;
		case SV_DISCONNECT:
			Disconnect("server closed connection");
			break;
		case SV_TEXT:
			break;
		case SV_TICK:
		{
			int server_tick = buf.ReadLong();	// this is the server's current tick
			float server_time = server_tick * engine.tick_interval;
			time_delta = engine.time - server_time;

			// hard reset the time values
			if (abs(time_delta) > time_reset_threshold->real) {
				sys_print("reset time %f\n", time_delta);
				engine.tick = server_tick;
				engine.time = server_time;
				time_delta = 0.0;
			}
			last_recieved_server_tick = server_tick;
		}break;
		default:
			Disconnect("of recieving an unknown message");
			return;
			break;
		}
	}
}

void write_delta_move_command(ByteWriter& msg, Move_Command from, Move_Command to)
{
	if (to.lateral_move != from.lateral_move || to.forward_move != from.forward_move || to.up_move != from.up_move) {
		msg.WriteBool(1);
		msg.WriteByte(Move_Command::quantize(to.forward_move));
		msg.WriteByte(Move_Command::quantize(to.lateral_move));
		msg.WriteByte(Move_Command::quantize(to.up_move));
	}
	else
		msg.WriteBool(0);

	if (to.view_angles.x != from.view_angles.x || to.view_angles.y != from.view_angles.y) {
		msg.WriteBool(1);
		msg.WriteFloat(to.view_angles.x);
		msg.WriteFloat(to.view_angles.y);
	}
	else
		msg.WriteBool(0);

	if (to.button_mask != from.button_mask) {
		msg.WriteBool(1);
		msg.WriteLong(to.button_mask);
	}
	else
		msg.WriteBool(0);
}

void read_delta_move_command(ByteReader& msg, Move_Command& to)
{
	if (msg.ReadBool()) {
		to.forward_move  = Move_Command::unquantize(msg.ReadByte());
		to.lateral_move = Move_Command::unquantize(msg.ReadByte());
		to.up_move = Move_Command::unquantize(msg.ReadByte());
	}
	if (msg.ReadBool()) {
		to.view_angles.x = msg.ReadFloat();
		to.view_angles.y = msg.ReadFloat();
	}
	if (msg.ReadBool()) {
		to.button_mask = msg.ReadLong();
	}
}


void Client::SendMovesAndMessages()
{
	if (state < CS_CONNECTED)
		return;

	// Send move
	Move_Command lastmove = get_command(server.out_sequence);

	uint8_t buffer[512];
	ByteWriter writer(buffer, 512);
	writer.WriteByte(CL_INPUT);
	// input format
	// tick of command
	// num commands total
	writer.WriteLong(engine.tick);


	int total_commands =  glm::min(server.out_sequence + 1, 8);
	writer.WriteByte(total_commands);

	Move_Command last = Move_Command();
	for (int i = 0; i < total_commands; i++) {
		write_delta_move_command(writer, last, get_command(server.out_sequence - i));	// FIXME negative index edge case!!
		last = get_command(server.out_sequence - i);
	}

	//writer.WriteByte(Move_Command::quantize(lastmove.forward_move));
	//writer.WriteByte(Move_Command::quantize(lastmove.lateral_move));
	//writer.WriteByte(Move_Command::quantize(lastmove.up_move));
	//
	//writer.WriteFloat(lastmove.view_angles.x);
	//writer.WriteFloat(lastmove.view_angles.y);
	//writer.WriteLong(lastmove.button_mask);

	writer.AlignToByteBoundary();

	writer.WriteByte(CL_SET_BASELINE);
	if (force_full_update)
		writer.WriteLong(-1);
	else
		writer.WriteLong(last_recieved_server_tick);

	writer.EndWrite();
	server.Send(buffer, writer.BytesWritten());
}

static Entity null_ent;

bool Client::OnEntSnapshot(ByteReader& msg)
{
	int delta_tick = msg.ReadLong();

	if (delta_tick == -1) {
		printf("client: recieved full update\n");
		if (force_full_update)
			force_full_update = false;
	}

	Frame* from = nullptr;

	if (delta_tick != -1) {
		from = FindSnapshotForTick(delta_tick);
		if (!from) {
			sys_print("Delta snapshot not found, requested: %d, current: %d", delta_tick, engine.tick);
			ForceFullUpdate();
			// discard packet
			return true;
		}
	}

	Entity_Baseline* baseline = get_entity_baseline();
	
	int sent1 = msg.ReadLong();
	if (sent1 != 0xabcdabcd) {
		sys_print("wrong sentinal\n");
		return true;
	}

	msg.AlignToByteBoundary();
	int to_index = msg.ReadBits(ENTITY_BITS);

	if (from) {
		Packed_Entity from_ent = from->begin();

		while (from_ent.index != ENTITY_SENTINAL && to_index != ENTITY_SENTINAL
			&& !msg.HasFailed() && !from_ent.failed) {
			
			Entity* ent = (to_index == ENTITY_SENTINAL) ? nullptr : &engine.ents[to_index];
			int prop_mask = (to_index == client_num) ? Net_Prop::PLAYER_PROP_MASK : Net_Prop::NON_PLAYER_PROP_MASK;

			if (from_ent.index < to_index) {
				// old entity, now gone
				Entity* oldent = &engine.ents[from_ent.index];	// FIXME: have to do more stuff like reset interp
				oldent->set_inactive();
				from_ent.increment();	// advance from_state to the next Packed_Entity
			}
			else if (from_ent.index > to_index) {
				// new entity:
				// set state to the baseline/null
				ByteReader base = baseline->get_buf();
				read_entity(ent, base, prop_mask, false);
				// now add in the delta entries
				read_entity(ent, msg, prop_mask, true);

				msg.AlignToByteBoundary();
				to_index = msg.ReadBits(ENTITY_BITS);
			}
			else {
				// delta entity
				ByteReader from_state = from_ent.get_buf();

				if (dont_replicate_player->integer) {
					read_entity(&null_ent, msg, prop_mask, true);	// FIXME: this is just to advance msg
				}
				else {
					read_entity(ent, from_state, prop_mask, false);	// reset
					read_entity(ent, msg, prop_mask, true);			// update delta
				}

				msg.AlignToByteBoundary();
				to_index = msg.ReadBits(ENTITY_BITS);
				from_ent.increment();
			}

		}

		//  these are *old* entities in the baseline that didnt find a partner in 
		// the delta packet :(, remove them
		while (from_ent.index != ENTITY_SENTINAL && !from_ent.failed)
		{
			Entity* ent =  &engine.ents[from_ent.index];
			ent->set_inactive();
			from_ent.increment();
		}
	}

	// now: there may be more *new* entities, so finish the list
	while (to_index != ENTITY_SENTINAL && !msg.HasFailed())
	{

		Entity* ent = (to_index == ENTITY_SENTINAL) ? nullptr : &engine.ents[to_index];
		int prop_mask = (to_index == client_num) ? Net_Prop::PLAYER_PROP_MASK : Net_Prop::NON_PLAYER_PROP_MASK;

		// reset to baseline:
		ByteReader base = baseline->get_buf();
		read_entity(ent, base, prop_mask, false);
		
		// delta props:
		read_entity(ent, msg, prop_mask, true);

		msg.AlignToByteBoundary();
		to_index = msg.ReadBits(ENTITY_BITS);
	}

	int sent2 = msg.ReadLong();
	if (sent2 != 0xef12ef12) {
		sys_print("wrong sentinal\n");
		return true;
	}

	// process updates from snapshot
	Frame* to = GetCurrentSnapshot();
	to->tick = last_recieved_server_tick;
	to->player_offset = -1;
	to->num_ents_this_frame = 0;
	read_snapshot(to);

	return false;
}