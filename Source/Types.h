#ifndef GAMETYPE_H
#define GAMETYPE_H
#include "Util.h"
#include "Animation.h"
#include <memory>
using std::unique_ptr;

#define EDITDOC

const int MAX_CLIENTS = 16;
const int DEFAULT_WIDTH = 1200;
const int DEFAULT_HEIGHT = 800;

const int ENTITY_BITS = 8;
const int NUM_GAME_ENTS = 1 << ENTITY_BITS;
const int ENTITY_SENTINAL = NUM_GAME_ENTS - 1;
const int SPAWNID_BITS = 3;

const float STANDING_EYE_OFFSET = 1.6f;
const float CROUCH_EYE_OFFSET = 1.1f;
const float CHAR_HITBOX_RADIUS = 0.3f;
const float CHAR_STANDING_HB_HEIGHT = 1.8f;
const float CHAR_CROUCING_HB_HEIGHT = 1.3f;

class User_Camera
{
public:
	glm::vec3 orbit_target = glm::vec3(0.f);
	glm::vec3 position = glm::vec3(0);
	glm::vec3 front = glm::vec3(1, 0, 0);
	glm::vec3 up = glm::vec3(0, 1, 0);
	float move_speed = 0.1f;
	float yaw = 0, pitch = 0;

	bool orbit_mode = false;

	void scroll_callback(int amt);
	void update_from_input(const bool keys[], int mouse_dx, int mouse_dy, glm::mat4 invproj);
	glm::mat4 get_view_matrix() const;
};

struct View_Setup
{
	glm::vec3 origin, front;
	glm::mat4 view, proj, viewproj;
	float fov, near, far;
	int width, height;
};

enum Game_Command_Buttons
{
	BUTTON_JUMP = (1),
	BUTTON_DUCK = (1 << 2),
	BUTTON_FIRE1 = (1 << 3),
	BUTTON_FIRE2 = (1 << 4),
	BUTTON_RELOAD = (1 << 5),
	BUTTON_USE = (1 << 6),

	BUTTON_ITEM1 = (1 << 8),
	BUTTON_ITEM2 = (1 << 9),
	BUTTON_ITEM3 = (1 << 10),
	BUTTON_ITEM4 = (1 << 11),
	BUTTON_ITEM5 = (1 << 12),
	BUTTON_ITEM_NEXT = (1 << 12),
	BUTTON_ITEM_PREV = (1 << 13),
};

struct Move_Command
{
	int tick = 0;
	float forward_move = 0.f;
	float lateral_move = 0.f;
	float up_move = 0.f;
	int button_mask = 0;
	glm::vec3 view_angles = glm::vec3(0.f);

	bool first_sim = true;	// not replicated, used in player_updates

	static uint8_t quantize(float f) {
		return glm::clamp(int((f + 1.0) * 128.f), 0, 255);
	}
	static float unquantize(uint8_t c) {
		return (float(c) - 128.f) / 128.f;
	}
};




#endif // !GAMETYPE_H
