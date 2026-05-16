#include "Level.h"

#include "Framework/Files.h"
#include "AssetCompile/Someutils.h"
#include "Assets/AssetDatabase.h"
#include "Game/LevelAssets.h"
#include "Game/Entity.h"
#include "Game/EntityComponent.h"
#include "Framework/Config.h"
#include "GameEnginePublic.h"
#include "tracy/public/tracy/Tracy.hpp"
#include "Game/Components/LightComponents.h"
#include "Game/Components/MeshComponent.h"
#include "Game/Components/PhysicsComponents.h"
#include "Render/DrawPublic.h"
namespace physx { class PxRigidActor; }
#include "Navigation/LevelNavUtil.h"
#include "Navigation/NavMeshDebugDraw.h"

ConfigVar r_disable_static_strip("r_disable_static_strip", "0", CVAR_BOOL | CVAR_DEV,
								  "If true, skip the runtime static-prop strip pass — every Entity stays in all_world_ents.");

void StaticPropPool::clear_and_release() {
	for (auto& p : props) {
		if (p.render.is_valid())
			idraw->get_scene()->remove_obj(p.render);
		if (p.physics_actor)
			release_static_meshcomponent_physics((physx::PxRigidActor*)p.physics_actor);
	}
	props.clear();
}

namespace {
// Eligibility per plan: anonymous bare Entity with exactly one bare MeshComponent and no children.
// Runtime-only. Conservative on purpose; opting out is "give it a name" or "subclass it".
bool is_strippable_static_prop(const Entity* e) {
	if (!e) return false;
	if (&e->get_type() != &Entity::StaticType) return false;
	if (!e->get_editor_name().empty()) return false;
	if (e->has_tag()) return false;
	if (!e->get_children().empty()) return false;
	if (e->get_parent() != nullptr) return false;
	const auto& comps = e->get_components();
	if (comps.size() != 1) return false;
	auto* mc = comps.front();
	if (!mc || &mc->get_type() != &MeshComponent::StaticType) return false;
	return true;
}
} // namespace

Level::~Level() {
	assert(all_world_ents.num_used == 0);
}

void Level::update_level() {
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
		} else if (e->is_a<Component>()) {
			auto ent = (Component*)e;
			ent->destroy();
		}
	}
	deferred_delete_list.clear();

	GameSceneGiUtil::check_changes();
	NavDebugDraw::tick();
}
void Level::sync_level_render_data() {
	ZoneScoped;
	for (auto ec : wants_sync_update)
		ec->on_sync_render_data();
	wants_sync_update.clear_all();
}
void Level::add_to_sync_render_data_list(Component* ec) {
	// if (eng->get_is_in_overlapped_period())
	wants_sync_update.insert(ec);
	// else
	//	ec->on_sync_render_data();
}

void Level::add_to_update_list(Component* ec) {
	if (b_is_in_update_tick.get_value())
		wantsToAddToUpdate.push_back(ec);
	else
		tick_list.insert(ec);
}

Entity* Level::spawn_entity_class_deferred_internal(const ClassTypeInfo& ti) {
	ASSERT(ti.has_allocate_func());
	ClassBase* e = ti.allocate_this_type(); // allocate + call constructor
	ASSERT(e);

	Entity* ec = nullptr;

	ec = e->cast_to<Entity>();
	if (!ec) {
		sys_print(Error, "spawn_entity_class_deferred_internal failed for %s\n", ti.classname);
		delete e;
		return nullptr;
	}

	insert_new_native_entity_into_hashmap_R(
		ec); // insert into hashmap but DONT call initialize, that is done by the RAII DeferredSpawnScope

	return ec;
}

ConfigVar log_destroy_game_objects("log_destroy_game_objects", "0", CVAR_BOOL, "");

