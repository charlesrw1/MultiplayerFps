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
void unserialize_entities_from_text_internal(UnserializationWrapper& scene, const std::string& text);

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
		return std::stoi(path.substr(last_slash + 1));
	}
	else
		return std::stoi(path);
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

	all_objs.insert({ path, e });
	
}

struct RootSettings
{
	SceneAsset* root_prefab = nullptr;
	Entity* creator = nullptr;
	int root_file_id = 0;
};

#include <array>

struct PrefabItem
{
	PrefabAsset* prefab = nullptr;
	std::string path;
};
struct RootItem
{
	Entity* root_and_creator = nullptr;
	int count = 0;

	std::string id;
	Entity* parent = nullptr;
};

class UnserializationWrapper
{
public:
	UnserializedSceneFile* scene = nullptr;
	std::vector<RootItem> root_stack;
	std::vector<PrefabItem> prefab_stack;

	BaseUpdater* find(const std::string& path) const {
		if (path == "/") {
			ASSERT(!root_stack.empty());
			return root_stack.back().root_and_creator;
		}
		return scene->find(path);
	}
	void add_obj(const std::string& path, Entity* parent_ent, BaseUpdater* e) {
	
		// check for root
		if (!root_stack.empty() && !root_stack.back().root_and_creator) {
			ASSERT(e->is_a<Entity>());
			root_stack.back().root_and_creator = (Entity*)e;
			scene->add_obj(get_current_root() + root_stack.back().id, root_stack.back().parent, e, get_current_root_owner(), get_current_root_scene());
			e->is_root_of_prefab = true;
		}
		else
			scene->add_obj(get_current_root() + path, parent_ent, e, get_current_owner(), get_current_scene());
	}

	std::string get_current_root() {
		return prefab_stack.empty() ? "" : prefab_stack.back().path;
	}
	Entity* get_current_owner() {
		if (root_stack.empty()) return nullptr;
		if (root_stack.back().root_and_creator) return root_stack.back().root_and_creator;
		return nullptr;
	}
	Entity* get_current_root_owner() {
		if (root_stack.size() >= 2) {
			auto p = root_stack[root_stack.size() - 2].root_and_creator;
			ASSERT(p);
		}
		return nullptr;
	}
	PrefabAsset* get_current_root_scene() {
		ASSERT(!root_stack.empty());
		return prefab_stack.at(prefab_stack.size() - root_stack.back().count).prefab;
	}
	PrefabAsset* get_current_scene() {
		if (prefab_stack.empty()) return nullptr;
		return prefab_stack.back().prefab;
	}

	void push_new_scene(PrefabAsset* prefab, std::string scene_file_id, Entity* scene_parent)  {
		
		if (!prefab)
			return;

		if (root_stack.empty() || root_stack.back().root_and_creator) {
			// already has root, so start a new root
			//ASSERT(scene_parent);
			
			RootItem r;
			r.count = 1;
			r.id = scene_file_id;
			r.root_and_creator = nullptr;
			r.parent = scene_parent;

			PrefabItem pi;
			pi.prefab = prefab;
			pi.path = get_current_root() + scene_file_id + "/";

			root_stack.push_back(r);
			prefab_stack.push_back(pi);
		}
		else {
			ASSERT(!root_stack.empty());
			ASSERT(!prefab_stack.empty());
			ASSERT(!scene_parent);
			ASSERT(scene_file_id == "/");
			// this is an inherited scene
			RootItem& back = root_stack.back();
			back.count += 1;

			PrefabItem pi;
			pi.path = get_current_root()+ "*/";
			pi.prefab = prefab;
			prefab_stack.push_back(pi);
		}
	}
	void pop_scene() {
		if (root_stack.empty())
			return;

	}
};
// root
	// unserialize items
	// root ...

void unserialize_one_item_text(
	StringView tok,						// in token
	DictParser& in,
	UnserializationWrapper& scene)
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

		auto parent = parentid.empty() ? nullptr : scene.find(parentid)->cast_to<Entity>();
		ASSERT(parent!=nullptr || parentid.empty());
		
		const bool is_root = !parent;

		// prefab
		if (type.rfind(".pfb") == type.size() - 6) {
			PrefabAsset* asset = GetAssets().find_sync<PrefabAsset>(type).get();
			if (!asset)
				throw std::runtime_error("couldnt find scene file: " + type);

			scene.push_new_scene(asset, id, parent);
			unserialize_entities_from_text_internal(scene,asset->text);
			scene.pop_scene();
		}
		// entity or component
		else {
			auto classinfo = ClassBase::find_class(type.c_str());
			if (!classinfo || !classinfo->allocate || !(classinfo->is_a(Entity::StaticType)||classinfo->is_a(EntityComponent::StaticType))) {
				throw std::runtime_error("couldn't find class: " + type);
			}
			auto obj = classinfo->allocate();
			ASSERT(obj&&obj->is_a<BaseUpdater>());

			// PUSH BACK OBJ
			scene.add_obj(id, parent?parent->cast_to<Entity>():nullptr, (BaseUpdater*)obj);
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

void unserialize_entities_from_text_internal(UnserializationWrapper& scene, const std::string& text)
{
	DictParser in;
	in.load_from_memory((uint8_t*)text.data(), text.size(), "");
	StringView tok;


	while (in.read_string(tok) && !in.is_eof()) {
		unserialize_one_item_text(tok, in, scene);
	}
}


UnserializedSceneFile unserialize_entities_from_text(const std::string& text, PrefabAsset* source_prefab)
{
	UnserializedSceneFile scene;
	UnserializationWrapper wrapper;
	wrapper.scene = &scene;
	wrapper.push_new_scene(source_prefab, "", nullptr);
	unserialize_entities_from_text_internal(wrapper,text);
	auto root = source_prefab?wrapper.root_stack.back().root_and_creator:nullptr;
	scene.set_root_entity(root);
	return std::move(scene);
}