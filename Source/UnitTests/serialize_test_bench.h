#include "Game/Entity.h"
#include "Game/EntityComponent.h"
#include "Game/LevelAssets.h"
#include "LevelSerialization/SerializationAPI.h"
#include "Framework/MapUtil.h"

using std::string;
using std::unordered_map;
using std::vector;
using std::unordered_set;




class SerializeTestUtil
{
public:
	static bool do_properties_equal(ClassBase* base1, ClassBase* base2);
};

class SerializeTestWorkbench
{
public:
	~SerializeTestWorkbench();

	template<typename T>
	T* add_component(Entity* e) {
		Component* ec = (Component*)T::StaticType.allocate();
		ec->unique_file_id = ++file_id;
		e->add_component_from_unserialization(ec);
		all.insert(ec);
		return (T*)ec;
	}

	Entity* add_entity(Entity* parent=nullptr);
	Entity* add_prefab(PrefabAsset* asset);
	PrefabAsset* create_prefab(Entity* root, string name);
	PrefabAsset* load_prefab(string s);
	void post_unserialization_R(Entity* e);
	void post_unserialization();
	vector<Entity*> get_all_entities();
	uint64_t get_next_id_and_increment();

	uint32_t file_id = 0;
	uint64_t handle_start = 0;
	unordered_set<BaseUpdater*> all;
};
