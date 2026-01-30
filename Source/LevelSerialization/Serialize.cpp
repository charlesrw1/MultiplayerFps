#include "SerializationAPI.h"
#include <unordered_map>
#include <unordered_set>
#include <stdexcept>
#include "Framework/Util.h"
#include "Game/Entity.h"
#include "Game/EntityComponent.h"
#include "Framework/DictWriter.h"
#include "Level.h"
#include "Game/LevelAssets.h"
#include "GameEnginePublic.h"
#include "Framework/ReflectionProp.h"

// TODO prefabs
// rules:
// * path based on source



bool serialize_this_objects_children(const Entity* b)
{
	if (b->dont_serialize_or_edit)
		return false;
	return true;
}

bool this_is_a_serializeable_object(const BaseUpdater* b)
{
	assert(b);
	if (b->dont_serialize_or_edit)
		return false;

	if (auto as_comp = b->cast_to<Component>()) {
		assert(as_comp->get_owner());
		if (!serialize_this_objects_children(as_comp->get_owner()))
			return false;
	}
	return true;
}

#include "LevelSerialization/SerializeNew.h"

