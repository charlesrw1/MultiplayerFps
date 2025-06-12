#include "SerializationAPI.h"
#include "Game/Entity.h"
#include "Assets/IAsset.h"
#include "Game/EntityComponent.h"
bool PrefabToolsUtil::is_this_the_root_of_the_prefab(const Entity& e)
{
	ASSERT(is_part_of_a_prefab(e));
	const Entity& root = find_root_of_this_prefab(e);
	return &root == &e;
}
const Entity& PrefabToolsUtil::find_root_of_this_prefab(const BaseUpdater& e)
{
	ASSERT(e.what_prefab);
	return find_root_of_this_one_prefab(e, *e.what_prefab);
}

const Entity& PrefabToolsUtil::find_root_of_this_one_prefab(const BaseUpdater& e, const PrefabAsset& asset)
{
	const PrefabAsset* myprefab = &asset;
	ASSERT(myprefab);

	const Entity* check = e.is_a<Entity>() ? (Entity*)&e : ((Component*)&e)->get_owner();
	const Entity* last_found = nullptr;
	ASSERT(check);
	while (check) {
		if (check->what_prefab == myprefab) {
			last_found = check;
			if (check->get_nested_owner_prefab())
				break;	// know its a done
		}
		check = check->get_parent();
	}
	ASSERT(last_found);
	return *last_found;
}

const Entity* PrefabToolsUtil::get_outer_prefab(const BaseUpdater& e)
{
	if (is_part_of_a_prefab(e)) {
		const Entity* as_ent = e.cast_to<Entity>();
		if (as_ent&&is_this_the_root_of_the_prefab(*as_ent)) {
			const PrefabAsset* parentPrefab = as_ent->get_nested_owner_prefab();
			if (!parentPrefab)
				return nullptr;
			return &find_root_of_this_one_prefab(e, *parentPrefab);
		}
		return &find_root_of_this_prefab(e);
	}
	return nullptr;
}
bool PrefabToolsUtil::is_newly_created(const BaseUpdater& e, const PrefabAsset* editing_prefab)
{
	const Entity* outer = get_outer_prefab(e);
	if (!outer)
		return true;
	return outer->what_prefab == editing_prefab;
}
bool PrefabToolsUtil::is_newly_created_nested(const BaseUpdater& e, const PrefabAsset* editing_prefab)
{
	const Entity* outer = get_outer_prefab(e);
	while (outer) {
		if (outer->what_prefab == editing_prefab)
			return true;
		outer = get_outer_prefab(*outer);
	}
	return false;
}
bool PrefabToolsUtil::is_part_of_a_prefab(const BaseUpdater& e)
{
	return e.what_prefab != nullptr;
}

#include "Framework/Log.h"
bool PrefabToolsUtil::am_i_the_root_prefab_node_for_this_prefab(const Entity& b, const PrefabAsset* for_prefab)
{
	ASSERT(for_prefab);
	if (b.what_prefab != for_prefab)
		return false;
	return PrefabToolsUtil::is_this_the_root_of_the_prefab(b);
}
using std::unordered_set;
using std::unordered_map;
bool PrefabToolsUtil::validate_object_collection(const vector<BaseUpdater*>& objectsvec)
{
	// things to check:
	// valid file ids
	// prefab includes root and whole set
	unordered_set<BaseUpdater*> objects;
	for (auto o : objectsvec)
		objects.insert(o);

	unordered_map<PrefabAsset*, unordered_set<int>> used_ids;
	for (auto o : objects) {
		if (o->dont_serialize_or_edit)
			continue;

		PrefabAsset* what = o->what_prefab;
		int id = o->unique_file_id;
		if (id == BaseUpdater::INVALID_FILEID) {
			LOG_ERR("invalid id");
			return false;
		}
		auto& idset = used_ids[what];
//		if (idset.find(id) != used_ids.end()) {
//
//		}
	}


	return false;
}
