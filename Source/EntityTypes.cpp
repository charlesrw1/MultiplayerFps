#include "EntityTypes.h"
#include "Framework/Factory.h"

ABSTRACT_CLASS_IMPL_NO_PROPS(Entity, ClassBase);

CLASS_IMPL_NO_PROPS(Door, Entity);
CLASS_IMPL_NO_PROPS(Grenade, Entity);
CLASS_IMPL_NO_PROPS(NPC, Entity);
