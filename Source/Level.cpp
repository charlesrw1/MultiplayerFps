#include "Level.h"
#include "Render/Model.h"
#include "cgltf.h"
#include "glm/gtc/type_ptr.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/gtx/euler_angles.hpp"
#include "Render/Texture.h"
#include <array>
#include "Physics/Physics2.h"
#include "Framework/Files.h"
#include "AssetCompile/Someutils.h"
#include "Assets/AssetRegistry.h"

#include "Assets/AssetDatabase.h"
#include "Game/LevelAssets.h"

void Physics_Mesh::build()
{
	std::vector<Bounds> bound_vec;
	for (int i = 0; i < tris.size(); i++) {
		Physics_Triangle& tri = tris[i];
		glm::vec3 corners[3];
		for (int i = 0; i < 3; i++)
			corners[i] = verticies[tri.indicies[i]];
		Bounds b(corners[0]);
		b = bounds_union(b, corners[1]);
		b = bounds_union(b, corners[2]);
		b.bmin -= glm::vec3(0.01);
		b.bmax += glm::vec3(0.01);

		bound_vec.push_back(b);
	}

	float time_start = GetTime();
	bvh = BVH::build(bound_vec, 1, BVH_SAH);
	printf("Built bvh in %.2f seconds\n", (float)GetTime() - time_start);
}



#include "Framework/Files.h"
#include "Framework/DictWriter.h"
#include <fstream>



Level::~Level()
{
	if (!source_asset)
		printf("closing level without calling close_level\n");
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
		if (!e) continue;
		ASSERT(e->is_a<Entity>());
		auto ent = (Entity*)e;
		ent->destroy();
	}
	deferred_delete_list.clear();
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

Entity* Level::spawn_entity_from_classtype(const ClassTypeInfo& ti)
{
	ASSERT(ti.allocate);

	ClassBase* e = ti.allocate();	// allocate + call constructor
	ASSERT(e);

	Entity* ec = nullptr;

	ec = e->cast_to<Entity>();
	if (!ec) {
		sys_print(Error, "spawn_entity_from_classtype failed for %s\n", ti.classname);
		delete e;
		return nullptr;
	}

	// call_startup_functions_for_new_entity
	insert_new_native_entity_into_hashmap_R(ec);
	initialize_new_entity_safe(ec);

	return ec;
}

void Level::destroy_entity(Entity* e)
{
	if (!e) return;
	uint64_t id = e->get_instance_id();
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
void Level::destroy_component(EntityComponent* ec)
{
	if (!ec) return;
	uint64_t id = ec->get_instance_id();
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


extern ConfigVar g_default_gamemode;

#include "Game/WorldSettings.h"

Level::Level() : all_world_ents(4/*2^4*/), tick_list(4)
{

}

void Level::create(SceneAsset* source, bool is_editor) 
{
	ASSERT(source);

	source_asset = source;
	b_is_editor_level = is_editor;

	if(source->sceneFile)
		insert_unserialized_entities_into_level(*source->sceneFile);
}

void Level::remove_from_update_list(BaseUpdater* b) {
	tick_list.remove(b);
	for (int i = 0; i < wantsToAddToUpdate.size(); i++)
		if (wantsToAddToUpdate[i] == b) {
			wantsToAddToUpdate[i] = nullptr;
		}
}
#include "Framework/InlineVec.h"

void add_entities_and_components_to_init_R(Entity* e, InlineVec<Entity*, 4>& es, InlineVec<EntityComponent*, 16>& cs)
{
	es.push_back(e);
	for (auto c : e->get_all_components())
		cs.push_back(c);
	for (auto child : e->get_all_children())
		add_entities_and_components_to_init_R(child, es, cs);
}

void Level::initialize_new_entity_safe(Entity* e)
{
	ASSERT(e);
	ASSERT(e->init_state == BaseUpdater::initialization_state::HAS_ID);

	// do it this way so initialization can add components/entities and not mess up
	// todo: make sure that they arent deleted by init (deferred delete?)
	InlineVec<Entity*, 4> init_entities;
	InlineVec<EntityComponent*, 16> init_components;
	add_entities_and_components_to_init_R(e, init_entities, init_components);
	
	for (int i = 0; i < init_entities.size();i++) {
		auto e = init_entities[i];
		if(!is_editor_level()||e->get_call_init_in_editor())
			e->pre_start();
	}
	for (int i = 0; i < init_components.size();i++) {
		auto ec = init_components[i];
		if(!is_editor_level()||ec->get_call_init_in_editor())
			ec->pre_start();
	}

	for (int i = 0; i < init_entities.size();i++) {
		auto e = init_entities[i];
		ASSERT(e->init_state == BaseUpdater::initialization_state::HAS_ID);
		e->initialize_internal();
	}
	for (int i = 0; i < init_components.size();i++) {
		auto ec = init_components[i];
		ASSERT(ec->init_state == BaseUpdater::initialization_state::HAS_ID);
		ec->initialize_internal();
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

	for (auto child : e->get_all_children())
		insert_new_native_entity_into_hashmap_R(child);
}

void Level::close_level()
{
	const bool is_this_editor_level = is_editor_level();
	
	for (auto ent : all_world_ents) {
		if (Entity* e = ent->cast_to<Entity>())
			e->destroy();
	}
	all_world_ents.clear_all();

	GetAssets().explicit_asset_free(source_asset);
	GetAssets().unreference_this_channel(0);

	source_asset = nullptr;
}

void Level::insert_unserialized_entities_into_level(UnserializedSceneFile& scene, const SerializedSceneFile* reassign_ids) // was bool assign_new_ids=false
{
	auto& objs = scene.get_objects();
	if (reassign_ids) {
		ASSERT(0);
	}
	else {
		for (auto o : objs) {
			ASSERT(o.second);
			o.second->post_unserialization(get_next_id_and_increment());
			all_world_ents.insert(o.second->get_instance_id(), o.second);
		}
		scene.unserialize_post_assign_ids();
		const bool is_editor = is_editor_level();
		for (auto o : objs) {
			if(!is_editor||o.second->get_call_init_in_editor())
				o.second->pre_start();
		}

		for (auto o : objs) {
			auto ent = o.second;
			if (Entity* e = ent->cast_to<Entity>())
				e->initialize_internal();
			else if (EntityComponent* ec = ent->cast_to<EntityComponent>())
				ec->initialize_internal();
			else
				ASSERT(0);
		}

	}
}


void Level::add_and_init_created_runtime_component(EntityComponent* c)
{
	ASSERT(c->init_state == BaseUpdater::initialization_state::CONSTRUCTOR);
	c->post_unserialization(get_next_id_and_increment());
	all_world_ents.insert(c->get_instance_id(), c);
	if(!is_editor_level()||c->get_call_init_in_editor())
		c->pre_start();
	c->initialize_internal();
}

Entity* Level::spawn_prefab(PrefabAsset* asset)
{
	auto unserialized_scene = unserialize_entities_from_text(asset->text,asset);
	unserialized_scene.get_root_entity()->is_root_of_prefab = true;
	insert_unserialized_entities_into_level(unserialized_scene, nullptr);
	return unserialized_scene.get_root_entity();
}

void Level::queue_deferred_delete(Entity* e)
{
	if (!e) return;
	deferred_delete_list.insert(e->get_instance_id());
}