Entity* Level::spawn_entity() {
	auto& ti = Entity::StaticType;
	ASSERT(ti.has_allocate_func());

	ClassBase* e = ti.allocate_this_type(); // allocate + call constructor
	ASSERT(e);

	auto ent = (Entity*)e;

	// call_startup_functions_for_new_entity
	insert_new_native_entity_into_hashmap_R(ent);
	initialize_new_entity_safe(ent);

	return ent;
}

void Level::destroy_entity(Entity* e) {
	if (!e)
		return;
	int64_t id = e->get_instance_id();
	assert(id != 0);
	if (log_destroy_game_objects.get_bool())
		sys_print(Debug, "removing entity (handle:%llu,class:%s)\n", id, e->get_type().classname);

	e->destroy_internal();
	delete e;
// remove from hashmap
#ifdef _DEBUG
	auto ent = all_world_ents.find(id);
	if (!ent) {
		sys_print(Warning, "destroy_entity: entity does not exist in hashmap, double delete?\n");
	}
#endif // _DEBUG
	all_world_ents.remove(id);
}
void Level::destroy_component(Component* ec) {
	if (!ec)
		return;
	wants_sync_update.remove(ec);

	int64_t id = ec->get_instance_id();
	// int uid = ec->unique_file_id;
	assert(id != 0);
	if (log_destroy_game_objects.get_bool())
		sys_print(Debug, "removing eComponent (handle:%llu,class:%s)\n", id, ec->get_type().classname);
	ec->destroy_internal();
	delete ec;
// remove from hashmap
#ifdef _DEBUG
	auto ent = all_world_ents.find(id);
	if (!ent) {
		sys_print(Warning, "destroy_component: entity does not exist in hashmap, double delete?\n");
	}
#endif // _DEBUG
	all_world_ents.remove(id);
}

Level::Level(bool is_editor) : all_world_ents(4 /*2^4*/), tick_list(4), wants_sync_update(4) {}

void Level::start(string source_name, UnserializedSceneFile* source) {
	this->source_name = source_name;
	ASSERT(source);
	double start = GetTime();
	insert_unserialized_entities_into_level_internal(*source, true);
	double end = GetTime();
	sys_print(Debug, "Level::start: took %f\n", float(end - start));

	GameSceneGiUtil::on_scene_load_gi(source_name);
	LevelNavUtil::on_scene_load_nav(source_name);
}

void Level::remove_from_update_list(Component* ec) {
	tick_list.remove(ec);
	for (int i = 0; i < wantsToAddToUpdate.size(); i++)
		if (wantsToAddToUpdate[i] == ec) {
			wantsToAddToUpdate[i] = nullptr;
		}
}

void add_entities_and_components_to_init_R(Entity* e, InlineVec<Entity*, 4>& es, InlineVec<Component*, 16>& cs) {
	es.push_back(e);
	for (auto c : e->get_components())
		cs.push_back(c);
	for (auto child : e->get_children())
		add_entities_and_components_to_init_R(child, es, cs);
}

void Level::initialize_new_entity_safe(Entity* e) {
	ASSERT(e);
	ASSERT(e->init_state == BaseUpdater::initialization_state::HAS_ID);

	// do it this way so initialization can add components/entities and not mess up
	// todo: make sure that they arent deleted by init (deferred delete?)
	InlineVec<Entity*, 4> init_entities;
	InlineVec<Component*, 16> init_components;
	add_entities_and_components_to_init_R(e, init_entities, init_components);

	for (int i = 0; i < init_entities.size(); i++) {
		auto e = init_entities[i];
		ASSERT(e->init_state == BaseUpdater::initialization_state::HAS_ID);
		e->initialize_internal(); // just sets init_state => CALLED_START
	}
	for (int i = 0; i < init_components.size(); i++) {
		auto ec = init_components[i];
		ASSERT(ec->init_state == BaseUpdater::initialization_state::HAS_ID);
		ec->activate_internal_step2();
	}
}

