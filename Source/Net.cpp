#include "Net.h"

#define ESP(x) #x,(int)&((Entity*)0)->x
#define ESPO(x) (int)&((Entity*)0)->x

Net_Prop entity_state_props[] =
{
	{ESP(type), 32, 8},
	{ESP(position.x), 0, -1, 1.f, Net_Prop::DEFAULT_PROP | Net_Prop::PLAYER_PROP},
	{ESP(position.y), 0, -1, 1.f, Net_Prop::DEFAULT_PROP | Net_Prop::PLAYER_PROP},
	{ESP(position.z), 0, -1, 1.f, Net_Prop::DEFAULT_PROP | Net_Prop::PLAYER_PROP},

	{ESP(model_index),		32, 8},

	{ESP(rotation.x), 0, 8, 256.0 / (2 * PI), Net_Prop::NON_PLAYER_PROP},
	{ESP(rotation.y), 0, 8, 256.0 / (2 * PI), Net_Prop::NON_PLAYER_PROP},
	{ESP(rotation.z), 0, 8, 256.0 / (2 * PI), Net_Prop::NON_PLAYER_PROP},

	{ESP(anim.m.staging_anim),		32,		8,		1.f,	Net_Prop::DEFAULT_PROP},
	{ESP(anim.m.staging_speed),		0,		8,		-10.f,	Net_Prop::DEFAULT_PROP},
	{ESP(anim.m.staging_loop),		8,		1,		1.f,	Net_Prop::DEFAULT_PROP},
	{ESP(anim.m.staging_frame),		0,		16,		100.f,	Net_Prop::DEFAULT_PROP},
	{ESP(anim.legs.staging_anim),	32,		8,		1.f,	Net_Prop::DEFAULT_PROP},
	{ESP(anim.legs.staging_speed),	0,		8,		-10.f,	Net_Prop::DEFAULT_PROP},
	{ESP(anim.legs.staging_loop),	8,		1,		1.f,	Net_Prop::DEFAULT_PROP},
	{ESP(anim.legs.staging_frame),	0,		16,		100.f,	Net_Prop::DEFAULT_PROP},

	{ESP(inv.active_item),	32,		5},
	{ESP(flags), 16},

	// for owning players only
	{ESP(state), 16, 16, 1.f, Net_Prop::PLAYER_PROP_MASK},
	{ESP(rotation.x), 0, -1, 1.f, Net_Prop::PLAYER_PROP},	// dont quantize for predicting player
	{ESP(rotation.y), 0, -1, 1.f, Net_Prop::PLAYER_PROP},
	{ESP(rotation.z), 0, -1, 1.f, Net_Prop::PLAYER_PROP},
	//
	{ESP(velocity.x), 0, -1, 1.f, Net_Prop::PLAYER_PROP},
	{ESP(velocity.y), 0, -1, 1.f, Net_Prop::PLAYER_PROP},
	{ESP(velocity.z), 0, -1, 1.f, Net_Prop::PLAYER_PROP},
	//
};

//#define NET_PROP_DBG
int net_prop_out_bits(Net_Prop& prop)
{
	int bits = prop.output_bits;
	if (bits == -1) bits = prop.input_bits;
	if (bits == 0) bits = 32;

#ifdef NET_PROP_DBG
	bits += 8*(1+strlen(prop.name));
#endif // NET_PROP_DBG


	return bits;
}

void write_net_prop_name(Net_Prop& prop, ByteWriter& msg)
{
	int len = strlen(prop.name);
	msg.WriteByte(len);
	msg.WriteBytes((uint8_t*)prop.name, len);
}
bool validate_net_prop_name(Net_Prop& prop, ByteReader& msg)
{
	char buffer[64];
	int len = msg.ReadByte();
	if (len < 64) {
		msg.ReadBytes((uint8_t*)buffer, len);
		buffer[len] = 0;
		if (strcmp(buffer, prop.name) != 0) {
			printf("invalid field\n");
			return false;
		}
	}
	else {
		printf("invalid field\n");
		return false;
	}
	return true;
}

void net_prop_field_read(ByteReader& msg, void* output, Net_Prop& prop, int condition, bool is_delta)
{
	bool should_read = prop.condition & condition;
	uint8_t* prop_start = ((uint8_t*)(output)+prop.offset);
	int out_bits = (prop.output_bits == -1) ? prop.input_bits : prop.output_bits;
	int in_bits = prop.input_bits;

	if (is_delta) {
		if (should_read) {
			if (!msg.ReadBool())
				return;
		}
		else
			return;
	}
	else if (!should_read) {
		out_bits = (out_bits == 0) ? 32 : out_bits;
		msg.ReadBits(out_bits);
		return;
	}


#ifdef  NET_PROP_DBG
	validate_net_prop_name(prop, msg);
#endif //  NET_PROP_DBG


	switch (in_bits)
	{
	case 8:
		*(uint8_t*)prop_start = msg.ReadBits(out_bits);
		break;
	case 16:
		*(uint16_t*)prop_start = msg.ReadBits(out_bits);
		break;
	case 32:
		*(uint32_t*)prop_start = msg.ReadBits(out_bits);
		break;
	case 0:
		if (out_bits == 0)
			*(float*)prop_start = msg.ReadFloat();
		else {
			if (prop.quantize < 0) {
				int bits = msg.ReadBits(out_bits);
				bits -= (1 << (out_bits-1));
				*(float*)prop_start = bits / -prop.quantize;
			}
			else
				*(float*)prop_start = msg.ReadBits(out_bits) / prop.quantize;
		}
		break;
	}
}

