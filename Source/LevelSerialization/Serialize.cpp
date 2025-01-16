#include "SerializationAPI.h"
#include <unordered_map>
#include <unordered_set>
#include <stdexcept>
#include "Framework/Util.h"
#include "Game/Entity.h"
#include "Game/EntityComponent.h"
#include "Framework/DictWriter.h"
#include "Level.h"
#include "Game/LevelAssets.h"
#if 0
bool validate_selection_R(EntityComponent* self_c, const std::unordered_set<const BaseUpdater*>& is_in_set)
{
	if (is_in_set.find(self_c) == is_in_set.end())
		return false;
	auto& children = self_c->get_children();
	for (auto c : children) {
		if (!validate_selection_R(c, is_in_set))
			return false;
	}

	return true;
}

bool validate_selection_for_serialization(const std::vector<BaseUpdater*>& input_objs, const std::unordered_set<const BaseUpdater*>& is_in_set)
{
	auto find_in_set = [&](const BaseUpdater* c) -> bool {
		return is_in_set.find(c) != is_in_set.end();
	};

	for (auto obj : input_objs) {

		// * if a object is part of a prefab, then entire prefab has to be selected (children checked below)
		if (obj->creator_source && !find_in_set(obj->creator_source))
			return false;

		// * all children of a selected object are selected
		if (obj->is_a<Entity>()) {
			auto root = ((Entity*)obj)->get_root_component();
			ASSERT(root);
			if (!validate_selection_R(root,is_in_set))
				return false;
		}
		else if (obj->is_a<EntityComponent>()) {
			auto ec = (EntityComponent*)obj;
			if (ec->is_root() && !find_in_set(ec->get_owner()))
				return false;

			if (!validate_selection_R((EntityComponent*)obj,is_in_set))
				return false;
		}
		else
			ASSERT(0);
	}


	return true;
}
#endif

//
//	Example:
//	
//	C++:
//	my-entity
//		mesh-component	(1)
//	
//	ThePrefab:
//	my-entity
//		mesh-component
//		another-entity	(1)
//			mesh-component (2)
//			
//	Inherited-ThePrefab:
//	ThePrefab (my-entity)
//		mesh-component
//		another-entity
//			mesh-component
//		ability-component (1)
//	
//	NestedPrefab:
//	another-entity
//		light-component (1)
//		mesh-component (2)
//		ThePrefab (my-entity) (3)
//			mesh-component
//			another-entity
//				mesh-component
//			ability-component
//		my-entity			(4)
//			mesh-component
//	In Map:
//												| PATH			| owner			| prefab		| is_root_of_prefab
//	map-root								(0)	| 0				| null			| null			| yes
//		my-entity 							(1)	| 1				| null			| null			| yes	
//			mesh-component						| 1/~1			| my-entity		| null			| no	
//											    |				|				|				|		
//		ThePrefab (my-entity)				(2) | 2				| null			| ThePrefab		| yes	
//			mesh-component						| 2/~1			| my-entity		| null			| no	
//			another-entity						| 2/1			| my-entity		| ThePrefab		| no
//				mesh-component					| 2/2			| my-entity		| ThePrefab		| no	
//											    |				|				|				|		
//		The2ndPrefab (my-entity)			(3) | 3				| null			| 2ndPrefab		| yes		
//			mesh-component						| 3/*/~1		|				|				|		
//			another-entity						| 3/*/1			|				|				|		
//				mesh-component					| 3/*/2			|				|				|		
//			ability-component					| 3/1			|				|				|		
//												|				|				|				|		
//		NestedPrefab (another-entity)		(4)	| 4				| null			| NestedPrefab	| yes		
//			light-component						| 4/1			| another-entity| NestedPrefab	| no		
//			mesh-component						| 4/2			| another-entity| NestedPrefab	| no	
//			The2ndPrefab (my-entity)			| 4/3			| another-entity| The2ndPrefab	| yes	
//				mesh-component					| 4/*/3/~1		| my-entity		| null			| no		
//				another-entity					| 4/*/3/1		| my-entity		| ThePrefab		| no		
//					mesh-component				| 4/*/3/2		| my-entity		| ThePrefab		| no		
//				ability-component				| 4/3/1			| my-entity		| The2ndPrefab	| no
//			my-entity2							| 4/4			| another-entity| null			| yes
//				mesh-component					| 4/4/~1		| my-entity2	| null			| no


