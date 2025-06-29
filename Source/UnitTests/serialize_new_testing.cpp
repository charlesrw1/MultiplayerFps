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

#
#include "Framework/SerializerJson.h"
#include "LevelSerialization/SerializeNewMakers.h"
static bool are_objects_equivlent(ClassBase& o1, ClassBase& o2, IMakePathForObject& pathmaker)
{
	if (&o1.get_type() != &o2.get_type()) return false;

	WriteSerializerBackendJson writerA("TestEquivA", pathmaker,o1);
	WriteSerializerBackendJson writerB("TestEquivB", pathmaker,o2);
	if (!writerA.get_root_object())
		return false;
	if (!writerB.get_root_object())
		return false;

	auto theDiff = JsonSerializerUtil::diff_json(*writerA.get_root_object(), *writerB.get_root_object());
	return theDiff.empty();
}
static bool are_entity_trees_equivlent(Entity& e1,Entity& e2,MakePathForObjectNew& pathmaker)
{
	auto cmp_base_updaters = [&](BaseUpdater& l, BaseUpdater& r) -> bool{
		if (PrefabToolsUtil::is_part_of_a_prefab(l) != PrefabToolsUtil::is_part_of_a_prefab(r))
			return false;
		if (bool(PrefabToolsUtil::get_outer_prefab(l)) != bool(PrefabToolsUtil::get_outer_prefab(r)))
			return false;
		auto p1 = pathmaker.make_path(&l);
		auto p2 = pathmaker.make_path(&r);
		if (p1.path != p2.path || p1.is_subobject != p2.is_subobject)
			return false;
		if (!are_objects_equivlent(l, r, pathmaker))
			return false;
		return true;
	};
	if (!cmp_base_updaters(e1, e2))
		return false;
	if (PrefabToolsUtil::is_part_of_a_prefab(e1)) {	// e2 will be part_of_prefab also,checked above
		if (PrefabToolsUtil::is_this_the_root_of_the_prefab(e1) != PrefabToolsUtil::is_this_the_root_of_the_prefab(e2))
			return false;
	}
	if (e1.get_children().size() != e2.get_children().size()) {
		return false;
	}

	for (int i = 0; i < e1.get_children().size(); i++) {
		Entity* e1_c = e1.get_children().at(i);
		Entity* e2_c = e2.get_children().at(i);
		if (!e1_c || !e2_c) return false;
		bool same = are_entity_trees_equivlent(*e1_c, *e2_c, pathmaker);
		if (!same) 
			return false;
	}
	if (e1.get_components().size() != e2.get_components().size()) 
		return false;
	for (int i = 0; i < e1.get_components().size(); i++) {
		Component* e1_c = e1.get_components().at(i);
		Component* e2_c = e2.get_components().at(i);
		if (!e1_c || !e2_c) 
			return false;
		if (!cmp_base_updaters(*e1_c, *e2_c))
			return false;
	}
	return true;
}


ADD_TEST(test_are_objects_equivlent)
{
	SerializeTestWorkbench workbench;
	auto make_e1 = [&]() {
		Entity* e = workbench.add_entity();
		workbench.add_component<Component>(e);
		e->set_ls_position(glm::vec3(1.f));
		e->set_ls_scale(glm::vec3(2.f));
		e->set_ls_euler_rotation(glm::vec3(3.f));
		auto child = workbench.add_entity(e);
		child->set_ls_euler_rotation(glm::vec3(0.1,-0.10005,0.000014));
		return e;
	};
	auto make_e2 = [&]() {
		Entity* e = workbench.add_entity();
		e->set_ls_position(glm::vec3(-1.f,0.f,0.f));
		e->set_ls_scale(glm::vec3(3.f));
		e->set_ls_euler_rotation(glm::vec3(3.f));
		add_component(e, new Component);
		add_component(e, new Component);
		return e;
	};
	Entity* e1_1 = make_e1();
	Entity* e1_2 = make_e1();
	Entity* e2_1 = make_e2();

	MakePathForObjectNew pathmaker(nullptr);
	checkTrue(are_objects_equivlent(*e1_1, *e1_2, pathmaker));
	checkTrue(!are_objects_equivlent(*e1_1, *e2_1, pathmaker));

	WriteSerializerBackendJson writerA("TestEquivA", pathmaker, *e1_2);

	UnserializedSceneFile file;
	MakeObjectForPathNew objmaker(*g_assets.loader, file, nullptr);
	ReadSerializerBackendJson reader("dbg", writerA.get_output().dump(1), objmaker, *g_assets.loader);
	ClassBase* root = reader.get_root_obj();
	checkTrue(root && are_objects_equivlent(*root, *e1_2,pathmaker));


	auto outFile = serialize_entities_to_text("", { e1_2 }, nullptr);
	printf("%s\n", outFile.text.c_str());
	auto unFile = unserialize_entities_from_text("", outFile.text, g_assets.loader, nullptr);

	checkTrue(unFile.get_root_entity() && are_entity_trees_equivlent(*unFile.get_root_entity(), *e1_2, pathmaker));
}

ADD_TEST(path_builder)
{
	SerializeTestWorkbench workbench;
	Entity* e = workbench.add_entity();
	Entity* e1 = workbench.add_entity(e);
	auto pfb = workbench.create_prefab(e, "MyPrefab.pfb");
	e1->what_prefab = pfb;
	MakePathForObjectNew pathmaker(nullptr);
	checkTrue(pathmaker.make_path(e).path == std::to_string(e->unique_file_id));
	auto subpath = pathmaker.make_path(e1).path;
	checkTrue(subpath == std::to_string(e->unique_file_id)+"/"+std::to_string(e1->unique_file_id));
	{
		auto serialized = serialize_entities_to_text("", { e }, pfb);
		pfb->text = serialized.text;

		auto unserialized = unserialize_entities_from_text("", pfb->text, g_assets.loader, pfb);
		checkTrue(are_entity_trees_equivlent(*unserialized.get_root_entity(), *e, pathmaker));
	}


	PrefabAsset* loadPfb = workbench.load_prefab("BIGTANK.pfb");
	auto check_this = [&](Entity* root) {
		checkTrue(pathmaker.make_path(root).path == std::to_string(root->unique_file_id));
		auto child = root->get_children().at(0);
		auto childpath = pathmaker.make_path(child).path;
		checkTrue(childpath == std::to_string(root->unique_file_id) + "/" + std::to_string(child->unique_file_id));

	};
	auto root = loadPfb->sceneFile->get_root_entity();
	check_this(root);

	root->unique_file_id = 1;
	auto serialized = serialize_entities_to_text("", { root }, nullptr);
	{
		auto unseri = unserialize_entities_from_text("", serialized.text, g_assets.loader, nullptr);
		check_this(unseri.get_root_entity());
	}
}

// Tests saving a prefab, then loading it again
ADD_TEST(load_unload_prefab)
{
	SerializeTestWorkbench workbench;


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
		MakePathForObjectNew pathmaker(nullptr);
		NewSerialization::add_objects_to_container("debug_tag", { root }, container, prefab, out);
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