void net_prop_field_write(Net_Prop& prop, void* output, ByteWriter& msg)
{
	int out_bits = (prop.output_bits == -1) ? prop.input_bits : prop.output_bits;
	uint8_t* prop_start = ((uint8_t*)(output)+prop.offset);

#ifdef  NET_PROP_DBG
	write_net_prop_name(prop, msg);
#endif //  NET_PROP_DBG

	switch (prop.input_bits)
	{
	case 8: {
		uint8_t p = *(prop_start);
		msg.WriteBits(p, out_bits);
	}break;
	case 16: {
		uint16_t p = *(uint16_t*)(prop_start);
		msg.WriteBits(p, out_bits);
	}break;
	case 32: {
		uint32_t p = *(uint32_t*)(prop_start);
		msg.WriteBits(p, out_bits);
	}break;
	case 0: {
		float p = *(float*)(prop_start);
		if (out_bits == 0) {
			msg.WriteFloat(p);
		}
		else {
			if (prop.quantize < 0) {
				int quantized = p * -prop.quantize;
				quantized += (1 << (out_bits-1));
				msg.WriteBits(quantized, out_bits);
			}
			else {
				int quantized = p * prop.quantize;
				msg.WriteBits(quantized, out_bits);
			}
		}
	}break;
	default:
		ASSERT("wrong input bits" && 0);
	}
}

// copies b=a
void net_prop_copy_field(Net_Prop& prop, void* a, void* b, int condition)
{
	bool should_read = prop.condition & condition;

	if (!should_read)
		return;

	uint8_t* astart = ((uint8_t*)(a)+prop.offset);
	uint8_t* bstart = ((uint8_t*)(b)+prop.offset);

	switch (prop.input_bits)
	{
	case 8:
		*(uint8_t*)bstart = *(uint8_t*)astart;
		break;
	case 16:
		*(uint16_t*)bstart = *(uint16_t*)astart;
		break;
	case 32:
		*(uint32_t*)bstart = *(uint32_t*)astart;
		break;
	case 0:
		*(float*)bstart = *(float*)astart;
		break;
	}
}

void write_full_entity(Entity* e, ByteWriter& msg)
{
	int num_props = sizeof(entity_state_props) / sizeof(Net_Prop);

	for (int i = 0; i < num_props; i++)
	{
		Net_Prop& prop = entity_state_props[i];
		net_prop_field_write(prop, e, msg);
	}
}

// msg is a buffer that either has a full state binary layout, or a delta layout
void read_entity(Entity* e, ByteReader& msg, int condition, bool is_delta)
{
	int num_props = sizeof(entity_state_props) / sizeof(Net_Prop);

	// s0 and s1 point to the same entity data buffer
	for (int i = 0; i < num_props; i++)
	{
		Net_Prop& prop = entity_state_props[i];
		net_prop_field_read(msg, e, prop, condition, is_delta);
	}
}
void set_entity_props_from_entity(Entity* from, Entity* to, int condition)
{

	int num_props = sizeof(entity_state_props) / sizeof(Net_Prop);

	// s0 and s1 point to the same entity data buffer
	for (int i = 0; i < num_props; i++)
	{
		Net_Prop& prop = entity_state_props[i];
		net_prop_copy_field(prop, from, to, condition);
	}
}


void write_delta_entity(ByteWriter& msg, ByteReader& s0, ByteReader& s1, int condition)
{
	int num_props = sizeof(entity_state_props) / sizeof(Net_Prop);

	// s0 and s1 point to the same entity data buffer
	for (int i = 0; i < num_props; i++)
	{
		Net_Prop& prop = entity_state_props[i];
		bool should_write = (prop.condition & condition);

		int out_bits = (prop.output_bits == -1) ? prop.input_bits : prop.output_bits;
		if (out_bits == 0)
			out_bits = 32;
#ifdef NET_PROP_DBG
		validate_net_prop_name(prop, s0);
		validate_net_prop_name(prop, s1);
#endif // NET_PROP_DBG

		uint32_t p0 = s0.ReadBits(out_bits);
		uint32_t p1 = s1.ReadBits(out_bits);

		if (should_write) {
			if (p0 != p1) {
				msg.WriteBool(1);
#ifdef NET_PROP_DBG
				write_net_prop_name(prop, msg);
#endif // NET_PROP_DBG

				msg.WriteBits(p1, out_bits);
			}
			else
				msg.WriteBool(0);
		}
	}
}

Entity_Baseline* get_entity_baseline()
{
	static Entity_Baseline* baseline = nullptr;
	if (!baseline)
	{
		// FIXME: never gets freed
		baseline = new Entity_Baseline;
		Entity baseline_ent = Entity();	// use initialized vars as baseline
		ByteWriter w(baseline->data, 1000);
		write_full_entity(&baseline_ent, w);
		w.EndWrite();
		ASSERT(!w.HasFailed());
		baseline->num_bytes_in_data = w.BytesWritten();
		
		int total_bits = ENTITY_BITS;
		int props = sizeof(entity_state_props) / sizeof(Net_Prop);
		for (int i = 0; i < props; i++)
			total_bits += net_prop_out_bits(entity_state_props[i]);

		baseline->num_bytes_including_index = (total_bits / 8) + ((total_bits % 8) != 0);

	}
	return baseline;
}
