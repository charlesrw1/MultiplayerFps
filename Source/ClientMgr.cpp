#include "Client.h"
#include "Game_Engine.h"

#include "Framework/Bytepacker.h"
//#define DebugOut(fmt, ...)


void Client::ReadPackets()
{
	// check cfg var updates
	sock.loss = fake_loss.integer();
	sock.lag = fake_lag.integer();
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
				//if (eng->start_map(mapname, true)) {
				//	eng->set_tick_rate(tickrate);
				//	eng->tick = tick;
				//	client_num = client_num_;
				//}
				//else {
				//	disconnect_to_menu("couldn't start map");
				//}
			}
			else if (msg == "reject" && state == CS_TRYINGCONNECT) {
				disconnect_to_menu("rejected by server");
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
		if (GetTime() - server.last_recieved > time_out.real()) {
			disconnect_to_menu("server timed out");
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
			disconnect_to_menu("HandleServerPacket read fail");
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
				ASSERT(0);
				//eng->client_enter_into_game();
			}
			bool ignore_packet = OnEntSnapshot(buf);
			if (ignore_packet)
				return;
		}break;
		case SV_DISCONNECT:
			disconnect_to_menu("server closed connection");
			break;
		case SV_UPDATE_VIEW:
			ASSERT(0);
		//	glm::vec3 new_angles = buf.ReadVec3();
			//eng->local.view_angles = new_angles;
			break;
		case SV_TEXT:
			break;
		case SV_TICK:
		{
			int server_tick = buf.ReadLong();	// this is the server's current tick
			float server_time = server_tick * eng->tick_interval;
			time_delta = eng->time - server_time;

			// hard reset the time values
			if (abs(time_delta) > time_reset_threshold.real()) {
				sys_print("reset time %f\n", time_delta);
				eng->tick = server_tick;
				eng->time = server_time;
				time_delta = 0.0;
			}
			last_recieved_server_tick = server_tick;
		}break;
		default:
			disconnect_to_menu("of recieving an unknown message");
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
	writer.WriteLong(eng->tick);


	int total_commands =  glm::min(server.out_sequence + 1, 8);
	writer.WriteByte(total_commands);

	Move_Command last = Move_Command();
	for (int i = 0; i < total_commands; i++) {
		write_delta_move_command(writer, last, get_command(server.out_sequence - i));	// FIXME negative index edge case!!
		last = get_command(server.out_sequence - i);
	}

	writer.AlignToByteBoundary();

	writer.WriteByte(CL_SET_BASELINE);
	if (force_full_update)
		writer.WriteLong(-1);
	else
		writer.WriteLong(last_recieved_server_tick);

	writer.EndWrite();
	server.Send(buffer, writer.BytesWritten());
}


void Client::read_entity_from_snapshot(Entity* ent, int index, ByteReader& msg, bool is_delta, ByteReader* from_ent, int tick)
{

	ASSERT(0);
#if 0
	ent->index = index;

	Entity_Baseline* baseline = get_entity_baseline();
	bool is_local_player = client_num == index;
	int prop_mask = (is_local_player) ? Net_Prop::PLAYER_PROP_MASK : Net_Prop::NON_PLAYER_PROP_MASK;
	
	if (is_delta) {
		read_entity(ent, *from_ent, prop_mask, false);	// reset
		read_entity(ent, msg, prop_mask, true);			// update delta
	}
	else {
		ByteReader base = baseline->get_buf();
		read_entity(ent, base, prop_mask, false); // reset to baseline
		read_entity(ent, msg, prop_mask, true); // now add in the delta entries
	}

	Model* next_model = nullptr;
	if (ent->model_index != -1)
		next_model = eng->media.get_game_model_from_index(ent->model_index);
	if (next_model != ent->model) {
		ent->model = next_model;
		if (ent->model && ent->model->bones.size() > 0)
			ent->anim.set_model(ent->model);
	}

	// if requesting forced animation on local player, then goto it
	if (is_local_player && ent->flags & EF_FORCED_ANIMATION) {
		// transition animations
		ent->anim.set_anim_from_index(ent->anim.m, ent->anim.m.staging_anim, false);
		ent->anim.set_anim_from_index(ent->anim.legs, ent->anim.legs.staging_anim, false);
		ent->anim.m.loop = ent->anim.m.staging_loop;
		ent->anim.m.speed = ent->anim.m.staging_speed;
		ent->anim.legs.loop = ent->anim.legs.staging_loop;
		ent->anim.legs.speed = ent->anim.legs.staging_speed;
	}

	// items, more shitty hacks
	if (is_local_player) {
		ASSERT(ent->inv.staging_item >= 0 && ent->inv.staging_item < Game_Inventory::NUM_GAME_ITEMS);
		auto& inv = ent->inv;
		bool all_inv_fields_equal = inv.staging_item == inv.active_item
			&& inv.ammo[inv.staging_item] == inv.staging_ammo && inv.clip[inv.staging_item] == inv.staging_clip;

		if (all_inv_fields_equal)
			ent->inv.tick_for_staging = -1;
		else if (!all_inv_fields_equal && ent->inv.tick_for_staging == -1)
			ent->inv.tick_for_staging = eng->tick;
	}

	if (!is_local_player) {
		Entity_Interp& interp = interpolation_data[index];
		if (ent->flags & EF_TELEPORTED)
			interp.clear();

		auto& l1 = ent->anim.m;
		auto& l2 = ent->anim.legs;

		auto functor = [](Entity* ent, Animator_Layer& l) {
			if (l.staging_anim == l.anim && l.speed > 0 && !l.loop && l.staging_frame < l.frame - 0.05) {
				ent->anim.set_anim_from_index(l, l.anim, true);	// restart the anim
			}
			else if (l.staging_anim != l.anim) {
				ent->anim.set_anim_from_index(l, l.staging_anim, false);
			}
			l.loop = l.staging_loop;
			l.speed = l.staging_speed;
		};
		functor(ent, l1);
		functor(ent, l2);

		Interp_Entry& entry = interp.hist[interp.hist_index];
		entry.tick = tick;
		entry.position = ent->position;
		entry.angles = ent->rotation;

		interp.increment_index();
	}
#endif
}

