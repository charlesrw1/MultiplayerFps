#include "SerializationAPI.h"
#include "Framework/DictParser.h"
#include "Framework/ClassBase.h"
#include "Game/Entity.h"
#include "Game/EntityComponent.h"
#include "AssetCompile/Someutils.h"
#include <stdexcept>
#include "Level.h"
#include "Assets/AssetDatabase.h"
#include "Game/LevelAssets.h"

class UnserializationWrapper;
Entity* unserialize_entities_from_text_internal(UnserializedSceneFile& scene, const std::string& text, const std::string& rootpath, 
	PrefabAsset* prefab, Entity* starting_root);


void UnserializedSceneFile::delete_objs()
{
	for (auto& o : all_objs)
		delete o.second;
	all_objs.clear();
	objs_with_extern_references.clear();
}

BaseUpdater* UnserializedSceneFile::find(const std::string& path)
{
	auto find = all_objs.find(path);
	return find == all_objs.end() ? nullptr : find->second;
}

void UnserializedSceneFile::add_components_and_children_from_entity_R(const std::string& path, Entity* e, Entity* source)
{
	for (auto& c : e->all_components)
	{
		auto cpath = path + "/~" + std::to_string(c->unique_file_id);
		all_objs.insert({ cpath,c });
	}
	for (auto& child : e->get_all_children())
	{
		auto cpath = path + "/~" + std::to_string(child->unique_file_id);
		all_objs.insert({ cpath, child });
		add_components_and_children_from_entity_R(cpath, child, source);
	}
}

static uint32_t parse_fileid(const std::string& path)
{
	auto last_slash = path.rfind('/');
	if (last_slash != std::string::npos) {
		if (path.size() == 1) return 0;

		ASSERT(last_slash != path.size() - 1);
		ASSERT(path.at(last_slash + 1) != '~');
		return std::stoll(path.substr(last_slash + 1));
	}
	else
		return std::stoll(path);
}

void UnserializedSceneFile::add_obj(const std::string& path, Entity* parent_ent, BaseUpdater* e, Entity* opt_source_owner, PrefabAsset* opt_prefab)
{
	ASSERT(e);

	// cases:
	// 1. adding entity
	// 2. adding component to entity (logic_component)
	// 3. adding component to component (must be world_components)
	// 4. adding entity to component (parent to component)

	// must do:
	// 1. fill out file_id with path integer
	// 2. add to parent if nessecary
	// 3. add to hashmap
	// 4. IMPORTANT: if adding entity, then check for constructor created components (and add them to hashmap)

	// IF_EDITOR >>>>>>>>
	// catch exception later

	e->unique_file_id = parse_fileid(path);
	e->creator_source = opt_source_owner;
	e->what_prefab = opt_prefab;
	// <<<<<<<<<<<<<<<<<<

	auto entity_obj = e->cast_to<Entity>();
	if (entity_obj) {
		add_components_and_children_from_entity_R(path,entity_obj, entity_obj);
		if (parent_ent)
			entity_obj->parent_to_entity(parent_ent);
	}
	else {
		auto comp_obj = e->cast_to<EntityComponent>();
		ASSERT(comp_obj);
		ASSERT(parent_ent);
		parent_ent->add_component_from_unserialization(comp_obj);
	}

	ASSERT(all_objs.find(path) == all_objs.end());
	all_objs.insert({ path, e });
	
}
// root
	// unserialize items
	// root ...

