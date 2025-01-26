#include "DeferredSpawnScope.h"
#include "Game/Entity.h"
#include "GameEnginePublic.h"
#include "Level.h"
DeferredSpawnScope::~DeferredSpawnScope()
{
	eng->get_level()->initialize_new_entity_safe(entityPtr);
}
DeferredSpawnScopePrefab::~DeferredSpawnScopePrefab()
{
	if (file) {
		eng->get_level()->insert_unserialized_entities_into_level(*file);
		delete file;
	}
}