bool Client::OnEntSnapshot(ByteReader& msg)
{
	ASSERT(0);
#if 0
	int delta_tick = msg.ReadLong();

	if (delta_tick == -1) {
		printf("client: recieved full update\n");
		if (force_full_update)
			force_full_update = false;
	}


	Frame2* from = nullptr;

	if (delta_tick != -1) {
		from = frame_storage.get_frame_for_tick(delta_tick);
		if (!from) {
			sys_print("Delta snapshot not found, requested: %d, current: %d", delta_tick, eng->tick);
			ForceFullUpdate();
			// discard packet
			return true;
		}
	}
	
	int sent1 = msg.ReadLong();
	if (sent1 != 0xabcdabcd) {
		sys_print("wrong sentinal\n");
		return true;
	}

	msg.AlignToByteBoundary();

	entityhandle to_index = msg.ReadBits(ENTITY_BITS);

	if (from) {
		int count_in_from = from->count;
		int index = 0;


		while (index < count_in_from && to_index != ENTITY_SENTINAL
			&& !msg.HasFailed()) {
			
			Obj_Net_State& from_ent = frame_storage.get_state(*from, index);

			Entity* ent = (to_index == ENTITY_SENTINAL) ? nullptr : &eng->get_ent(to_index);
			int prop_mask = (to_index == client_num) ? Net_Prop::PLAYER_PROP_MASK : Net_Prop::NON_PLAYER_PROP_MASK;


			if (from_ent.handle < to_index) {
				// old entity, now gone
				Entity* oldent = &eng->get_ent(from_ent.handle);	// FIXME: have to do more stuff like reset interp
				oldent->set_inactive();
				from_ent.increment();	// advance from_state to the next Packed_Entity
			}
			else if (from_ent.index > to_index) {
				// new entity:
				read_entity_from_snapshot(ent, to_index, msg, false, nullptr, last_recieved_server_tick);

				msg.AlignToByteBoundary();
				to_index = msg.ReadBits(ENTITY_BITS);
			}
			else {
				// delta entity
				ByteReader from_state = from_ent.get_buf();
				read_entity_from_snapshot(ent, to_index, msg, true, &from_state, last_recieved_server_tick);

				msg.AlignToByteBoundary();
				to_index = msg.ReadBits(ENTITY_BITS);
				from_ent.increment();
			}

		}

		//  these are *old* entities in the baseline that didnt find a partner in 
		// the delta packet :(, remove them
		while (from_ent.index != ENTITY_SENTINAL && !from_ent.failed)
		{
			Entity* ent =  &eng->ents[from_ent.index];
			ent->set_inactive();
			from_ent.increment();
		}
	}

	// now: there may be more *new* entities, so finish the list
	while (to_index != ENTITY_SENTINAL && !msg.HasFailed())
	{
		Entity* ent = (to_index == ENTITY_SENTINAL) ? nullptr : &eng->ents[to_index];
		read_entity_from_snapshot(ent, to_index, msg, false, nullptr, last_recieved_server_tick);

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

#endif
	return false;
}