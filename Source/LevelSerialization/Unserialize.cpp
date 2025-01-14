#include "SerializationAPI.h"
#include "Framework/DictParser.h"
#include "Framework/ClassBase.h"
#include "Game/Entity.h"
#include "Game/EntityComponent.h"
#include "AssetCompile/Someutils.h"
#include <stdexcept>

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
		auto cpath = path + "/" + std::to_string(c->unique_file_id);
		c->creator_source = source;
		all_objs.insert({ cpath,c });
	}
	for (auto& child : e->get_all_children())
	{
		auto cpath = path + "/" + std::to_string(child->unique_file_id);
		child->creator_source = source;
		all_objs.insert({ cpath, child });
		add_components_and_children_from_entity_R(path, child, source);
	}
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
	int file_id = 0;
	size_t last_slash = path.rfind('/'); 
	if (last_slash != std::string::npos)
		file_id = std::stoi(path.substr(last_slash + 1));
	else
		file_id = std::stoi(path);

	e->unique_file_id = file_id;
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

	all_objs.insert({ path, e });
	
}

void unserialize_one_item_text(const std::string& root_path, BaseUpdater* root_parent, StringView tok, DictParser& in, UnserializedSceneFile& scene)
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
			parentid = to_std_string_sv(tok);
			in.read_string(tok);
		}

		if (type.rfind('.') != std::string::npos) {
			// prefab
		}
		else {
			auto classinfo = ClassBase::find_class(type.c_str());
			if (!classinfo || !classinfo->allocate || !(classinfo->is_a(Entity::StaticType)||classinfo->is_a(EntityComponent::StaticType))) {
				throw std::runtime_error("couldn't find class: " + type);
			}
			auto obj = classinfo->allocate();
			ASSERT(obj&&obj->is_a<BaseUpdater>());

			auto parent = parentid.empty() ? root_parent : scene.find(parentid);
			ASSERT(parent==nullptr || parentid.empty());


			// PUSH BACK OBJ
			scene.add_obj(root_path + id, parent?parent->cast_to<Entity>():nullptr, (BaseUpdater*)obj);
		}
	}
	else if (tok.cmp("override")) {

		// get id
		in.read_string(tok);
		auto id = to_std_string_sv(tok);

		auto obj = scene.find(id);
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

void unserialize_entities_from_text_internal(UnserializedSceneFile& scene, const std::string& text)
{
	DictParser in;
	in.load_from_memory((uint8_t*)text.data(), text.size(), "lvlsrl");
	StringView tok;
	while (in.read_string(tok) && !in.is_eof()) {
		unserialize_one_item_text("",nullptr,tok, in, scene );
	}
}


UnserializedSceneFile unserialize_entities_from_text(const std::string& text)
{
	UnserializedSceneFile scene;
	unserialize_entities_from_text_internal(scene, text);
	return std::move(scene);
}