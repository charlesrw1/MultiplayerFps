#include "BaseUpdater.h"
#include "Level.h"
#include "GameEnginePublic.h"
#include "Scripting/FunctionReflection.h"
#include "Framework/ReflectionMacros.h"

void BaseUpdater::destroy_deferred()
{
	eng->get_level()->queue_deferred_delete(this);
}
