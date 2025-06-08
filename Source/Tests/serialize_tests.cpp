#include "Unittest.h"
#undef ADD_TEST
#define ADD_TEST(x) void test##x()

#include "LevelSerialization/SerializationAPI.h"

#include "Game/Entity.h"
#include "Game/EntityComponent.h"
#include "Level.h"

#include "Game/LevelAssets.h"
#include "Assets/AssetDatabase.h"
#include "Game/Entities/Player.h"
#include "serialize_test_bench.h"

// tests
// * serialize scene
// 
// * serialize prefab
// * serialize scene with prefab
// * serialize scene with 2 prefabs
// * serialize prefab with prefab
// * serialize <new> prefab
// * serialize <existing> prefab

extern std::string build_path_for_object(const BaseUpdater* obj, const PrefabAsset* for_prefab);


ADD_TEST(Serialization, BuildPath)
{
	SerializeTestWorkbench work;
	auto e = work.add_entity<Entity>();

	PrefabAsset* loadPrefab = g_assets.find_sync<PrefabAsset>("test2.pfb").get();
	auto pent = work.add_prefab(loadPrefab);

	work.post_unserialization();
	// test basic path
	checkTrue(build_path_for_object(e, nullptr) == std::to_string(e->unique_file_id));
	auto e2 = work.add_entity<Entity>();
	e2->parent_to(e);
	// test parented path
	checkTrue(build_path_for_object(e2, nullptr) == std::to_string(e2->unique_file_id));

	checkTrue(build_path_for_object(pent, nullptr) == std::to_string(pent->unique_file_id));

}


ADD_TEST(unserialize_prefab)
{
	// prefab unserialization
	auto text = UnitTestUtil::get_text_of_file("TestFiles/test2.pfb");
	PrefabAsset temp;
	auto unserialized = unserialize_entities_from_text(text, nullptr, &temp);
	auto root = unserialized.get_root_entity();
	checkTrue(root);
	checkTrue(!root->creator_source && root->is_root_of_prefab && root->what_prefab == &temp);
	checkTrue(unserialized.find("/"));
	checkTrue(unserialized.find("2"));
	checkTrue(unserialized.find("2/~2122221332"));
	checkTrue(unserialized.find("3"));
	checkTrue(unserialized.find("3/~2122221332"));
}

extern bool this_is_newly_created(const BaseUpdater* b, const PrefabAsset* for_prefab);
ADD_TEST(serialize_newly_created)
{
	SerializeTestWorkbench work;
	PrefabAsset* loadPrefab = g_assets.find_sync<PrefabAsset>("test4.pfb").get();
	auto pent = work.add_prefab(loadPrefab);
	auto subent = pent->get_children().at(0);
	auto newent = work.add_entity<Entity>();
	auto newcomp = work.add_component<MeshComponent>(newent);
	pent->parent_to(work.add_entity<Entity>());
	work.post_unserialization();

	checkTrue(this_is_newly_created(pent, nullptr));
	checkTrue(this_is_newly_created(newent, nullptr));
	checkTrue(this_is_newly_created(newcomp, nullptr));

	checkTrue(!this_is_newly_created(subent, nullptr));
	checkTrue(this_is_newly_created(subent, loadPrefab));
}

ADD_TEST(serialize_relative_paths)
{
	checkTrue(serialize_build_relative_path("1/2", "1") == "..");
	checkTrue(serialize_build_relative_path("1", "1/2") == "2");
	checkTrue(serialize_build_relative_path("1/2/3", "1/3") == "../../3");
	checkTrue(serialize_build_relative_path("1/2/3", "2/~3") == "../../../2/~3");

	checkTrue(unserialize_relative_to_absolute("..", "1/2") == "1");
	checkTrue(unserialize_relative_to_absolute("2", "1") == "1/2");
	checkTrue(unserialize_relative_to_absolute("../../3", "1/2/3") == "1/3");
	checkTrue(unserialize_relative_to_absolute("../../../2/~3", "1/2/3") == "2/~3");

	SerializeTestWorkbench work;
	PrefabAsset* loadPrefab = g_assets.find_sync<PrefabAsset>("test4.pfb").get();
	auto pent = work.add_prefab(loadPrefab);
	auto subent = pent->get_children().at(0);
	auto newent = work.add_entity<Entity>();
	auto newcomp = work.add_component<MeshComponent>(newent);
	pent->parent_to(work.add_entity<Entity>());
	work.post_unserialization();

	auto from = build_path_for_object((BaseUpdater*)subent, nullptr);
	auto to = build_path_for_object((BaseUpdater*)pent, nullptr);
	auto rel = serialize_build_relative_path(from.c_str(), to.c_str());
	auto abs = unserialize_relative_to_absolute(rel.c_str(), from.c_str());

	checkTrue(build_path_for_object(pent, nullptr) == std::to_string(pent->unique_file_id));
	checkTrue(build_path_for_object(subent, nullptr) == std::to_string(pent->unique_file_id) + "/" + std::to_string(subent->unique_file_id));

}


