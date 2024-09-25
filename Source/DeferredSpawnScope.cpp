#include "DeferredSpawnScope.h"
#include "Game/Entity.h"
DeferredSpawnScope::~DeferredSpawnScope()
{
	entityPtr->initialize();
}