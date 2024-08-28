#include "MiscEntities.h"

CLASS_IMPL(Door);
CLASS_IMPL(Grenade);
CLASS_IMPL(NPC);



Grenade::Grenade()
{
}


void Grenade::update()
{

	if (eng->get_game_time() - throw_time > 2.5) {
		sys_print("BOOM\n");

		eng->remove_entity(this);
	}
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

#include "Sound/SoundPublic.h"
#include "Render/Texture.h"
#include "Render/MaterialPublic.h"
CLASS_H(PhysicsMaterialDefinition, ClassBase)
public:
	float staticFriction = 0.1;
	float dynamicFriction = 0.7;
	float restitution = 0.8;

	AssetPtr<SoundFile> footStepSound;
	AssetPtr<SoundFile> bulletImpactSound;
	std::vector<AssetPtr<MaterialInstance>> gunshotImpactDecals;
	// particle effects too

	static const PropertyInfoList* get_props() {
		MAKE_VECTORCALLBACK_ATOM(AssetPtr<MaterialInstance >, gunshotImpactDecals);
		START_PROPS(PhysicsMaterialDefinition)
			REG_ASSET_PTR(footStepSound,PROP_DEFAULT),
			REG_ASSET_PTR(bulletImpactSound,PROP_DEFAULT),
			REG_STDVECTOR(gunshotImpactDecals, PROP_DEFAULT),
			REG_FLOAT(staticFriction,PROP_DEFAULT, "0.1"),
			REG_FLOAT(dynamicFriction,PROP_DEFAULT,"0.7"),
			REG_FLOAT(restitution,PROP_DEFAULT,"0.8")
		END_PROPS(PhysicsMaterialDefinition)
	}
};
CLASS_IMPL(PhysicsMaterialDefinition);