void Level::insert_new_native_entity_into_hashmap_R(Entity* e) {
	ASSERT(e);
	ASSERT(e->init_state == BaseUpdater::initialization_state::CONSTRUCTOR);
	ASSERT(e->get_instance_id() == 0);

	e->post_unserialization(get_next_id_and_increment());

	ASSERT(all_world_ents.find(e->get_instance_id()) == nullptr);
	ASSERT(e->get_instance_id() != 0);

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

void Level::close_level() {
	GameSceneGiUtil::on_scene_exit();

	for (auto ent : all_world_ents) {
		if (Entity* e = ent->cast_to<Entity>())
			e->destroy();
	}
	ASSERT(all_world_ents.num_used == 0);
	all_world_ents.clear_all();
	static_pool.clear_and_release();
}
#include "Framework/Log.h"
#include "Framework/MapUtil.h"
#include "LevelSerialization/SerializeNew.h"
void Level::insert_unserialized_entities_into_level(UnserializedSceneFile& scene) {
	insert_unserialized_entities_into_level_internal(scene, false);
}
void Level::insert_unserialized_entities_into_level_internal(UnserializedSceneFile& scene,
															 bool addSpawnNames) // was bool assign_new_ids=false
{
#ifndef EDITOR_BUILD
	assert(!reassign_ids);
#endif // !EDITOR_BUILD

	// sys_print(Debug, "Level::insert_unserialized_entities_into_level: (level=%s) (objs=%d)\n",
	// sourceAssetName.c_str(), (int)scene.all_obj_vec.size());

	auto& objs = scene.all_obj_vec;

	std::unordered_set<BaseUpdater*> ObjsTest;
#ifdef EDITOR_BUILD

	for (auto o : objs) {
		SetUtil::insert_test_exists(ObjsTest, o);
		if (o->is_a<Entity>()) {
			ASSERT(((Entity*)o)->get_parent() == nullptr);
		}
	}

#endif
	// Pre-pass: identify static-prop entities that will be stripped from all_world_ents.
	// Components are also added to strip_set so the insert+init loops skip them; they get
	// freed during the bake loop via their owner Entity (we never iterate Components in the
	// bake — they're at later indices in objs and would be dangling pointers by then).
	std::unordered_set<BaseUpdater*> strip_set;
	std::vector<Entity*> strip_entities;
	const bool can_strip = !eng->is_editor_level() && !r_disable_static_strip.get_bool();
	if (can_strip) {
		for (auto* o : objs) {
			if (auto* e = o->cast_to<Entity>()) {
				if (is_strippable_static_prop(e)) {
					strip_entities.push_back(e);
					strip_set.insert(e);
					for (auto* c : e->get_components())
						strip_set.insert(c);
				}
			}
		}
	}

	{
		for (auto& o : objs) {
			ASSERT(o);
			if (strip_set.count(o)) continue;
			if (o->get_instance_id() != 0) {
				ASSERT(all_world_ents.find(o->get_instance_id()) == nullptr);
			} else {
				o->post_unserialization(get_next_id_and_increment());
			}
			ASSERT(all_world_ents.find(o->get_instance_id()) == nullptr);
			assert(o->get_instance_id() != 0);
			all_world_ents.insert(o->get_instance_id(), o);
		}
	}
	validate();

	for (int i = 0; i < objs.size(); i++) {
		BaseUpdater* o = objs[i];
		if (!o || strip_set.count(o)) continue;
		assert(o->get_instance_id() != 0);
		auto ent = o;
		if (Entity* e = ent->cast_to<Entity>())
			e->initialize_internal(); // just sets init_state => CALLED_START. also does prefab setup
		else if (Component* ec = ent->cast_to<Component>()) {
			if (!ec->get_owner()) {
				const char* type = ec->get_type().classname;
				sys_print(Error,
						  "Level::insert_unserialized_entities_into_level: component witout owner (type=%s,id=%lld)\n",
						  type, ec->get_instance_id());
				ASSERT(ec->get_instance_id() != 0);
				all_world_ents.remove(ec->get_instance_id());
				delete ec;
				objs[i] = nullptr;
			} else {
				ec->activate_internal_step2();
			}
		} else
			ASSERT(!"Non Eentity/Component?");
	}
	// Bake stripped static-prop entities into the pool. Render + (optional) physics handles
	// outlive the source Entity/Component, which we then free directly — they were never
	// inserted into all_world_ents and never had start() called, so we bypass destroy().
	// Walk strip_entities (built in the pre-pass) instead of objs: the matching Components
	// also live in objs but get freed via their owner here, so iterating objs and calling
	// cast_to on a Component pointer that we've already freed would touch a dangling vtable.
	if (!strip_entities.empty()) {
		for (Entity* e : strip_entities) {
			ASSERT(e->get_components().size() == 1);
			auto* mc = (MeshComponent*)e->get_components().front();
			ASSERT(mc && &mc->get_type() == &MeshComponent::StaticType);

			StaticProp prop;
			if (mc->get_model()) {
				prop.render = bake_static_meshcomponent_render(*mc, e->get_ws_transform());
				if (mc->get_add_collision())
					prop.physics_actor = bake_static_meshcomponent_physics(mc->get_model(), e->get_ws_transform());
			}
			static_pool.add(prop);

			// Free MC + Entity. Both are at init_state == CONSTRUCTOR (never post_unserialized
			// for stripped objs) so destructors only assert they were never started — true here.
			e->all_components.clear(); // friend access; clear before delete to avoid stale iteration
			delete mc;
			delete e;
		}
		// Null out stripped entries so the ownership-transfer path doesn't touch dangling pointers.
		// strip_set contains the (now-deleted) Entity AND Component pointers; comparing the dangling
		// pointer values is fine (we never dereference them).
		for (auto& o : objs) {
			if (o && strip_set.count(o)) o = nullptr;
		}
	}

	// Take ownership of any unknown-typename JSON blobs the reader stashed. They round-trip
	// to the next save so unresolved component types aren't silently dropped from the .tmap.
	preserved_unknown_objs.insert(preserved_unknown_objs.end(),
								  std::make_move_iterator(scene.unknown_objs.begin()),
								  std::make_move_iterator(scene.unknown_objs.end()));
	scene.unknown_objs.clear();
	unknown_field_warnings.insert(unknown_field_warnings.end(),
								  std::make_move_iterator(scene.unknown_field_warnings.begin()),
								  std::make_move_iterator(scene.unknown_field_warnings.end()));
	scene.unknown_field_warnings.clear();
	// Ownership of the BaseUpdater* now lives in all_world_ents.
	scene.mark_ownership_transferred();
}

void Level::add_and_init_created_runtime_component(Component* c) {
	ASSERT(c->init_state == BaseUpdater::initialization_state::CONSTRUCTOR);
	c->post_unserialization(get_next_id_and_increment());
	ASSERT(all_world_ents.find(c->get_instance_id()) == nullptr);
	all_world_ents.insert(c->get_instance_id(), c);
	c->activate_internal_step2();
}

void Level::queue_deferred_delete(BaseUpdater* e) {
	if (!e)
		return;
	if (log_destroy_game_objects.get_bool()) {
		sys_print(Debug, "Level::queue_deferred_delete: (%lld)", e->get_instance_id());
	}
	deferred_delete_list.insert(e->get_instance_id());
}

void Level::validate() {
	for (auto o : all_world_ents) {
		if (auto e = o->cast_to<Entity>())
			e->validate_check();
	}
}

Entity* Level::find_initial_entity_by_name(const string& name) const {
	return MapUtil::get_or(spawnNameToEntity, name, EntityPtr()).get();
}

Component* Level::find_first_component(const ClassTypeInfo* type) const {
	for (auto o : all_world_ents) {
		if (o->is_a<Component>() && o->get_type().is_a(*type))
			return (Component*)o;
	}
	return nullptr;
}
