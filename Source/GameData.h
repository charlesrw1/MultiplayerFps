#pragma once

const float STANDING_EYE_OFFSET = 1.6f;
const float CROUCH_EYE_OFFSET = 1.1f;
const float CHAR_HITBOX_RADIUS = 0.3f;
const float CHAR_STANDING_HB_HEIGHT = 1.8f;
const float CHAR_CROUCING_HB_HEIGHT = 1.3f;

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