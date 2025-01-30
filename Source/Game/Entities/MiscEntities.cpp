#include "MiscEntities.h"

CLASS_IMPL(Door);
CLASS_IMPL(Grenade);
CLASS_IMPL(NPC);



Grenade::Grenade()
{
}


void Grenade::update()
{
}


void Door::update()
{
	float rotation_y = eng->get_game_time()*PI*0.5;
	set_ws_rotation(glm::vec3(0, rotation_y, 0));
}

DECLARE_ENGINE_CMD(spawn_npc)
{
	if (!eng->get_level())
		return;

	//auto p = checked_cast<Player>(eng->get_local_player());
	//if (!p) {
	//	sys_print("no player\n");
	//	return;
	//}
	//auto va = p->view_angles;
	//vec3 front = AnglesToVector(va.x, va.y);
	//vec3 pos = p->calc_eye_position();
	//
	//auto npc = eng->spawn_entity_class<NPC>();
	//npc->position = pos;
}
