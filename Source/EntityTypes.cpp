#include "EntityTypes.h"
#include "Framework/Factory.h"

Factory<std::string, Entity>& get_entityfactory() {
	static Factory<std::string, Entity> inst;
	return inst;
}

ENTITY_IMPL(Door);
ENTITY_IMPL(Grenade);
ENTITY_IMPL(NPC);
