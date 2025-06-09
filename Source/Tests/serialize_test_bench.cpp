#include "serialize_test_bench.h"
#include "Assets/AssetDatabase.h"
#include "Framework/SerializerJson.h"

SerializeTestWorkbench::~SerializeTestWorkbench() {
	//
	for (auto e : all) {
		e->init_state = BaseUpdater::initialization_state::CONSTRUCTOR;
		if (auto ent = e->cast_to<Entity>())
			ent->all_components.clear();
		delete e;
	}

	g_assets.reset_testing();
}

Entity* SerializeTestWorkbench::add_entity() {
	Entity* e = (Entity*)Entity::StaticType.allocate();
	e->unique_file_id = ++file_id;
	post_unserialization_R(e);
	return e;
}

Entity* SerializeTestWorkbench::add_prefab(PrefabAsset* asset) {
	auto unserialized_scene = unserialize_entities_from_text(asset->text, nullptr, asset);
	unserialized_scene.get_root_entity()->is_root_of_prefab = true;
	unserialized_scene.get_root_entity()->unique_file_id = ++file_id;
	post_unserialization_R(unserialized_scene.get_root_entity());
	return unserialized_scene.get_root_entity();
}

PrefabAsset* SerializeTestWorkbench::create_prefab(Entity* root, string name) {
	PrefabAsset* pa = new PrefabAsset;
	root->is_root_of_prefab = true;
	root->what_prefab = pa;
	g_assets.install_system_asset(pa, name);
	return pa;
}

PrefabAsset* SerializeTestWorkbench::load_prefab(string s)
{
	return g_assets.find_sync<PrefabAsset>(s).get();
}

void SerializeTestWorkbench::post_unserialization_R(Entity* e)
{
	all.insert(e);
	e->unique_file_id = ++file_id;

	for (int i = 0; i < e->all_components.size(); i++) {
		auto& c = e->all_components[i];
		ASSERT(c->get_instance_id() == 0);
		all.insert(c);
		c->unique_file_id = ++file_id;

	}

	for (auto child : e->get_children())
		post_unserialization_R(child);
}

void SerializeTestWorkbench::post_unserialization() {
	for (auto a : all) {
		a->post_unserialization(get_next_id_and_increment());
	}
}

std::vector<Entity*> SerializeTestWorkbench::get_all_entities() {
	std::vector<Entity*> out;
	for (auto e : all) {
		if (e->is_a<Entity>())
			out.push_back((Entity*)e);
	}
	return out;
}

uint64_t SerializeTestWorkbench::get_next_id_and_increment()
{
	return ++handle_start;
}

bool SerializeTestUtil::do_properties_equal(ClassBase* base1, ClassBase* base2)
{
	if (!base1 || !base2)
		return false;
	if (&base1->get_type() != &base2->get_type())
		return false;
	MakePathForGenericObj pathmaker;
	WriteSerializerBackendJson write1(pathmaker,*base1);
	WriteSerializerBackendJson write2(pathmaker,*base2);
	if (!write1.get_root_object() || !write2.get_root_object())
		return false;
	bool is_eq = *write1.get_root_object() == *write2.get_root_object();
	return is_eq;
}
