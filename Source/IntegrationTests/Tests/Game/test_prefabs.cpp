// Source/IntegrationTests/Tests/Game/test_prefabs.cpp
#include "IntegrationTests/TestContext.h"
#include "IntegrationTests/TestRegistry.h"
#include "GameEnginePublic.h"
#include "Game/Prefab.h"
#include "Game/Components/MeshComponent.h"
#include "LevelSerialization/SerializeNew.h"
#include "Assets/AssetDatabase.h"

// Test saving and loading a prefab with multiple entities
static TestTask test_prefab_save_and_load(TestContext& t) {
	// Create a temporary prefab path
	const std::string temp_prefab_path = "TestFiles/test_prefab_save_load.tprefab";

	// Create 2 entities with MeshComponent
	Entity* e1 = eng->create_entity();
	e1->set_editor_name("Test Entity 1");
	e1->set_ls_position({1.0f, 2.0f, 3.0f});

	Entity* e2 = eng->create_entity();
	e2->set_editor_name("Test Entity 2");
	e2->set_ls_position({4.0f, 5.0f, 6.0f});

	// Serialize to text
	std::vector<Entity*> entities = {e1, e2};
	SerializedSceneFile serialized;
	try {
		serialized = NewSerialization::serialize_to_text("test_prefab", entities, false);
	} catch (const std::exception& e) {
		t.require(false, "Failed to serialize entities");
		co_return;
	}

	// Save to prefab file
	bool save_ok = PrefabFile::save_text(temp_prefab_path, serialized.text);
	t.require(save_ok, "Failed to save prefab file");

	// Load prefab file
	std::string loaded_text = PrefabFile::load_text(temp_prefab_path);
	t.require(!loaded_text.empty(), "Loaded prefab text is empty");
	t.check(loaded_text == serialized.text, "Loaded text matches saved text");

	// Deserialize back
	try {
		UnserializedSceneFile unserialized = unserialize_entities_from_text("test_prefab_load", loaded_text,
			AssetDatabase::loader, false);

		// Check entity count
		int entity_count = 0;
		for (auto base : unserialized.all_obj_vec) {
			if (dynamic_cast<Entity*>(base)) {
				entity_count++;
			}
		}
		t.check(entity_count == 2, "Deserialized prefab contains 2 entities");

		// Cleanup
		e1->destroy();
		e2->destroy();
		unserialized.delete_objs();
	} catch (const std::exception& e) {
		t.require(false, "Failed to deserialize prefab");
		co_return;
	}

	co_await t.wait_ticks(1);
}
GAME_TEST("prefab/save_and_load", 10.f, test_prefab_save_and_load);

// Test roundtrip: entity properties survive serialization
static TestTask test_prefab_roundtrip_properties(TestContext& t) {
	const std::string temp_prefab_path = "TestFiles/test_prefab_roundtrip.tprefab";

	// Create entity with known properties
	Entity* original = eng->create_entity();
	original->set_editor_name("MyTestEntity");
	original->set_ls_position({10.0f, 20.0f, 30.0f});
	original->set_ls_rotation(glm::quat(glm::radians(glm::vec3(45.0f, 0.0f, 0.0f))));

	// Serialize, save, load, deserialize
	SerializedSceneFile serialized;
	try {
		serialized = NewSerialization::serialize_to_text("test_roundtrip", {original}, false);
	} catch (const std::exception& e) {
		t.require(false, "Failed to serialize");
		co_return;
	}

	bool save_ok = PrefabFile::save_text(temp_prefab_path, serialized.text);
	t.require(save_ok, "Failed to save");

	std::string loaded_text = PrefabFile::load_text(temp_prefab_path);
	try {
		UnserializedSceneFile unserialized = unserialize_entities_from_text("test_roundtrip_load", loaded_text,
			AssetDatabase::loader, false);

		// Check properties
		Entity* restored = nullptr;
		for (auto base : unserialized.all_obj_vec) {
			if (auto e = dynamic_cast<Entity*>(base)) {
				restored = e;
				break;
			}
		}

		t.require(restored != nullptr, "Deserialized entity found");

		glm::vec3 pos = restored->get_ls_position();
		t.check(pos.x > 9.9f && pos.x < 10.1f, "Position X preserved");
		t.check(pos.y > 19.9f && pos.y < 20.1f, "Position Y preserved");
		t.check(pos.z > 29.9f && pos.z < 30.1f, "Position Z preserved");

		// Cleanup
		original->destroy();
		unserialized.delete_objs();
	} catch (const std::exception& e) {
		t.require(false, "Failed to deserialize");
		co_return;
	}

	co_await t.wait_ticks(1);
}
GAME_TEST("prefab/roundtrip_properties", 10.f, test_prefab_roundtrip_properties);

// Test that PrefabAssetComponent can load a prefab at runtime
static TestTask test_prefab_asset_component(TestContext& t) {
	const std::string temp_prefab_path = "TestFiles/test_prefab_component.tprefab";

	// Create and save a prefab with one entity
	Entity* prefab_entity = eng->create_entity();
	prefab_entity->set_editor_name("Prefab Content");
	prefab_entity->set_ls_position({5.0f, 0.0f, 0.0f});

	SerializedSceneFile serialized;
	try {
		serialized = NewSerialization::serialize_to_text("test_component", {prefab_entity}, false);
	} catch (const std::exception& e) {
		t.require(false, "Failed to serialize prefab entity");
		co_return;
	}

	bool save_ok = PrefabFile::save_text(temp_prefab_path, serialized.text);
	t.require(save_ok, "Failed to save prefab");

	// Create a parent entity with PrefabAssetComponent
	Entity* parent = eng->create_entity();
	parent->set_editor_name("Parent with Prefab");

	auto prefab_comp = parent->create_component<PrefabAssetComponent>();
	t.require(prefab_comp != nullptr, "Created PrefabAssetComponent");

	prefab_comp->prefab_path = temp_prefab_path;

	// Tick to let the component start and load the prefab
	co_await t.wait_ticks(2);

	// Check that children were spawned
	const auto& children = parent->get_children();
	t.check(children.size() > 0, "Parent has spawned children from prefab");

	// Cleanup
	parent->destroy();
	prefab_entity->destroy();

	co_await t.wait_ticks(1);
}
GAME_TEST("prefab/asset_component", 10.f, test_prefab_asset_component);
