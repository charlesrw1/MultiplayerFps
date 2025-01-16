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

bool am_i_the_root_prefab_node(const Entity* b, const PrefabAsset* for_prefab)
{
	if (!for_prefab) return false;
	bool has_parent = b->get_entity_parent();
	return !has_parent || (b->is_root_of_prefab&&for_prefab==b->what_prefab);
}

bool serializing_for_prefab(const PrefabAsset* for_prefab)
{
	return for_prefab != nullptr;
}

std::string build_path_for_object(const BaseUpdater* obj, const PrefabAsset* for_prefab)
{
	if (serializing_for_prefab(for_prefab)) {

		if (auto e = obj->cast_to<Entity>()) {
			if (am_i_the_root_prefab_node(e, for_prefab)) {
				ASSERT(e->is_root_of_prefab || !e->get_entity_parent());
				return "/";
			}
			// fallthrough
		}

		if (obj->what_prefab == for_prefab || !obj->creator_source || am_i_the_root_prefab_node(obj->creator_source,for_prefab)) {
			return std::to_string(obj->unique_file_id);
		}
		// fallthrough
	}

	if(!obj->creator_source)		// no creator source, top level objects
		return std::to_string(obj->unique_file_id);
	else {
	
		auto parentpath = build_path_for_object(obj->creator_source, for_prefab);
		if(obj->is_root_of_prefab)
			return parentpath + "/" + std::to_string(obj->unique_file_id);	// prefab root, just file id
		else if (!obj->what_prefab) { // native objects (creator_source!=null, what_prefab==null)

			auto path = parentpath + "/";
			return path + "~" + std::to_string(obj->unique_file_id);
		}
		else {	// prefab objects

			auto path = parentpath + "/";
			return path + std::to_string(obj->unique_file_id);
		}
	}
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

const char* get_type_for_new_serialized_item(const BaseUpdater* b, PrefabAsset* for_prefab)
{
	if (b->what_prefab && b->what_prefab != for_prefab)
		return  b->what_prefab->get_name().c_str();
	else
		return b->get_type().classname;
}

bool this_is_newly_created(const BaseUpdater* b, PrefabAsset* for_prefab)
{
	return b->creator_source == nullptr || (for_prefab && b->what_prefab == for_prefab) || am_i_the_root_prefab_node(b->creator_source,for_prefab);
}

// rules:
// * gets "new" if object created in current context (if edting map, prefab entities dont count)
// * if owner isnt in set
// * "old" entity/component can still have "new" children, so traverse

void serialize_new_object_text_R(
	const Entity* e, 
	DictWriter& out,
	bool dont_write_parent,
	PrefabAsset* for_prefab)
{
	if (e->dont_serialize_or_edit)
		return;

	auto serialize_new = [&](const BaseUpdater* b) {
		out.write_item_start();

		out.write_key_value("new", get_type_for_new_serialized_item(b,for_prefab));
	
		out.write_key_value("id", build_path_for_object(b, for_prefab).c_str());

		Entity* parent = nullptr;
		if (auto ent = b->cast_to<Entity>()) {
			parent = ent->get_entity_parent();
		}
		else {
			auto ec = b->cast_to<EntityComponent>();
			ASSERT(ec);
			parent = ec->get_owner();
			ASSERT(parent);
		}

		if (parent && !dont_write_parent) {
			out.write_key_value("parent", build_path_for_object(parent, for_prefab).c_str());
		}

		out.write_item_end();
	};

	if (this_is_newly_created(e, for_prefab))
		serialize_new(e);

	auto& all_comps = e->get_all_components();
	for (auto c : all_comps) {
		if (!c->dont_serialize_or_edit && this_is_newly_created(c, for_prefab))
			serialize_new(c);
	}
	auto& children = e->get_all_children();
	for (auto child : children)
		serialize_new_object_text_R(child, out, false /* dont_write_parent=false*/, for_prefab);
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

void validate_for_prefab_R(Entity* e, PrefabAsset* for_prefab)
{
	//if(!(e->what_prefab==for_prefab&&e->is_root_of_prefab&&e->creator_source==nullptr))
	//	ASSERT(e->creator_source != nullptr || e->is_native_created);
	//for(auto c : e->get_all_components())
	//	ASSERT(c->creator_source != nullptr || c->is_native_created);
	//for (auto c : e->get_all_children())
	//	validate_for_prefab_R(c, for_prefab);
}

void validate_serialize_input(const std::vector<Entity*>& input_objs, PrefabAsset* for_prefab)
{
	if (for_prefab) {
		ASSERT(input_objs.size() == 1);
		//ASSERT(input_objs[0]->what_prefab == for_prefab);
		//ASSERT(input_objs[0]->creator_source == nullptr);
		//ASSERT(input_objs[0]->is_root_of_prefab);


		validate_for_prefab_R(input_objs[0], for_prefab);
	}
	else {
		for (auto o : input_objs) {
			ASSERT(!o->what_prefab || o->is_root_of_prefab);
		}
	}
}

// to serialize:
// 1. sort by hierarchy
// 2. all "new" commands go first
// 3. "override" commands go second
// 4. fileID, parentFileID
// 5. skip transient components
// 6. only add "new" commands when the source_owner is nullptr (currently editing source)

// for_prefab: "im serializing for this prefab, the root node of this will be "/" and referenced by ".", dont include parent id in path if parent is root"

void add_to_write_set_R(Entity* o, std::unordered_set<Entity*>& to_write)
{
	if (o->dont_serialize_or_edit)
		return;
	to_write.insert(o);
	for (auto c : o->get_all_children())
		add_to_write_set_R(c, to_write);

}
std::vector<Entity*> root_objects_to_write(const std::vector<Entity*>& input_objs)
{
	std::unordered_set<Entity*> to_write;
	for (auto o : input_objs) {
		add_to_write_set_R(o,to_write);
	}

	std::vector<Entity*> roots;
	for (auto o : to_write) {
		if (!o->get_entity_parent() || to_write.find(o->get_entity_parent()) == to_write.end())
			roots.push_back(o);
	}

	return roots;
}

// cases:
// 1. class is newly created, diff from C++
//		a. use default object
//		b. look in default object for native constructed
// 2. class is a created prefab, diff from prefab

const ClassBase* find_diff_class(const BaseUpdater* obj, PrefabAsset* for_prefab)
{
	if (!obj->creator_source && !obj->what_prefab) {
		auto default_ = obj->get_type().default_class_object;
		return default_;
	}
	
	auto top_level = obj;
	while (top_level->creator_source && (!for_prefab || top_level->creator_source->what_prefab != for_prefab))
		top_level = top_level->creator_source;
	
	if(!for_prefab)
		ASSERT(!top_level->what_prefab || top_level->is_root_of_prefab);

	if (!top_level->what_prefab || for_prefab == top_level->what_prefab ) {
		auto source_owner_default = (const Entity*)top_level->get_type().default_class_object;

		if (top_level == obj) return source_owner_default;

		ASSERT(obj->is_native_created);

		ASSERT(source_owner_default);

		if (obj->is_a<Entity>())
			return find_entity_diff_in_children_R((Entity*)obj, source_owner_default);
		else {
			ASSERT(obj->is_a<EntityComponent>());
			return find_component_diff_in_children_R((EntityComponent*)obj, source_owner_default);
		}
	}
	else {
		ASSERT(top_level->what_prefab && top_level->what_prefab->sceneFile);

		auto& objs = top_level->what_prefab->sceneFile->get_objects();

		auto path = build_path_for_object(obj, top_level->what_prefab);

		return objs.find(path)->second;

	}
}

void serialize_overrides_R(Entity* e, PrefabAsset* for_prefab, SerializedSceneFile& output, DictWriter& out)
{
	auto write_obj = [&](BaseUpdater* obj) {
		auto objpath = build_path_for_object(obj, for_prefab);
		auto e = obj->cast_to<Entity>();

		output.path_to_instance_handle.insert({ objpath,obj->instance_id });
		out.write_item_start();
		out.write_key_value("override", objpath.c_str());

		auto diff = find_diff_class(obj,for_prefab);
		write_just_props(obj, diff, out, nullptr);

		out.write_item_end();
	};

	if (!e->dont_serialize_or_edit) {
		write_obj(e);
		for (auto comp : e->get_all_components())
			if (!comp->dont_serialize_or_edit)
				write_obj(comp);
		for (auto o : e->get_all_children())
			serialize_overrides_R(o, for_prefab, output, out);
	}
}

SerializedSceneFile serialize_entities_to_text(const std::vector<Entity*>& input_objs, PrefabAsset* for_prefab)
{
	
	SerializedSceneFile output;
	DictWriter out;

	auto add_to_extern_parents = [&](const BaseUpdater* obj, const BaseUpdater* parent) {
		SerializedSceneFile::external_parent ext;
		ext.child_path = build_path_for_object(obj, for_prefab);
		ext.external_parent_handle = parent->instance_id;
		output.extern_parents.push_back(ext);
	};

	// write out top level objects (no parent in serialize set)


	auto roots = root_objects_to_write(input_objs);

	validate_serialize_input(roots, for_prefab);

	for (auto obj : roots)
	{
		auto ent = obj->cast_to<Entity>();
		ASSERT(ent);
		ASSERT(!ent->dont_serialize_or_edit);
		auto root_parent = ent->get_entity_parent();
		if (root_parent)
				add_to_extern_parents(obj, root_parent);
			
		serialize_new_object_text_R(ent, out, root_parent != nullptr /* dont_write_parent if root_parent exists*/, for_prefab);
	}


	

	for (auto inobj : roots) {
		// serialize overrides

		serialize_overrides_R(inobj,for_prefab, output, out);
	}

	output.text = std::move(out.get_output());

	return output;
}