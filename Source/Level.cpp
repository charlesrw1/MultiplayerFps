#include "Level.h"

#include "Framework/Files.h"
#include "AssetCompile/Someutils.h"
#include "Assets/AssetDatabase.h"
#include "Game/LevelAssets.h"
#include "Game/Entity.h"
#include "Game/EntityComponent.h"
#include "Framework/Config.h"
#include "LevelSerialization/SerializationAPI.h"
#include "GameEnginePublic.h"
#include "tracy/public/tracy/Tracy.hpp"

Level::~Level()
{
	assert(all_world_ents.num_used == 0);
}

void Level::update_level()
{
	{
		BooleanScope scope(b_is_in_update_tick);

		for (auto updater : tick_list)
			updater->update();
	}

	for (auto want : wantsToAddToUpdate) {
		if (want)
			tick_list.insert(want);
	}
	wantsToAddToUpdate.clear();

	for (auto h : deferred_delete_list) {
		auto e = get_entity(h);
		if (!e) 
			continue;
		if (e->is_a<Entity>()) {
			auto ent = (Entity*)e;
			ent->destroy();
		}
		else if (e->is_a<Component>()) {
			auto ent = (Component*)e;
			ent->destroy();
		}
	}
	deferred_delete_list.clear();
}
void Level::sync_level_render_data()
{
	ZoneScoped;
	for (auto ec : wants_sync_update)
		ec->on_sync_render_data();
	wants_sync_update.clear_all();
}
void Level::add_to_sync_render_data_list(Component* ec)
{
	if (eng->get_is_in_overlapped_period())
		wants_sync_update.insert(ec);
	else
		ec->on_sync_render_data();
}

void Level::add_to_update_list(Component* ec) {
	if (b_is_in_update_tick.get_value())
		wantsToAddToUpdate.push_back(ec);
	else
		tick_list.insert(ec);
}

Entity* Level::spawn_entity_class_deferred_internal(const ClassTypeInfo& ti)
{
	ASSERT(ti.allocate);
	ClassBase* e = ti.allocate();	// allocate + call constructor
	ASSERT(e);

	Entity* ec = nullptr;

	ec = e->cast_to<Entity>();
	if (!ec) {
		sys_print(Error, "spawn_entity_class_deferred_internal failed for %s\n", ti.classname);
		delete e;
		return nullptr;
	}

	insert_new_native_entity_into_hashmap_R(ec);	// insert into hashmap but DONT call initialize, that is done by the RAII DeferredSpawnScope

	return ec;
}

ConfigVar log_destroy_game_objects("log_destroy_game_objects", "1", CVAR_BOOL, "");

Entity* Level::spawn_entity()
{
	auto& ti = Entity::StaticType;
	ASSERT(ti.allocate);

	ClassBase* e = ti.allocate();	// allocate + call constructor
	ASSERT(e);

	auto ent = (Entity*)e;


	// call_startup_functions_for_new_entity
	insert_new_native_entity_into_hashmap_R(ent);
	initialize_new_entity_safe(ent);

	return ent;
}

void Level::destroy_entity(Entity* e)
{
	if (!e) 
		return;
	uint64_t id = e->get_instance_id();
	assert(id != 0);
	if(log_destroy_game_objects.get_bool())
		sys_print(Debug, "removing entity (handle:%llu,class:%s)\n", id, e->get_type().classname);

	e->destroy_internal();
	delete e;
	// remove from hashmap
	#ifdef _DEBUG
		auto ent = all_world_ents.find(id);
		if (!ent) {
			sys_print(Warning,"destroy_entity: entity does not exist in hashmap, double delete?\n");
		}
	#endif // _DEBUG
	all_world_ents.remove(id);
}
void Level::destroy_component(Component* ec)
{
	if (!ec) 
		return;
	wants_sync_update.remove(ec);

	uint64_t id = ec->get_instance_id();
	assert(id != 0);
	if (log_destroy_game_objects.get_bool())
		sys_print(Debug,"removing eComponent (handle:%llu,class:%s)\n", id, ec->get_type().classname);
	ec->destroy_internal();
	delete ec;
	// remove from hashmap
	#ifdef _DEBUG
		auto ent = all_world_ents.find(id);
		if (!ent) {
			sys_print(Warning,"destroy_component: entity does not exist in hashmap, double delete?\n");
		}
	#endif // _DEBUG
	all_world_ents.remove(id);
}

Level::Level(bool is_editor) 
	: all_world_ents(4/*2^4*/), tick_list(4), wants_sync_update(4)
{
	b_is_editor_level = is_editor;
}
void Level::start(SceneAsset* source)
{
	if (source)
		sourceAssetName = source->get_name();
	else
		sourceAssetName = "<unnamed level>";
	if (source && source->sceneFile) {
		insert_unserialized_entities_into_level(*source->sceneFile);
		source->sceneFile.reset();
	}
		
}

void Level::remove_from_update_list(Component* ec) {
	tick_list.remove(ec);
	for (int i = 0; i < wantsToAddToUpdate.size(); i++)
		if (wantsToAddToUpdate[i] == ec) {
			wantsToAddToUpdate[i] = nullptr;
		}
}

