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
#include "GameEnginePublic.h"
#include "Framework/ReflectionProp.h"

// TODO prefabs
// rules:
// * path based on source

CLASS_IMPL(LevelSerializationContext);

Entity* LevelSerializationContext::get_entity(uint64_t handle)
{
	ASSERT(out&&!in);
	bool is_from_diff = handle & (1ull << 63ull);
	BaseUpdater* obj = nullptr;
	if (is_from_diff) {
		ASSERT(diffprefab)
		obj= diffprefab->find_entity(handle);
	}
	else
		obj = eng->get_level()->get_entity(handle);
	if (obj)
		return obj->cast_to<Entity>();
	return nullptr;
}

bool am_i_the_root_prefab_node(const Entity* b, const PrefabAsset* for_prefab)
{
	if (!for_prefab) return false;
	bool has_parent = b->get_parent();
	return !has_parent || (b->is_root_of_prefab&&for_prefab==b->what_prefab);
}

bool serializing_for_prefab(const PrefabAsset* for_prefab)
{
	return for_prefab != nullptr;
}

bool this_is_newly_created(const BaseUpdater* b, const PrefabAsset* for_prefab)
{
	return (b->creator_source == nullptr || (for_prefab && b->what_prefab == for_prefab) || am_i_the_root_prefab_node(b->creator_source,for_prefab));
}
bool serialize_this_objects_children(const Entity* b, const PrefabAsset* for_prefab)
{
	if (b->dont_serialize_or_edit)
		return false;
	if (b->what_prefab && b->what_prefab != for_prefab && b->is_root_of_prefab && !b->get_prefab_editable()) {
		return false;
	}
	return true;
}
bool this_is_a_serializeable_object(const BaseUpdater* b, const PrefabAsset* for_prefab)
{
	if (b->dont_serialize_or_edit)
		return false;
	if (this_is_newly_created(b, for_prefab))
		return true;
	if (b->creator_source && !serialize_this_objects_children(b->creator_source, for_prefab))
		return false;
	return true;
}