extern const ClassBase* find_diff_class(const BaseUpdater* obj, PrefabAsset* for_prefab, PrefabAsset*& diff_prefab);
ADD_TEST(serialize_nested_prefabs)
{
	SerializeTestWorkbench work;
	PrefabAsset* loadPrefab = g_assets.find_sync<PrefabAsset>("test4.pfb").get();
	auto pent = work.add_prefab(loadPrefab);
	auto subent = pent->get_children().at(0);
	work.post_unserialization();

	checkTrue(subent->what_prefab && subent->what_prefab->get_name() == "test2.pfb");
	PrefabAsset* diff_prefab = nullptr;
	checkTrue(find_diff_class(pent, nullptr, diff_prefab) == loadPrefab->sceneFile->get_root_entity());
	checkTrue(diff_prefab == loadPrefab);
	diff_prefab = nullptr;
	checkTrue(find_diff_class(subent, nullptr, diff_prefab) == loadPrefab->sceneFile->get_root_entity()->get_children().at(0));
	checkTrue(diff_prefab == loadPrefab);



	auto file = serialize_entities_to_text(work.get_all_entities(), nullptr);
}

ADD_TEST(serialize_write_prefab)
{
	SerializeTestWorkbench work;
	auto root = work.add_entity<Entity>();
	auto prefab = work.create_prefab(root);
	auto ent2 = work.add_entity<Entity>();
	ent2->parent_to(root);
	auto comp = work.add_component<PointLightComponent>(root);


	PrefabAsset* loadPrefab = g_assets.find_sync<PrefabAsset>("test2.pfb").get();
	auto pent = work.add_prefab(loadPrefab);
	pent->parent_to(root);

	auto file = serialize_entities_to_text({ root }, prefab);

	checkTrue(build_path_for_object(pent, prefab) == std::to_string(pent->unique_file_id));
	auto child_ent = pent->get_children()[0];
	checkTrue(build_path_for_object(child_ent, prefab) == std::to_string(pent->unique_file_id) + "/" + std::to_string(child_ent->unique_file_id));




	auto unserialized = unserialize_entities_from_text(file.text, nullptr, prefab);

	bool good = true;
	for (auto obj : unserialized.get_objects()) {
		auto e = obj.second->cast_to<Entity>();
		if (e && !e->get_parent()) continue;
		if (!obj.second->creator_source || (obj.second->what_prefab != prefab && obj.second->what_prefab != loadPrefab))
		{
			good = false;
			break;
		}
	}

	checkTrue(good && "creator source not set");
	auto path = build_path_for_object(pent, prefab);
	checkTrue(path == std::to_string(pent->unique_file_id));
	auto find = unserialized.find(path);
	checkTrue(find && find->cast_to<Entity>()->get_children().size() == loadPrefab->sceneFile->get_root_entity()->get_children().size());



	//checkTrue(unserialized.get_root_entity());
}

extern bool this_is_newly_created(const BaseUpdater* b, const PrefabAsset* for_prefab);

// testing:
// bad paths
// multiple ids
// bad classnames
// variable not found, list not found
// test that skip object works right
// prefabs:
// prefab diffing
// bad input to serialize()
//		circular parents
//		prefab multiple roots
//		


ADD_TEST(serialize_find_diff)
{
	SerializeTestWorkbench work;
	auto root = work.add_entity<Entity>();
	auto prefab = work.create_prefab(root);
	auto ent2 = work.add_entity<Entity>();
	ent2->parent_to(root);
	auto comp = work.add_component<PointLightComponent>(root);


	PrefabAsset* loadPrefab = g_assets.find_sync<PrefabAsset>("test2.pfb").get();
	auto pent = work.add_prefab(loadPrefab);
	pent->parent_to(root);

	PrefabAsset* diff_prefab = nullptr;
	auto diff = find_diff_class(root, prefab, diff_prefab);
	checkTrue(diff == Entity::StaticType.default_class_object);
	diff = find_diff_class(comp, prefab, diff_prefab);
	checkTrue(diff == PointLightComponent::StaticType.default_class_object);

	diff = find_diff_class(pent, prefab, diff_prefab);
	checkTrue(diff == loadPrefab->sceneFile->get_root_entity());
}

ADD_TEST(serialize_diff_in_scene)
{
	SerializeTestWorkbench work;
	auto ent1 = work.add_entity<Entity>();
	auto ent2 = work.add_entity<Entity>();
	ent2->parent_to(ent1);
	auto comp = work.add_component<PointLightComponent>(ent1);

	PrefabAsset* loadPrefab = g_assets.find_sync<PrefabAsset>("test2.pfb").get();
	auto pent = work.add_prefab(loadPrefab);

	PrefabAsset* diff_prefab = nullptr;
	auto diff = find_diff_class(ent1, nullptr, diff_prefab);
	checkTrue(diff == Entity::StaticType.default_class_object);
	diff = find_diff_class(comp, nullptr, diff_prefab);
	checkTrue(diff == PointLightComponent::StaticType.default_class_object);

	diff = find_diff_class(pent, nullptr, diff_prefab);
	checkTrue(diff == loadPrefab->sceneFile->get_root_entity());
}


ADD_TEST(serialize_basic)
{
	SerializeTestWorkbench work;
	auto e = work.add_entity<Entity>();
	auto light = work.add_component<PointLightComponent>(e);
	auto sm = work.add_entity<Entity>();
	auto another_e = work.add_entity<Entity>();
	work.add_component<MeshComponent>(another_e);
	another_e->parent_to(e);
	work.post_unserialization();
	auto ents = work.get_all_entities();
	auto file = serialize_entities_to_text(ents);

	auto scene2 = unserialize_entities_from_text(file.text, nullptr, nullptr);
	checkTrue(scene2.get_objects().size() == work.all.size());
	auto path = build_path_for_object(e, nullptr);
	checkTrue(scene2.find(path)->cast_to<Entity>());
	path = build_path_for_object(another_e, nullptr);
	checkTrue(scene2.find(path)->cast_to<Entity>()->get_parent());
}