void unserialize_one_item_text(
	StringView tok,						// in token
	DictParser& in,
	UnserializedSceneFile& scene,
	const std::string& root_path,
	PrefabAsset* root_prefab,
	Entity*& inout_root_entity,
	bool& found_new_root)
{

	if (!in.check_item_start(tok))
		throw std::runtime_error("expected {");
	in.read_string(tok);

	if (tok.cmp("new")) {

		// get type
		in.read_string(tok);
		auto type = to_std_string_sv(tok);
		
		// get ID
		if (!in.expect_string("id"))
			throw std::runtime_error("expected id");
		in.read_string(tok);
		auto id = to_std_string_sv(tok);
		in.read_string(tok);
		
		// check for parent
		std::string parentid;
		if (tok.cmp("parent")) {
			in.read_string(tok);
			parentid = to_std_string_sv(tok);
			in.read_string(tok);
		}

		const bool is_root = parentid.empty() && root_prefab;
		ASSERT(!is_root || !found_new_root);
		

		auto get_parent = [&]() -> Entity* {
			if (parentid.empty()) return nullptr;
			if (parentid == "/") return inout_root_entity;
			return scene.find(root_path + parentid)->cast_to<Entity>();
		};

		auto parent = get_parent();
		
		// prefab
		if (type.rfind(".pfb") == type.size() - 4) {
			ASSERT(!is_root);	// cant have prefabs as root in prefab... yet :)

			PrefabAsset* asset = GetAssets().find_sync<PrefabAsset>(type).get();
			if (!asset)
				throw std::runtime_error("couldnt find scene file: " + type);

			Entity* this_prefab_root = unserialize_entities_from_text_internal(scene, asset->text, root_path + id + "/", asset, inout_root_entity);
			this_prefab_root->is_root_of_prefab = true;
			if(parent)
				this_prefab_root->parent_to_entity(parent);
		}
		// entity or component
		else {
			auto classinfo = ClassBase::find_class(type.c_str());
			if (!classinfo || !classinfo->allocate || !(classinfo->is_a(Entity::StaticType)||classinfo->is_a(EntityComponent::StaticType))) {
				throw std::runtime_error("couldn't find class: " + type);
			}
			auto obj = classinfo->allocate();
			ASSERT(obj&&obj->is_a<BaseUpdater>());

			auto path = root_path + id;
			if (is_root && !root_path.empty())
				path = root_path.substr(0,root_path.size() - 1);


			// PUSH BACK OBJ
			scene.add_obj(path, parent, (BaseUpdater*)obj, inout_root_entity, root_prefab);
			// set after!
			if (is_root) {
				found_new_root = true;
				((BaseUpdater*)obj)->is_root_of_prefab = true;
				inout_root_entity = obj->cast_to<Entity>();
			}
		}
	}
	else if (tok.cmp("override")) {

		// get id
		in.read_string(tok);
		auto id = to_std_string_sv(tok);
		auto path = root_path + id;
		if (id == "/" && !root_path.empty()) {
			path = root_path.substr(0, root_path.size() - 1);
		}

		auto obj = scene.find(path);
		if (!obj)
			throw std::runtime_error("couldn't find overrided obj: " + id);

		auto res = read_props_to_object(obj, &obj->get_type(), in, {}, nullptr);
		if (!res.second) {
			throw std::runtime_error("failed prop parse");
		}
		tok = res.first;
	}
	else {
		throw std::runtime_error("expected new/override, got: " + to_std_string_sv(tok));
	}
}

Entity* unserialize_entities_from_text_internal(UnserializedSceneFile& scene, const std::string& text, const std::string& rootpath, PrefabAsset* prefab, Entity* starting_root)
{
	DictParser in;
	in.load_from_memory((uint8_t*)text.data(), text.size(), "");
	StringView tok;

	Entity* root_entity = starting_root;
	bool found_new_root = false;

	while (in.read_string(tok) && !in.is_eof()) {
		unserialize_one_item_text(tok, in, scene,rootpath,prefab,root_entity, found_new_root);
	}

	return root_entity;
}


UnserializedSceneFile unserialize_entities_from_text(const std::string& text, PrefabAsset* source_prefab)
{
	UnserializedSceneFile scene;
	auto root = unserialize_entities_from_text_internal(scene,text,"", source_prefab, nullptr);
	scene.set_root_entity(root);
	return std::move(scene);
}