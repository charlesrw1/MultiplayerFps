#include "SerializationAPI.h"
#include "Test/Test.h"
#include "Game/Entity.h"
#include "Game/EntityComponent.h"
#include "Level.h"
#include "Test/Test.h"

#include "Game/LevelAssets.h"
#include "Assets/AssetDatabase.h"
#include "Game/Entities/Player.h"

// tests
// * serialize scene
// 
// * serialize prefab
// * serialize scene with prefab
// * serialize scene with 2 prefabs
// * serialize prefab with prefab
// * serialize <new> prefab
// * serialize <existing> prefab

class SerializeTestWorkbench
{
public:
	~SerializeTestWorkbench() {
		//
		for (auto e : all) {
			e->init_state = BaseUpdater::initialization_state::CONSTRUCTOR;
			if (auto ent = e->cast_to<Entity>())
				ent->all_components.clear();
			delete e;
		}
	}

	template<typename T>
	T* add_entity() {
		Entity* e = (Entity*)T::StaticType.allocate();
		e->unique_file_id = ++file_id;
		post_unserialization_R(e);
		return (T*)e;
	}

	template<typename T>
	T* add_component(Entity* e) {
		EntityComponent* ec = (EntityComponent*)T::StaticType.allocate();
		ec->unique_file_id = ++file_id;
		e->add_component_from_unserialization(ec);
		all.insert(ec);
		return (T*)ec;
	}

	Entity* add_prefab(PrefabAsset* asset) {
		auto unserialized_scene = unserialize_entities_from_text(asset->text,asset);
		unserialized_scene.get_root_entity()->is_root_of_prefab = true;
		unserialized_scene.get_root_entity()->unique_file_id = ++file_id;
		post_unserialization_R(unserialized_scene.get_root_entity());
		return unserialized_scene.get_root_entity();
	}

	PrefabAsset* create_prefab(Entity* root) {
		PrefabAsset* pa = new PrefabAsset;
		prefabs.insert(pa);
		root->is_root_of_prefab = true;
		root->what_prefab = pa;
		return pa;
	}

	void post_unserialization_R(Entity* e)
	{
		all.insert(e);

		for (int i = 0; i < e->all_components.size(); i++) {
			auto& c = e->all_components[i];
			ASSERT(c->get_instance_id() == 0);
			all.insert(c);
		}

		for (auto child : e->get_children())
			post_unserialization_R(child);
	}
	void post_unserialization() {
		for(auto a : all)
			a->post_unserialization(get_next_id_and_increment());
	}

	uint32_t file_id = 0;
	uint64_t handle_start = 0;
	std::unordered_set<BaseUpdater*> all;
	std::unordered_set<PrefabAsset*> prefabs;

	std::vector<Entity*> get_all_entities() {
		std::vector<Entity*> out;
		for (auto e : all) {
			if (e->is_a<Entity>())
				out.push_back((Entity*)e);
		}
		return out;
	}

	uint64_t get_next_id_and_increment()
	{
		return ++handle_start;
	}
};
extern std::string build_path_for_object(const BaseUpdater* obj, const PrefabAsset* for_prefab);


ADD_TEST(Serialization, BuildPath)
{
	SerializeTestWorkbench work;
	auto e = work.add_entity<Entity>();

	PrefabAsset* loadPrefab = g_assets.find_sync<PrefabAsset>("test2.pfb").get();
	auto pent = work.add_prefab(loadPrefab);

	work.post_unserialization();
	// test basic path
	TEST_TRUE(build_path_for_object(e, nullptr) == std::to_string(e->unique_file_id));
	auto e2 = work.add_entity<Entity>();
	e2->parent_to(e);
	// test parented path
	TEST_TRUE(build_path_for_object(e2, nullptr) == std::to_string(e2->unique_file_id));
	
	TEST_TRUE(build_path_for_object(pent, nullptr) == std::to_string(pent->unique_file_id));

}

#include "Framework/Files.h"

