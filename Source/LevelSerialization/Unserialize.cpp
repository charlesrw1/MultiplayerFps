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
#include "Framework/ReflectionProp.h"

class UnserializationWrapper;
Entity* unserialize_entities_from_text_internal(UnserializedSceneFile& scene, const std::string& text, const std::string& rootpath, 
	PrefabAsset* prefab, Entity* starting_root, IAssetLoadingInterface* load);


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
		auto cpath = path + "~" + std::to_string(c->unique_file_id);
		all_objs.insert({ cpath,c });
	}
	for (auto& child : e->get_children())
	{
		auto cpath = path + "~" + std::to_string(child->unique_file_id);
		all_objs.insert({ cpath, child });
		add_components_and_children_from_entity_R(cpath+"/", child, source);
	}
}

uint32_t parse_fileid(const std::string& path)
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
		auto path_to_use = (path == "/") ? "" : path + "/";
		add_components_and_children_from_entity_R(path_to_use,entity_obj, entity_obj);
		if (parent_ent)
			entity_obj->parent_to(parent_ent);
	}
	else {
		auto comp_obj = e->cast_to<Component>();
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
extern void parse_skip_object(DictParser& in, StringView tok);
void unserialize_one_item_text(
	StringView& tok,						// in token
	DictParser& in,
	UnserializedSceneFile& scene,
	const std::string& root_path,
	PrefabAsset* root_prefab,
	Entity*& inout_root_entity,
	bool& found_new_root,
	IAssetLoadingInterface* load)
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

			auto pfb = load->load_asset(&PrefabAsset::StaticType, type);
			PrefabAsset* asset = pfb->cast_to<PrefabAsset>();
			if (!asset)
				throw std::runtime_error("couldnt find scene file: " + type);

			Entity* this_prefab_root = unserialize_entities_from_text_internal(scene, asset->text, root_path + id + "/", asset, inout_root_entity,load);
			this_prefab_root->is_root_of_prefab = true;
			if(parent)
				this_prefab_root->parent_to(parent);
		}
		// entity or component
		else {
			auto classinfo = ClassBase::find_class(type.c_str());
			if (!classinfo || !classinfo->allocate || !(classinfo->is_a(Entity::StaticType)||classinfo->is_a(Component::StaticType))) {
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

		LevelSerializationContext ctx;
		ctx.in = &scene;
		ctx.in_root = &path;
		ctx.cur_obj = obj;
		auto res = read_props_to_object(obj, (obj)?&obj->get_type():nullptr, in, {}, load, &ctx);
		if (!res.second) {
			throw std::runtime_error("failed prop parse");
		}
		tok = res.first;
	}
	else {
		throw std::runtime_error("expected new/override, got: " + to_std_string_sv(tok));
	}
}

Entity* unserialize_entities_from_text_internal(UnserializedSceneFile& scene, const std::string& text, const std::string& rootpath, PrefabAsset* prefab, Entity* starting_root, IAssetLoadingInterface* load)
{
	DictParser in;
	in.load_from_memory((char*)text.data(), text.size(), "");
	StringView tok;

	Entity* root_entity = starting_root;
	bool found_new_root = false;

	while (in.read_string(tok) && !in.is_eof()) {
		try {
			unserialize_one_item_text(tok, in, scene, rootpath, prefab, root_entity, found_new_root,load);
		}
		catch (std::exception er) {
			sys_print(Warning, "caught parsing error on line %d: %s\n", in.get_last_line(), er.what());
			parse_skip_object(in,tok);
		}

	}

	return root_entity;
}


UnserializedSceneFile unserialize_entities_from_text(const std::string& text, IAssetLoadingInterface* load,PrefabAsset* source_prefab)
{
	if (!load)
		load = AssetDatabase::loader;
		
	UnserializedSceneFile scene;
	auto root = unserialize_entities_from_text_internal(scene,text,"", source_prefab, nullptr,load);
	scene.set_root_entity(root);
	return std::move(scene);
}

void check_props_for_entityptr(void* inst, const PropertyInfoList* list)
{
	for (int i = 0; i < list->count; i++) {
		auto prop = list->list[i];
		if (strcmp(prop.custom_type_str, "EntityPtr") == 0) {
			// wtf!
			Entity** e = (Entity**)prop.get_ptr(inst);
			EntityPtr* eptr = (EntityPtr*)prop.get_ptr(inst);
			if (*e) {
				*eptr = EntityPtr((*e)->get_instance_id());
			}
		}
		else if(prop.type==core_type_id::List) {
			auto listptr = prop.get_ptr(inst);
			auto size = prop.list_ptr->get_size(listptr);
			for (int j = 0; j < size; j++) {
				auto ptr = prop.list_ptr->get_index(listptr, j);
				check_props_for_entityptr(ptr, prop.list_ptr->props_in_list);
			}
		}
	}
}

void UnserializedSceneFile::unserialize_post_assign_ids()
{
	for (auto obj : objs_with_extern_references) {
		auto type = &obj->get_type();
		while (type) {
			auto props = type->props;
			if(props)
				check_props_for_entityptr(obj, props);
			type = type->super_typeinfo;
		}
	}
}