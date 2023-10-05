#pragma once

const float STANDING_EYE_OFFSET = 1.2f;
const float CROUCH_EYE_OFFSET = 0.6f;
const float CHAR_HITBOX_RADIUS = 0.2f;
const float CHAR_STANDING_HB_HEIGHT = 1.3f;
const float CHAR_CROUCING_HB_HEIGHT = 0.75f;

enum WpnTypes {
	Wpn_Rifle,
	Wpn_Knife,
	NUM_WPNS
};

struct WpnStatInfo {
	const char* model_name;
	int clip_size;
	float reload_spd;
	float fire_rate;
	float dmg;
};