void add_entities_and_components_to_init_R(Entity* e, InlineVec<Entity*, 4>& es, InlineVec<Component*, 16>& cs)
{
	es.push_back(e);
	for (auto c : e->get_components())
		cs.push_back(c);
	for (auto child : e->get_children())
		add_entities_and_components_to_init_R(child, es, cs);
}

void Level::initialize_new_entity_safe(Entity* e)
{
	ASSERT(e);
	ASSERT(e->init_state == BaseUpdater::initialization_state::HAS_ID);

	// do it this way so initialization can add components/entities and not mess up
	// todo: make sure that they arent deleted by init (deferred delete?)
	InlineVec<Entity*, 4> init_entities;
	InlineVec<Component*, 16> init_components;
	add_entities_and_components_to_init_R(e, init_entities, init_components);
	
	for (int i = 0; i < init_entities.size();i++) {
		auto e = init_entities[i];
		ASSERT(e->init_state == BaseUpdater::initialization_state::HAS_ID);
		e->initialize_internal();	// just sets init_state => CALLED_START
	}
	for (int i = 0; i < init_components.size();i++) {
		auto ec = init_components[i];
		ASSERT(ec->init_state == BaseUpdater::initialization_state::HAS_ID);
		ec->initialize_internal_step1();
	}
	for (int i = 0; i < init_components.size();i++) {
		auto ec = init_components[i];
		ASSERT(ec->init_state == BaseUpdater::initialization_state::CALLED_PRE_START);
		ec->initialize_internal_step2();
	}
}

void Level::insert_new_native_entity_into_hashmap_R(Entity* e) {
	ASSERT(e);
	ASSERT(e->init_state == BaseUpdater::initialization_state::CONSTRUCTOR);
	ASSERT(e->get_instance_id()==0);

	e->post_unserialization(get_next_id_and_increment());

	ASSERT(all_world_ents.find(e->get_instance_id()) == nullptr);
	ASSERT(e->get_instance_id()!=0);

	all_world_ents.insert(e->get_instance_id(), e);

	for (int i = 0; i < e->all_components.size(); i++) {
		auto& c = e->all_components[i];
		ASSERT(c->get_instance_id() == 0);
		c->post_unserialization(get_next_id_and_increment());
		ASSERT(all_world_ents.find(c->get_instance_id()) == nullptr);
		all_world_ents.insert(c->get_instance_id(), c);
	}

	for (auto child : e->get_children())
		insert_new_native_entity_into_hashmap_R(child);
}

void Level::close_level()
{
	const bool is_this_editor_level = is_editor_level();	
	for (auto ent : all_world_ents) {
		if (Entity* e = ent->cast_to<Entity>())
			e->destroy();
	}
	ASSERT(all_world_ents.num_used == 0);
	all_world_ents.clear_all();
}
#include "Framework/Log.h"
#include "Framework/MapUtil.h"
void Level::insert_unserialized_entities_into_level(UnserializedSceneFile& scene, const SerializedSceneFile* reassign_ids) // was bool assign_new_ids=false
{
	const char* reassign_id_str = print_get_bool_string(reassign_ids != nullptr);
	sys_print(Debug, "Level::insert_unserialized_entities_into_level: (level=%s) (reassign_ids=%s) (objs=%d)\n", sourceAssetName.c_str(), reassign_id_str,(int)scene.all_obj_vec.size());

	auto& objs = scene.all_obj_vec;

	std::unordered_set<BaseUpdater*> ObjsTest;
	std::unordered_set<int> fileIds;
	for (auto o : objs) {
		SetUtil::insert_test_exists(ObjsTest,o);
		if (o->unique_file_id != 0)
			SetUtil::insert_test_exists(fileIds, o->unique_file_id);
	}

	if(reassign_ids) {
		std::unordered_set<uint64_t> found;
		for (auto[fileid,eng_handle] : reassign_ids->path_to_instance_handle) {
			SetUtil::insert_test_exists(found, eng_handle);
		}
		for (auto& o : objs) {
			ASSERT(o);
			assert(o->get_instance_id() == 0);
			int pathFileId = o->unique_file_id;
			uint64_t id_to_use = 0;
			if (pathFileId != 0) {
				auto idfind = reassign_ids->path_to_instance_handle.find(pathFileId);
				if (idfind != reassign_ids->path_to_instance_handle.end()) {
					id_to_use = idfind->second;

					if (id_to_use == 0) {
						sys_print(Warning, "Level::insert_unserialized_entities_into_level: reassign_ids->path_to_instance_handle returned 0 for fileid=(%lld)\n", pathFileId);
					}
				}
			}
			if(id_to_use==0) {
				//sys_print(Warning, "Level::insert_unserialized_entities_into_level: couldnt find id in reassign_ids (%lld)\n", id_to_use);
				id_to_use = get_next_id_and_increment();
			}
			ASSERT(id_to_use != 0);
			auto ent = all_world_ents.find(id_to_use);
			ASSERT(all_world_ents.find(id_to_use) == nullptr);

			o->post_unserialization(id_to_use);
			assert(o->get_instance_id() == id_to_use);
			all_world_ents.insert(o->get_instance_id(), o);
		}
	}
	else {
		for (auto& o : objs) {
			ASSERT(o);
			assert(o->get_instance_id() == 0);

			o->post_unserialization(get_next_id_and_increment());
			ASSERT(all_world_ents.find(o->get_instance_id()) == nullptr);
			assert(o->get_instance_id() != 0);
			all_world_ents.insert(o->get_instance_id(), o);
		}
	}
	
	validate();
	scene.unserialize_post_assign_ids();

	for (int i = 0; i < objs.size();i++) {
		BaseUpdater* o = objs[i];
		assert(o->get_instance_id() != 0);
		auto ent = o;
		if (Entity* e = ent->cast_to<Entity>())
			e->initialize_internal(); // just sets init_state => CALLED_START
		else if (Component* ec = ent->cast_to<Component>()) {
			if (!ec->get_owner()) {
				const char* type = ec->get_type().classname;
				sys_print(Error, "Level::insert_unserialized_entities_into_level: component witout owner (type=%s,id=%lld)\n",type,ec->get_instance_id());
				ASSERT(ec->get_instance_id() != 0);
				all_world_ents.remove(ec->get_instance_id());
				delete ec;
				objs[i] = nullptr;
			}
			else {
				ec->initialize_internal_step1();
			}
		}
		else
			ASSERT(!"Non Eentity/Component?");
	}

	for (auto& o : objs) {
		if (o) {
			assert(o->get_instance_id() != 0);
			auto ent = o;
			if (Component* ec = ent->cast_to<Component>())
				ec->initialize_internal_step2();
		}
	}
}


