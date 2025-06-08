#include "Game/Entity.h"
#include "Game/EntityComponent.h"
#include "Game/LevelAssets.h"

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
		Component* ec = (Component*)T::StaticType.allocate();
		ec->unique_file_id = ++file_id;
		e->add_component_from_unserialization(ec);
		all.insert(ec);
		return (T*)ec;
	}

	Entity* add_prefab(PrefabAsset* asset) {
		auto unserialized_scene = unserialize_entities_from_text(asset->text, nullptr, asset);
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
		for (auto a : all)
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