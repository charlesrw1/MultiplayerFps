#include "Net.h"
#include "Types.h"
#include "Framework/Bytepacker.h"
#define ESP(x) #x,(int)&((Entity*)0)->x
#define ESPO(x) (int)&((Entity*)0)->x

#define OFFSET(type, var) #var,(int)&((type*)0)->var
#define ONS(var) OFFSET(Obj_Net_State, var)

#define VECTORQUANTIZE(bits, fac) -3, bits, fac 
#define VECTORPROP -3, -1, 1.f
#define INT32PROP(outbits) 32, outbits, -1
#define INT8PROP(outbits) 8, outbits, -1
#define FLOATPROP 0, -1, 1.f
#define FLOATQUANTIZE(outbits, fac) 0,outbits,fac

#define SENDFORPLAYERONLY Net_Prop::PLAYER_PROP
#define SENDFORALL Net_Prop::DEFAULT_PROP
#define SENDFORALLBUTPLAYER Net_Prop::NON_PLAYER_PROP

Net_Prop entity_state_props[] =
{
	{ONS(pos.x),	VECTORPROP,						SENDFORALL},
	{ONS(rot.x),	VECTORQUANTIZE(8, 256.0/(2*PI)),SENDFORALLBUTPLAYER},
	{ONS(model),	INT32PROP(12),					SENDFORALL},
	{ONS(item),		INT32PROP(6),					SENDFORALL},
	{ONS(state),	INT32PROP(16),					SENDFORALL},
	{ONS(flags),	INT32PROP(16),					SENDFORALL},
	{ONS(vel),		VECTORPROP,						SENDFORPLAYERONLY},
	{ONS(clip),		INT32PROP(9),					SENDFORPLAYERONLY},
	{ONS(ammo),		INT32PROP(10),					SENDFORPLAYERONLY},
	{ONS(rot.x),	VECTORPROP,						SENDFORPLAYERONLY}
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