// TODO prefabs
// rules:
// * path based on source

static int find_inheritance_level(const PrefabAsset* owner, const PrefabAsset* me)
{
	int count = 0;
	bool found = false;
	while (owner) {
		if (owner == me) {
			found = true;
			break;
		}
		owner = owner->parent_scene;
		count++;
	}
	ASSERT(found || !me);
	return count;
}

std::string build_path_for_object(const BaseUpdater* obj)
{
	if(!obj->creator_source)		// no creator source, top level objects
		return std::to_string(obj->unique_file_id);
	else {
		auto parentpath = build_path_for_object(obj->creator_source);
		if(obj->is_root_of_prefab)
			return parentpath + "/" + std::to_string(obj->unique_file_id);	// prefab root, just file id
		else if (!obj->what_prefab) { // native objects

			int level = find_inheritance_level(obj->creator_source->what_prefab, nullptr);
			int stars = level - 1;
			auto path = parentpath + "/";
			for (int i = 0; i < stars; i++) path += "*/";
			return path + "~" + std::to_string(obj->unique_file_id);
		}
		else {	// what_prefab != nullptr
	
			int stars = find_inheritance_level(obj->creator_source->what_prefab, obj->what_prefab);

			auto path = parentpath + "/";
			for (int i = 0; i < stars; i++) path += "*/";
			return path + std::to_string(obj->unique_file_id);
		}
	}

	/*
	if (obj->creator_source == obj)
		return std::to_string(obj->unique_file_id) + "/0";
	else if(obj->creator_source)
		return build_path_for_object(obj->creator_source) + "/" + std::to_string(obj->unique_file_id);;
	return std::to_string(obj->unique_file_id);
	*/
}
std::string make_path_relative_to(const std::string& inpath, const std::string& outer_path)
{
	// inpath = 4/3/~1
	// outer_path = 4
	// return 3/~1

	if (inpath.find(outer_path) == 0) {
		auto path = inpath.substr(outer_path.size() + 1);	// remove "/" too
		if (path.empty()) return "/";
		ASSERT(path[0] != '/');
		return path;
	}
	ASSERT(0);
}


// rules:
// * gets "new" if object created in current context (if edting map, prefab entities dont count)
// * if owner isnt in set
// * "old" entity/component can still have "new" children, so traverse

void serialize_new_object_text_R(
	const Entity* e, 
	DictWriter& out,
	bool skip_parent)
{
	if (e->dont_serialize_or_edit)
		return;

	auto this_is_newly_created = [&](const BaseUpdater* b) -> bool {
		return b->creator_source == nullptr;
	};

	auto serialize_new = [&](const BaseUpdater* b) {
		out.write_item_start();
		out.write_key_value("new", b->get_type().classname);
		out.write_key_value("id", build_path_for_object(b).c_str());

		BaseUpdater* parent = nullptr;
		if (auto ent = b->cast_to<Entity>()) {
			parent = ent->get_entity_parent();
		}
		else {
			auto ec = b->cast_to<EntityComponent>();
			ASSERT(ec);
			parent = ec->get_owner();
			ASSERT(parent);
		}
		if (!skip_parent && parent) {
			out.write_key_value("parent", build_path_for_object(parent).c_str());
		}

		out.write_item_end();
	};

	if (this_is_newly_created(e))
		serialize_new(e);

	auto& all_comps = e->get_all_components();
	for (auto c : all_comps) {
		if (!c->dont_serialize_or_edit && this_is_newly_created(c))
			serialize_new(c);
	}
	auto& children = e->get_all_children();
	for (auto child : children)
		serialize_new_object_text_R(child, out, false);
}


static void write_just_props(ClassBase* e, const ClassBase* diff, DictWriter& out, SerializeEntityObjectContext* ctx)
{
	std::vector<PropertyListInstancePair> props;
	const ClassTypeInfo* typeinfo = &e->get_type();
	while (typeinfo) {
		if (typeinfo->props)
			props.push_back({ typeinfo->props, e });
		typeinfo = typeinfo->super_typeinfo;
	}

	for (auto& proplist : props) {
		if (proplist.list)
			write_properties_with_diff(*const_cast<PropertyInfoList*>(proplist.list), e, diff, out, ctx);
	}
}