std::string get_text_of_file(const char* path)
{
	auto file = FileSys::open_read_engine(path);
	std::string str(file->size(), ' ');
	file->read(&str[0], str.size());
	return str;
}


ADD_TEST(Serialization, UnserializePrefab)
{
	// prefab unserialization
	auto text = get_text_of_file("TestFiles/test2.pfb");
	PrefabAsset temp;
	auto unserialized = unserialize_entities_from_text(text, &temp);
	auto root = unserialized.get_root_entity();
	TEST_TRUE(root);
	TEST_TRUE(!root->creator_source && root->is_root_of_prefab && root->what_prefab == &temp);
	TEST_TRUE(unserialized.find("/"));
	TEST_TRUE(unserialized.find("2"));
	TEST_TRUE(unserialized.find("2/~2122221332"));
	TEST_TRUE(unserialized.find("3"));
	TEST_TRUE(unserialized.find("3/~2122221332"));
}

extern bool this_is_newly_created(const BaseUpdater* b, const PrefabAsset* for_prefab);
ADD_TEST(Serialization, ThisIsNewlyCreated)
{
	SerializeTestWorkbench work;
	PrefabAsset* loadPrefab = g_assets.find_sync<PrefabAsset>("test4.pfb").get();
	auto pent = work.add_prefab(loadPrefab);
	auto subent = pent->get_children().at(0);
	auto newent = work.add_entity<Entity>();
	auto newcomp = work.add_component<MeshComponent>(newent);
	pent->parent_to(work.add_entity<Entity>());
	work.post_unserialization();

	TEST_TRUE(this_is_newly_created(pent, nullptr));
	TEST_TRUE(this_is_newly_created(newent, nullptr));
	TEST_TRUE(this_is_newly_created(newcomp, nullptr));

	TEST_TRUE(!this_is_newly_created(subent, nullptr));
	TEST_TRUE(this_is_newly_created(subent, loadPrefab));
}

ADD_TEST(Serialization, RelativePaths)
{
	TEST_TRUE(serialize_build_relative_path("1/2", "1") == "..");
	TEST_TRUE(serialize_build_relative_path("1", "1/2") == "2");
	TEST_TRUE(serialize_build_relative_path("1/2/3", "1/3") == "../../3");
	TEST_TRUE(serialize_build_relative_path("1/2/3", "2/~3") == "../../../2/~3");

	TEST_TRUE(unserialize_relative_to_absolute("..", "1/2") == "1");
	TEST_TRUE(unserialize_relative_to_absolute("2", "1") == "1/2");
	TEST_TRUE(unserialize_relative_to_absolute("../../3","1/2/3")== "1/3");
	TEST_TRUE(unserialize_relative_to_absolute("../../../2/~3", "1/2/3") == "2/~3");

	SerializeTestWorkbench work;
	PrefabAsset* loadPrefab = g_assets.find_sync<PrefabAsset>("test4.pfb").get();
	auto pent = work.add_prefab(loadPrefab);
	auto subent = pent->get_children().at(0);
	auto newent = work.add_entity<Entity>();
	auto newcomp = work.add_component<MeshComponent>(newent);
	pent->parent_to(work.add_entity<Entity>());
	work.post_unserialization();

	auto from = build_path_for_object((BaseUpdater*)subent,nullptr);
	auto to = build_path_for_object((BaseUpdater*)pent,nullptr);
	auto rel = serialize_build_relative_path(from.c_str(),to.c_str());
	auto abs = unserialize_relative_to_absolute(rel.c_str(), from.c_str());

	TEST_TRUE(build_path_for_object(pent, nullptr) == std::to_string(pent->unique_file_id));
	TEST_TRUE(build_path_for_object(subent, nullptr) == std::to_string(pent->unique_file_id) +"/" + std::to_string(subent->unique_file_id));

}


