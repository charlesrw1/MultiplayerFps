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


//	auto file = NewSerialization::serialize_to_text({ root }, prefab);
	//auto unserialized = NewSerialization::unserialize_from_text(file.text, AssetDatabase::loader, prefab);
//	checkTrue(unserialized.get_objects().size() == 7);

}

class EntityBuilder
{
public:
	EntityBuilder();
};

struct PrefabTuple {
	PrefabTuple(string name, bool global=false):is_global(global) {
		asset.editor_set_newly_made_path(name);
	}
	bool is_global = false;
	int id = 1;
	PrefabAsset asset;
	vector<BaseUpdater*> objs;
};
BaseUpdater* set_prefab(BaseUpdater* b, PrefabTuple& p) {
	b->unique_file_id = p.id++;
	if(!p.is_global)
		b->what_prefab = &p.asset;
	p.objs.push_back(b);
	return b;
}
Entity* set_entity_owner_prefab(Entity* e, PrefabTuple& p, PrefabTuple& owner) {
	e->unique_file_id = owner.id++;
	if(!p.is_global)
		e->what_prefab = &p.asset;
	if (!owner.is_global)
		e->set_nested_owner_prefab(&owner.asset);
	owner.objs.push_back(e);
	return e;
}

ADD_TEST(prefab_utils)
{
	SerializeTestWorkbench workbench;
	PrefabTuple global("global", true);
	PrefabTuple prefab1("p1");
	PrefabTuple prefab2("p2");
	PrefabTuple prefab3("p3");

	Entity* e1 = workbench.add_entity();
	set_entity_owner_prefab(e1, prefab1, global);
	auto e1_child = set_prefab(create_child(e1), prefab1);
	
	Entity* e2 = create_child(e1);
	set_entity_owner_prefab(e2, prefab2, prefab1);
	auto e2_c = set_prefab(add_component(e2, new Component), prefab2);
	Entity* e2_child = (Entity*)set_prefab(create_child(e2), prefab2);

	Entity* e1_child2 = create_child(e2);
	set_prefab(e1_child2, prefab1);

	Entity* e3 = create_child(e2);
	set_entity_owner_prefab(e3, prefab3, global);
	auto e3_child = set_prefab(create_child(e3),prefab3);


	//		e1(prefab)	e3(prefab)
	//	   /   \
	//	  child \
	//			 e2(prefab)

	checkTrue(e1 == &PrefabToolsUtil::find_root_of_this_prefab(*e1_child));
	checkTrue(e1 == &PrefabToolsUtil::find_root_of_this_prefab(*e1));
	checkTrue(e2 == &PrefabToolsUtil::find_root_of_this_prefab(*e2));
	checkTrue(e2 == &PrefabToolsUtil::find_root_of_this_prefab(*e2_c));
	checkTrue(e2 == &PrefabToolsUtil::find_root_of_this_prefab(*e2_child));
	checkTrue(e1 == &PrefabToolsUtil::find_root_of_this_prefab(*e1_child2));
	checkTrue(e3 == &PrefabToolsUtil::find_root_of_this_prefab(*e3));
	checkTrue(e3 == &PrefabToolsUtil::find_root_of_this_prefab(*e3_child));


	checkTrue(e1 == PrefabToolsUtil::get_outer_prefab(*e2));
	checkTrue(nullptr == PrefabToolsUtil::get_outer_prefab(*e1));
	checkTrue(e2 == PrefabToolsUtil::get_outer_prefab(*e2_c));
	checkTrue(e1 == PrefabToolsUtil::get_outer_prefab(*e1_child));
	checkTrue(e2 == PrefabToolsUtil::get_outer_prefab(*e2_child));
	checkTrue(e1 == PrefabToolsUtil::get_outer_prefab(*e1_child2));
	checkTrue(nullptr == PrefabToolsUtil::get_outer_prefab(*e3));
	checkTrue(e3 == PrefabToolsUtil::get_outer_prefab(*e3_child));


	checkTrue(PrefabToolsUtil::is_this_the_root_of_the_prefab(*e1));
	checkTrue(PrefabToolsUtil::is_this_the_root_of_the_prefab(*e2));
	checkTrue(PrefabToolsUtil::is_this_the_root_of_the_prefab(*e3));
	checkTrue(!PrefabToolsUtil::is_this_the_root_of_the_prefab(*e2_child));
	checkTrue(!PrefabToolsUtil::is_this_the_root_of_the_prefab(*e1_child2));

	checkTrue(PrefabToolsUtil::is_newly_created_nested(*e2_child, &prefab1.asset));
	checkTrue(PrefabToolsUtil::is_newly_created_nested(*e2, &prefab1.asset));
	checkTrue(!PrefabToolsUtil::is_newly_created_nested(*e1, &prefab1.asset));
	checkTrue(PrefabToolsUtil::is_newly_created_nested(*e1_child, &prefab1.asset));
	checkTrue(!PrefabToolsUtil::is_newly_created_nested(*e3, &prefab1.asset));


	checkTrue(PrefabToolsUtil::is_newly_created(*e1_child2, &prefab1.asset));
	checkTrue(PrefabToolsUtil::is_newly_created(*e2, &prefab1.asset));
	checkTrue(!PrefabToolsUtil::is_newly_created(*e2_child, &prefab1.asset));
	checkTrue(PrefabToolsUtil::is_newly_created(*e1, &prefab1.asset));
	checkTrue(PrefabToolsUtil::is_newly_created(*e1_child, &prefab1.asset));
	checkTrue(!PrefabToolsUtil::is_newly_created(*e1_child, nullptr));
	checkTrue(PrefabToolsUtil::is_newly_created(*e1, nullptr));
	checkTrue(PrefabToolsUtil::is_newly_created(*e3, nullptr));
	checkTrue(PrefabToolsUtil::is_newly_created(*e3_child, &prefab3.asset));
}