// fixme: cache a table or something
const Entity* find_entity_diff_in_children_R(const Entity* input, const Entity* check)
{
	if (input->unique_file_id == check->unique_file_id)
		return check;
	for (auto c : check->get_all_children()) {
		auto ptr = find_entity_diff_in_children_R(input, c);
		if (ptr) return ptr;
	}
	return nullptr;
}
const EntityComponent* find_component_diff_in_children_R(const EntityComponent* input, const Entity* check)
{
	for (auto c : check->get_all_components()) {
		if (c->unique_file_id == input->unique_file_id)
			return c;
	}

	for (auto c : check->get_all_children()) {
		auto ptr = find_component_diff_in_children_R(input, c);
		if (ptr) return ptr;
	}
	return nullptr;
}


// to serialize:
// 1. sort by hierarchy
// 2. all "new" commands go first
// 3. "override" commands go second
// 4. fileID, parentFileID
// 5. skip transient components
// 6. only add "new" commands when the source_owner is nullptr (currently editing source)

SerializedSceneFile serialize_entities_to_text(const std::vector<Entity*>& input_objs)
{
	std::unordered_set<const BaseUpdater*> is_in_set;
	auto find_in_set = [&](const BaseUpdater* c) -> bool {
		return is_in_set.find(c) != is_in_set.end();
	};
	for (auto obj : input_objs) {
		ASSERT(!find_in_set(obj));
		is_in_set.insert(obj);
	}

	//ASSERT(validate_selection_for_serialization(input_objs,is_in_set));

	SerializedSceneFile output;
	DictWriter out;

	auto add_to_extern_parents = [&](const BaseUpdater* obj, const BaseUpdater* parent) {
		SerializedSceneFile::external_parent ext;
		ext.child_path = build_path_for_object(obj);
		ext.external_parent_handle = parent->instance_id;
		output.extern_parents.push_back(ext);
	};

	// write out top level objects (no parent in serialize set)
	for (auto obj : input_objs) {
		auto ent = obj->cast_to<Entity>();
		ASSERT(ent);
		auto root_parent = ent->get_entity_parent();
		if (!find_in_set(root_parent)&&!ent->dont_serialize_or_edit) {
			
			if (root_parent)
				add_to_extern_parents(obj, root_parent);
			
			serialize_new_object_text_R(ent, out, root_parent!=nullptr);
		}
	}


	auto find_diff_class = [&](const BaseUpdater* obj) -> const ClassBase* {

		if (!obj->creator_source && !obj->what_prefab) {
			auto default_ = obj->get_type().default_class_object;
			return default_;
		}


		auto top_level = obj;
		while (top_level->creator_source)
			top_level = top_level->creator_source;
		ASSERT(!top_level->creator_source && (!top_level->what_prefab || top_level->is_root_of_prefab));
		if (!top_level->what_prefab) {
			auto source_owner_default = (const Entity*)top_level->get_type().default_class_object;
			ASSERT(source_owner_default);
			if (obj->is_a<Entity>())
				return find_entity_diff_in_children_R((Entity*)obj, source_owner_default);
			else {
				ASSERT(obj->is_a<EntityComponent>());
				return find_component_diff_in_children_R((EntityComponent*)obj, source_owner_default);
			}
		}
		else {
			auto& objs = top_level->what_prefab->sceneFile->get_objects();
			if (top_level == obj)
				return objs.find("/")->second;
			else
				return objs.find(make_path_relative_to(build_path_for_object(obj), build_path_for_object(top_level)))->second;
		}
	};

	for (auto inobj : input_objs) {
		// serialize overrides

		auto write_obj = [&](BaseUpdater* obj) {
			auto objpath = build_path_for_object(obj);
			output.path_to_instance_handle.insert({ objpath,obj->instance_id });
			out.write_item_start();
			out.write_key_value("override", objpath.c_str());

			auto diff = find_diff_class(obj);
			write_just_props(obj, diff, out, nullptr);

			out.write_item_end();
		};

		if (!inobj->dont_serialize_or_edit) {
			write_obj(inobj);
			for (auto comp : inobj->get_all_components())
				if (!comp->dont_serialize_or_edit)
					write_obj(comp);
		}
	}

	output.text = std::move(out.get_output());

	return output;
}
