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

BaseUpdater* LevelSerializationContext::get_object(uint64_t handle)
{
	ASSERT(out&&!in);
	bool is_from_diff = handle & (1ull << 63ull);
	BaseUpdater* obj = nullptr;
	if (is_from_diff) {
		ASSERT(diffprefab)
		obj= diffprefab->find_entity(handle);
	}
	else
		obj = eng->get_level()->get_entity(handle);
	return obj;
}


bool serialize_this_objects_children(const Entity* b)
{
	if (b->dont_serialize_or_edit)
		return false;
	if (b->get_object_prefab_spawn_type() != EntityPrefabSpawnType::None)
		return false;
	return true;
}

bool this_is_a_serializeable_object(const BaseUpdater* b)
{
	assert(b);
	if (b->dont_serialize_or_edit)
		return false;
	if (auto as_ent = b->cast_to<Entity>()) {
		if (as_ent->get_object_prefab_spawn_type() == EntityPrefabSpawnType::SpawnedByPrefab)
			return false;
	}
	else if (auto as_comp = b->cast_to<Component>()) {
		assert(as_comp->get_owner());
		if (!serialize_this_objects_children(as_comp->get_owner()))
			return false;
	}
	return true;
}

void add_to_write_set_R(Entity* o, std::unordered_set<Entity*>& to_write)
{
	auto dont_serialize_or_edit_me = [](auto&& self, Entity* e) -> bool {
		if (e->dont_serialize_or_edit)
			return true;
		if (e->get_parent())
			return self(self, e->get_parent());
		return false;
	};

	if (dont_serialize_or_edit_me(dont_serialize_or_edit_me,o))
		return;
	to_write.insert(o);
	for (auto c : o->get_children())
		add_to_write_set_R(c, to_write);

}
std::vector<Entity*> root_objects_to_write(const std::vector<Entity*>& input_objs)
{
	std::unordered_set<Entity*> to_write;
	for (auto o : input_objs) {
		add_to_write_set_R(o,to_write);
	}

	std::vector<Entity*> roots;
	for (auto o : to_write) {
		if (!o->get_parent() || to_write.find(o->get_parent()) == to_write.end())
			roots.push_back(o);
	}

	return roots;
}

void add_to_extern_parents(const BaseUpdater* obj, const BaseUpdater* parent, const PrefabAsset* for_prefab, SerializedSceneFile& output)
{
	SerializedSceneFile::external_parent ext;
	assert(obj->unique_file_id != 0);
	ext.child_id = obj->unique_file_id;
	//ext.child_path = std::to_string(obj->unique_file_id);
	ext.external_parent_handle = parent->get_instance_id();
	output.extern_parents.push_back(ext);
}
#include "LevelSerialization/SerializeNew.h"

SerializedSceneFile serialize_entities_to_text(const char* debug_tag, const std::vector<Entity*>& input_objs)
{
	return NewSerialization::serialize_to_text(debug_tag, input_objs);
}