std::string build_path_for_object(const BaseUpdater* obj, const PrefabAsset* for_prefab)
{
	if (serializing_for_prefab(for_prefab)) {

		if (auto e = obj->cast_to<Entity>()) {
			if (am_i_the_root_prefab_node(e, for_prefab)) {
				ASSERT(e->is_root_of_prefab || !e->get_parent());
				return "/";
			}
			// fallthrough
		}

		if (this_is_newly_created(obj,for_prefab)) {
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
		else {	// prefab objects

			auto path = parentpath + "/";
			return path + std::to_string(obj->unique_file_id);
		}
	}
}


const char* get_type_for_new_serialized_item(const BaseUpdater* b, PrefabAsset* for_prefab)
{
	if (b->what_prefab && b->what_prefab != for_prefab)
		return  b->what_prefab->get_name().c_str();
	else
		return b->get_type().classname;
}


// rules:
// * gets "new" if object created in current context (if edting map, prefab entities dont count)
// * if owner isnt in set
// * "old" entity/component can still have "new" children, so traverse

extern uint32_t parse_fileid(const std::string& path);

void serialize_new_object_text_R(
	const Entity* e, 
	DictWriter& out,
	bool dont_write_parent,
	PrefabAsset* for_prefab,
	SerializedSceneFile& output)
{
	if (e->dont_serialize_or_edit)
		return;

	auto serialize_new = [&](const BaseUpdater* b, bool dont_write_parent_for_this) {

		ASSERT(this_is_a_serializeable_object(b, for_prefab));

		out.write_item_start();

		out.write_key_value("new", get_type_for_new_serialized_item(b,for_prefab));
	
		auto id = build_path_for_object(b, for_prefab);
		out.write_key_value("id", id.c_str());

		Entity* parent = nullptr;
		if (auto ent = b->cast_to<Entity>()) {
			parent = ent->get_parent();
		}
		else {
			auto ec = b->cast_to<EntityComponent>();
			ASSERT(ec);
			parent = ec->get_owner();
			ASSERT(parent);
		}

		if (parent && !dont_write_parent_for_this) {
			out.write_key_value("parent", build_path_for_object(parent, for_prefab).c_str());
		}

		out.write_item_end();
	};

	auto objpath = build_path_for_object(e, for_prefab);
	ASSERT(output.path_to_instance_handle.find(objpath) == output.path_to_instance_handle.end());
	output.path_to_instance_handle.insert({ objpath,e->get_instance_id() });

	if (this_is_newly_created(e, for_prefab))
		serialize_new(e, dont_write_parent);

	if (serialize_this_objects_children(e, for_prefab)) {
		auto& all_comps = e->get_components();
		for (auto c : all_comps) {
			if (!c->dont_serialize_or_edit && this_is_newly_created(c, for_prefab))
				serialize_new(c, false);
		}
		auto& children = e->get_children();
		for (auto child : children)
			serialize_new_object_text_R(child, out, false /* dont_write_parent=false*/, for_prefab, output);
	}
}


static void write_just_props(ClassBase* e, const ClassBase* diff, DictWriter& out, LevelSerializationContext* ctx)
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



void validate_serialize_input(const std::vector<Entity*>& input_objs, PrefabAsset* for_prefab)
{
	if (for_prefab) {

		auto assert_one_root = [&]() {
			bool root_found = false;
			for (auto o : input_objs) {
				ASSERT(o->get_parent() || !root_found);
				if (o->get_parent()) 
					root_found = true;
			}
		};
		assert_one_root();
	}
	else {
		for (auto o : input_objs) {
			ASSERT(!o->what_prefab || o->is_root_of_prefab);

			if (o->what_prefab && !o->is_root_of_prefab) {
				ASSERT(o->creator_source);
			}
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
	for (auto c : o->get_children())
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
		if (!o->get_parent() || to_write.find(o->get_parent()) == to_write.end())
			roots.push_back(o);
	}

	return roots;
}

// cases:
// 1. class is newly created, diff from C++
//		a. use default object
//		b. look in default object for native constructed
// 2. class is a created prefab, diff from prefab

const ClassBase* find_diff_class(const BaseUpdater* obj, PrefabAsset* for_prefab, PrefabAsset*& out_prefab_diff)
{
	if (!obj->creator_source && !obj->what_prefab) {
		auto default_ = obj->get_type().default_class_object;
		return default_;
	}
	
	auto top_level = obj;
	while (top_level->creator_source && (!for_prefab || top_level->creator_source->what_prefab != for_prefab))
		top_level = top_level->creator_source;
	
	if (!for_prefab) {
		ASSERT(!top_level->what_prefab || top_level->is_root_of_prefab);
	}
	if (!top_level->what_prefab || for_prefab == top_level->what_prefab) {
		auto source_owner_default = (const Entity*)top_level->get_type().default_class_object;
		ASSERT(top_level == obj);
		return source_owner_default;
	}
	else {
		ASSERT(top_level->what_prefab && top_level->what_prefab->sceneFile);

		auto& objs = top_level->what_prefab->sceneFile->get_objects();

		auto path = build_path_for_object(obj, top_level->what_prefab);
		out_prefab_diff = top_level->what_prefab;
		return objs.find(path)->second;

	}
}

void serialize_overrides_R(Entity* e, PrefabAsset* for_prefab, SerializedSceneFile& output, DictWriter& out, LevelSerializationContext* ctx)
{
	auto write_obj = [&](BaseUpdater* obj) {

		ASSERT(this_is_a_serializeable_object(obj, for_prefab));


		auto objpath = build_path_for_object(obj, for_prefab);
		out.write_item_start();
		out.write_key_value("override", objpath.c_str());

		ctx->diffprefab = nullptr;
		auto diff = find_diff_class(obj,for_prefab,ctx->diffprefab);

		ctx->cur_obj = obj;
		write_just_props(obj, diff, out, ctx);
		ctx->cur_obj = nullptr;

		out.write_item_end();
	};

	if (!e->dont_serialize_or_edit) {
		write_obj(e);
		if (serialize_this_objects_children(e,for_prefab)) {
			for (auto comp : e->get_components())
				if (!comp->dont_serialize_or_edit)
					write_obj(comp);
			for (auto o : e->get_children())
				serialize_overrides_R(o, for_prefab, output, out, ctx);
		}
	}
}

SerializedSceneFile serialize_entities_to_text(const std::vector<Entity*>& input_objs, PrefabAsset* for_prefab)
{
	
	SerializedSceneFile output;
	DictWriter out;

	auto add_to_extern_parents = [&](const BaseUpdater* obj, const BaseUpdater* parent) {
		SerializedSceneFile::external_parent ext;
		ext.child_path = build_path_for_object(obj, for_prefab);
		ext.external_parent_handle = parent->get_instance_id();
		output.extern_parents.push_back(ext);
	};

	// write out top level objects (no parent in serialize set)


	auto roots = root_objects_to_write(input_objs);

	// last chance to crash out before writing a bad file
	validate_serialize_input(roots, for_prefab);

	for (auto obj : roots)
	{
		auto ent = obj->cast_to<Entity>();
		ASSERT(ent);
		ASSERT(!ent->dont_serialize_or_edit);
		auto root_parent = ent->get_parent();
		if (root_parent)
				add_to_extern_parents(obj, root_parent);
			
		serialize_new_object_text_R(ent, out, root_parent != nullptr /* dont_write_parent if root_parent exists*/, for_prefab, output);
	}


	
	LevelSerializationContext ctx;
	ctx.out = &output;
	ctx.for_prefab = for_prefab;

	for (auto inobj : roots) {
		// serialize overrides

		serialize_overrides_R(inobj,for_prefab, output, out, &ctx);
	}

	output.text = std::move(out.get_output());

	return output;
}




void split_path_c(const char* path, char** components, char* buffer, int& buffer_ofs, int& count, int num_components, int buffer_size) {
    count = 0;
    const char* start = path;
    while (*path) {
        if (*path == '/') {
			if (count >= num_components)
				throw std::runtime_error("split_path_c out of components");
			if ((path - start) + 1 + buffer_ofs >= buffer_size)
				throw std::runtime_error("split_path_c out of buffer");
			auto len = path - start;
			strncpy(buffer + buffer_ofs, start, len);
			buffer[buffer_ofs + len] = 0;
			*components++ = &buffer[buffer_ofs];
			buffer_ofs +=len + 1;
            start = path + 1;
            count++;
        }
        path++;
    }
    if (start != path) {
		if (count >= num_components)
			throw std::runtime_error("split_path_c out of components");
		if ((path - start) + 1 + buffer_ofs >= buffer_size)
			throw std::runtime_error("split_path_c out of buffer");
		auto len = (path - start);
		strncpy(buffer+buffer_ofs, start,len);
		buffer[buffer_ofs + len] = 0;
		*components++ = &buffer[buffer_ofs];
		buffer_ofs += len + 1;
		count++;
    }
}


std::string serialize_build_relative_path(const char* from, const char* to)
{
	const int MAX_COMPONENTS = 10;
	static char buffer[256];
    static char* from_components[MAX_COMPONENTS];
    static char* to_componenets[MAX_COMPONENTS];
    int from_count=0, to_count=0;

	int buffer_ofs = 0;
    split_path_c(from, from_components, buffer,buffer_ofs, from_count,MAX_COMPONENTS,256);
    split_path_c(to, to_componenets, buffer,buffer_ofs, to_count,MAX_COMPONENTS,256);

    int common_length = 0;
    while (common_length < from_count && common_length < to_count && strcmp(from_components[common_length], to_componenets[common_length]) == 0) {
        common_length++;
    }

    static char relativePath[1024];
    char* p = relativePath;

    for (int i = common_length; i < from_count; ++i) {
        if (i > common_length) {
            *p++ = '/';
        }
        strcpy(p, "..");
        p += 2;
    }

    for (int i = common_length; i < to_count; ++i) {
        if (p != relativePath) {
            *p++ = '/';
        }
        strcpy(p, to_componenets[i]);
        p += strlen(to_componenets[i]);
    }

    *p = '\0';

    return relativePath;
}
std::string unserialize_relative_to_absolute(const char* relative, const char* root)
{
	const int MAX_COMPONENTS = 10;
	static char buffer[256];
	static char* relative_components[MAX_COMPONENTS];
	static char* root_componenents[MAX_COMPONENTS];
	int rel_count = 0, root_count = 0;

	char* buffer_ptr = buffer;
	int buffer_ofs = 0;
    split_path_c(relative, relative_components, buffer,buffer_ofs, rel_count,MAX_COMPONENTS,256);
    split_path_c(root, root_componenents, buffer,buffer_ofs, root_count,MAX_COMPONENTS,256);

	for (int i = 0; i < rel_count; i++) {
		if (strcmp(relative_components[i], "..") == 0) {
			if (root_count > 0)
				root_count--;
		}
		else {
			root_componenents[root_count++] = relative_components[i];
		}
	}


	static char absolutePath[1024];
	char* p = absolutePath;
	for (int i = 0; i < root_count; i++) {
		if (i > 0) {
			*p++ = '/';
		}
		strcpy(p, root_componenents[i]);
        p += strlen(root_componenents[i]);
	}

    *p = '\0';

    return absolutePath;
}
