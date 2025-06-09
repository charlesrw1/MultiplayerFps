#include "Unittest.h"

#include "serialize_test_bench.h"
#include "Game/Components/MeshComponent.h"
#include "Game/Components/LightComponents.h"
#include "Game/TopDownShooter/TopDownPlayer.h"

#include "LevelSerialization/SerializeNew.h"




static Component* add_component(Entity* e, Component* c)
{
	e->add_component_from_unserialization(c);
	return c;
}
static Entity* create_child(Entity* parent)
{
	Entity* e = new Entity;
	e->parent_to(parent);
	return e;
}
static void set_from_prefab_recursive(Entity* e, PrefabAsset* asset)
{
	for (auto c : e->get_components())
		c->what_prefab = asset;
	for (auto child : e->get_children())
		set_from_prefab_recursive(child, asset);
}

static bool check_file_ids(unordered_set<BaseUpdater*>& inobjs) {
	unordered_set<int> fileids;
	for (auto o : inobjs) {
		if (fileids.find(o->unique_file_id) != fileids.end())
			return false;
		fileids.insert(o->unique_file_id);
	}
	return true;
}

ADD_TEST(serialize_scene_new)
{
	SerializeTestWorkbench workbench;
	Entity* root = workbench.add_entity();
	Component* root_mc = add_component(root, new MeshComponent);
	Entity* root_child0 = create_child(root);
	Component* root_child0_mc = add_component(root_child0, new MeshComponent);
	Entity* root_child1 = create_child(root);
	Entity* root_child1_child0 = create_child(root_child1);
	Component* root_child1_child0_mc = add_component(root_child1_child0, new MeshComponent);
	auto prefab = workbench.create_prefab(root, "myprefab.pfb");
	workbench.post_unserialization_R(root);
	set_from_prefab_recursive(root, prefab);
	root->is_root_of_prefab = true;
	{
		auto roots = root_objects_to_write({ root });
		checkTrue(roots.size() == 1 && roots[0] == root);
	}
	{
		SerializeEntitiesContainer container;
		SerializedSceneFile out;
		NewSerialization::add_objects_to_container({ root }, container, prefab, out);
		checkTrue(container.objects.size() == 7);
		checkTrue(out.extern_parents.size() == 0);

		checkTrue(check_file_ids(container.objects));
	}


	auto file = NewSerialization::serialize_to_text({ root }, prefab);
	auto unserialized = NewSerialization::unserialize_from_text(file.text, AssetDatabase::loader, prefab);
	checkTrue(unserialized.get_objects().size() == 7);

}