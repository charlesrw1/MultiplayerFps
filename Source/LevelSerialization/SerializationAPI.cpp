#include "SerializationAPI.h"
#include "Game/Entity.h"
#include "Assets/IAsset.h"

bool PrefabToolsUtil::is_this_the_root_of_the_prefab(const Entity* e)
{
	ASSERT(e);
	ASSERT(is_part_of_a_prefab(e));	// must have a prefab
	auto parent = e->get_parent();
	if (!parent)
		return true;
	return parent->owner_asset != e->owner_asset;
}
Entity* PrefabToolsUtil::find_root_of_this_prefab(Entity* e)
{
	ASSERT(e);
	ASSERT(is_part_of_a_prefab(e));

	Entity* check = e;
	for (;;) {
		ASSERT(check);// check will never be null
		const bool is_root = is_this_the_root_of_the_prefab(check);
		if (is_root)
			return check;
		check = check->get_parent();
	}
}

bool PrefabToolsUtil::is_part_of_a_prefab(const BaseUpdater* e)
{
	ASSERT(e);
	return get_prefab_of_object(e) != nullptr;
}
PrefabAsset* PrefabToolsUtil::get_prefab_of_object(const BaseUpdater* e)
{
	ASSERT(e->owner_asset);
	return e->owner_asset->cast_to<PrefabAsset>();
}

bool PrefabToolsUtil::am_i_the_root_prefab_node_for_this_prefab(const Entity* b, const PrefabAsset* for_prefab)
{
	ASSERT(for_prefab);
	ASSERT(b);
	PrefabAsset* as_prefab = get_prefab_of_object(b);
	ASSERT(as_prefab);	// must be a prefab
	if (as_prefab != for_prefab)
		return false;
	const bool am_i_root = PrefabToolsUtil::is_this_the_root_of_the_prefab(b);
	return am_i_root;
}

bool PrefabToolsUtil::this_is_created_by(const BaseUpdater* b, const IAsset* for_asset)
{
	ASSERT(b);
	ASSERT(for_asset);
	ASSERT(b->owner_asset);
	return b->owner_asset == for_asset;
}