void Level::add_and_init_created_runtime_component(Component* c)
{
	ASSERT(c->init_state == BaseUpdater::initialization_state::CONSTRUCTOR);
	c->post_unserialization(get_next_id_and_increment());
	ASSERT(all_world_ents.find(c->get_instance_id()) == nullptr);
	all_world_ents.insert(c->get_instance_id(), c);

	c->initialize_internal_step1();
	c->initialize_internal_step2();
}


DeferredSpawnScopePrefab Level::spawn_prefab_deferred(Entity*& out, const PrefabAsset* asset)
{
	if (!asset) {
		sys_print(Warning, "Level::spawn_prefab_deferred: null prefab\n");
		return DeferredSpawnScopePrefab(nullptr);
	}
	auto unserialized_scene = asset->unserialize(nullptr);// unserialize_entities_from_text("unserialize_prefab", asset->text, nullptr);
	out = unserialized_scene.get_root_entity();
	if (!out) {
		sys_print(Warning, "Level::spawn_prefab_deferred: null root object\n");
		unserialized_scene.delete_objs();
		return DeferredSpawnScopePrefab(nullptr);
	}
	set_prefab_spawned(*out, *asset, unserialized_scene);
	return DeferredSpawnScopePrefab(new UnserializedSceneFile(std::move(unserialized_scene)));
}


Entity* Level::spawn_prefab(const PrefabAsset* asset)
{
	return spawn_prefab_shared(asset, true);
}

void Level::queue_deferred_delete(BaseUpdater* e)
{
	if (!e) 
		return;
	if (log_destroy_game_objects.get_bool()) {
		sys_print(Debug, "Level::queue_deferred_delete: (%lld)", e->get_instance_id());
	}
	deferred_delete_list.insert(e->get_instance_id());
}

Entity* Level::editor_spawn_prefab_but_dont_set_spawned_by(const PrefabAsset* asset)
{
	return spawn_prefab_shared(asset, false);
}

void Level::validate()
{
	for (auto o : all_world_ents) {
		if (auto e = o->cast_to<Entity>())
			e->validate_check();
	}
}

void Level::set_prefab_spawned(Entity& root, const PrefabAsset& asset, UnserializedSceneFile& file)
{
	for (auto& o : file.all_obj_vec) {
		if (o != &root) {
			assert(o);
			if (auto as_ent = o->cast_to<Entity>()) {
				as_ent->set_spawned_by_prefab();
			}
		}
	}
	root.set_root_object_prefab(asset);
}

Entity* Level::spawn_prefab_shared(const PrefabAsset* asset, bool set_vars)
{
	if (!asset || asset->did_load_fail()) {
		sys_print(Warning, "Level::spawn_prefab: null asset\n");
		return nullptr;
	}
	auto unserialized_scene = asset->unserialize(nullptr);// unserialize_entities_from_text("unserialize_prefab", asset->text, nullptr);
	auto root = unserialized_scene.get_root_entity();
	if (!root) {
		sys_print(Warning, "Level::spawn_prefab: root of prefab is null\n");
		unserialized_scene.delete_objs();
		return nullptr;
	}
	if(set_vars)
		set_prefab_spawned(*root, *asset, unserialized_scene);
	else {
		assert(root->get_object_prefab_spawn_type() == EntityPrefabSpawnType::None);
	}
	insert_unserialized_entities_into_level(unserialized_scene, nullptr);
	return unserialized_scene.get_root_entity();
}
