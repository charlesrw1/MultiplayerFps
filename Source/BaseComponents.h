#pragma once

#include "glm/glm.hpp"
#include <vector>

struct RenderInterpolationComponent
{
	RenderInterpolationComponent(int size) :
		interpdata(size),
		interpdata_head(0) {}
	glm::vec3 lerped_pos;
	glm::vec3 lerped_rot;

	float max_extrapolate_time = 0.2f;
	float telport_velocity_threshold = 30.f;	// how many units/time before disabling interpolation

	void add_state(float time, glm::vec3 p, glm::vec3 r);
	void evaluate(float time);
	void clear();

private:
	struct InterpolationData
	{
		float time;
		glm::vec3 position;
		glm::vec3 rotation;
	};
	InterpolationData& get(int index);
	std::vector<InterpolationData> interpdata;
	int interpdata_head;
};

struct LagCompState
{
	int startmatrix = 0;
	int matrixcount = 0;
	glm::vec3 pos = glm::vec3(0.0);
	glm::vec3 rot = glm::vec3(0.0);
	int tick = 0;
};

// uses 10*60*48 + 36*60 + 4 = ~31kb per lag compped
struct LagCompensationComponent
{
	LagCompensationComponent() :
		hitbox_matricies(10 * 60, glm::mat4x3(1)),
		state_hist(60),
		matrixhead(0) {}
	std::vector<glm::mat4x3> hitbox_matricies;
	std::vector<LagCompState> state_hist;
	int matrixhead;
};



enum Item_Use_State
{
	ITEM_IDLE,
	ITEM_IN_FIRE,
	ITEM_RELOAD,
	ITEM_SCOPED,
	ITEM_RAISING,
	ITEM_LOWERING,
	ITEM_USING,
};

enum Game_Item_Category
{
	ITEM_CAT_RIFLE,
	ITEM_CAT_BOLT_ACTION,
	ITEM_CAT_BOMB,
	ITEM_CAT_MELEE,
	ITEM_CAT_THROWABLE,
};


struct Game_Item_Stats
{
	const char* name = "";
	const char* world_model = "";
	const char* view_model = "";
	int category = 0;
	int param = 0;

	float fire_rate = 1.f;
	float reload_time = 0.0;
	float holster_time = 0.0;
	float draw_time = 0.0;
	int damage = 0;
	int clip_size = 0;
	int start_ammo = 0;
	float spread = 0.f;
};

struct Game_Inventory
{
	enum {
		UNEQUIP = 0,
		GUN_M16,
		GUN_AK47,
		GUN_M24,
		MELEE_KNIFE,
		ITEM_BOMB,
		ITEM_HE_GRENADE,

		NUM_GAME_ITEMS
	};

	int active_item = UNEQUIP;
	int item_mask = 0;
	int ammo[NUM_GAME_ITEMS];
	int clip[NUM_GAME_ITEMS];
	int state = ITEM_IDLE;
	float timer = 0.f;

	int pending_item = -1;

	int tick_for_staging = 0;// horrible hack again
	int staging_item = 0;
	int staging_ammo = 0;
	int staging_clip = 0;

};
Game_Item_Stats* get_item_stats();	// size = NUM_GAME_ITEMS

