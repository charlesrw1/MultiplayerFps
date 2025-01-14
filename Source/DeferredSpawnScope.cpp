#include "DeferredSpawnScope.h"
#include "Game/Entity.h"
#include "GameEnginePublic.h"
#include "Level.h"
DeferredSpawnScope::~DeferredSpawnScope()
{
	eng->get_level()->initialize_new_entity_safe(entityPtr);
}