extern const ClassBase* find_diff_class(const BaseUpdater* obj, PrefabAsset* for_prefab, PrefabAsset*& diff_prefab);
ADD_TEST(Serialization, PrefabInPrefabSerialize)
{
	SerializeTestWorkbench work;
	PrefabAsset* loadPrefab = g_assets.find_sync<PrefabAsset>("test4.pfb").get();
	auto pent = work.add_prefab(loadPrefab);
	auto subent = pent->get_children().at(0);
	work.post_unserialization();

	TEST_TRUE(subent->what_prefab && subent->what_prefab->get_name() == "test2.pfb");
	PrefabAsset* diff_prefab = nullptr;
	TEST_TRUE(find_diff_class(pent, nullptr, diff_prefab) == loadPrefab->sceneFile->get_root_entity());
	TEST_TRUE(diff_prefab == loadPrefab);
	diff_prefab = nullptr;
	TEST_TRUE(find_diff_class(subent, nullptr, diff_prefab) == loadPrefab->sceneFile->get_root_entity()->get_children().at(0));
	TEST_TRUE(diff_prefab == loadPrefab);
	 


	auto file = serialize_entities_to_text(work.get_all_entities(), nullptr);
}

ADD_TEST(Serialization, WritePrefab)
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

	TEST_TRUE(build_path_for_object(pent, prefab) == std::to_string(pent->unique_file_id));
	auto child_ent = pent->get_children()[0];
	TEST_TRUE(build_path_for_object(child_ent, prefab) == std::to_string(pent->unique_file_id) + "/" + std::to_string(child_ent->unique_file_id));




	auto unserialized = unserialize_entities_from_text(file.text, prefab);

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

	TEST_TRUE(good && "creator source not set");
	auto path = build_path_for_object(pent, prefab);
	TEST_TRUE(path == std::to_string(pent->unique_file_id));
	auto find = unserialized.find(path);
	TEST_TRUE(find&&find->cast_to<Entity>()->get_children().size()==loadPrefab->sceneFile->get_root_entity()->get_children().size());



	//TEST_TRUE(unserialized.get_root_entity());
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


ADD_TEST(Serialization, FindDiffInPrefab)
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
	auto diff = find_diff_class(root, prefab,diff_prefab);
	TEST_TRUE(diff == Entity::StaticType.default_class_object);
	diff = find_diff_class(comp,prefab,diff_prefab);
	TEST_TRUE(diff == PointLightComponent::StaticType.default_class_object);
	
	diff = find_diff_class(pent, prefab,diff_prefab);
	TEST_TRUE(diff == loadPrefab->sceneFile->get_root_entity());
}

ADD_TEST(Serialization, FindDiffInScene)
{
	SerializeTestWorkbench work;
	auto ent1 = work.add_entity<Entity>();
	auto ent2 = work.add_entity<Entity>();
	ent2->parent_to(ent1);
	auto comp = work.add_component<PointLightComponent>(ent1);

	PrefabAsset* loadPrefab = g_assets.find_sync<PrefabAsset>("test2.pfb").get();
	auto pent = work.add_prefab(loadPrefab);

	PrefabAsset* diff_prefab = nullptr;
	auto diff = find_diff_class(ent1, nullptr,diff_prefab);
	TEST_TRUE(diff == Entity::StaticType.default_class_object);
	diff = find_diff_class(comp,nullptr,diff_prefab);
	TEST_TRUE(diff == PointLightComponent::StaticType.default_class_object);
	
	diff = find_diff_class(pent, nullptr,diff_prefab);
	TEST_TRUE(diff == loadPrefab->sceneFile->get_root_entity());
}


ADD_TEST(Serialization, Scene)
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

	auto scene2 = unserialize_entities_from_text(file.text);
	TEST_TRUE(scene2.get_objects().size() == work.all.size());
	auto path = build_path_for_object(e, nullptr);
	TEST_TRUE(scene2.find(path)->cast_to<Entity>());
	path = build_path_for_object(another_e, nullptr);
	TEST_TRUE(scene2.find(path)->cast_to<Entity>()->get_parent());
}