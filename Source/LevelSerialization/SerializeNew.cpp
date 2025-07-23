#include "SerializeNew.h"
#include "Framework/SerializerJson.h"
#include "Framework/StringUtils.h"
#include "Game/BaseUpdater.h"
#include "Game/Entity.h"
#include "Game/EntityComponent.h"
#include "Game/LevelAssets.h"
#include "Assets/AssetDatabase.h"
#include "Framework/MapUtil.h"

#include "SerializeNewMakers.h"
#include "Framework/Log.h"
#include "SerializationAPI.h"


MakePathForObjectNew::MakePathForObjectNew()
{
}

MakeObjectForPathNew::MakeObjectForPathNew(IAssetLoadingInterface& load, UnserializedSceneFile& out)
	: load(load), out(out) {}


using std::to_string;

string build_path_for_object_new(const BaseUpdater& obj)
{
	//const Entity* outer = PrefabToolsUtil::get_outer_prefab(obj);
	//if (outer) {
	//	return build_path_for_object_new(*outer) + "/" + to_string(obj.unique_file_id);
	//}
	return to_string(obj.unique_file_id);
}


IMakePathForObject::MakePath MakePathForObjectNew::make_path(const ClassBase* to)
{
	const BaseUpdater* bu = to->cast_to<BaseUpdater>();
	if (!bu)
		return "x" + to_string((uintptr_t)to);
	//throw std::runtime_error("make_path request for non baseupdater");
	MakePath mp(build_path_for_object_new(*bu));
	//if (!PrefabToolsUtil::is_newly_created(*bu,for_prefab))
	//	mp.is_subobject = true;
	return mp;
}

string MakePathForObjectNew::make_type_name(ClassBase* obj)
{
#ifndef EDITOR_BUILD
	ASSERT(0);
	return "";
#else
	const BaseUpdater* bu = obj->cast_to<BaseUpdater>();
	if (!bu)
		return obj->get_type().classname;
	if (const Entity* as_ent = bu->cast_to<Entity>()) {
		if (as_ent->get_object_prefab_spawn_type()==EntityPrefabSpawnType::RootOfPrefab) {
			const PrefabAsset& pfb = as_ent->get_object_prefab();
			return pfb.get_name();
		}
		
	}

	return bu->get_type().classname;
#endif
}
#include "Framework/SerializedForDiffing.h"
nlohmann::json* MakePathForObjectNew::find_diff_for_obj(ClassBase* obj)
{
	if (obj->get_type().diff_data) {
		return &obj->get_type().diff_data->jsonObj;
	}
	else {
		return nullptr;
	}
}

template<typename T>
T* cast_to(ClassBase* ptr) {
	return ptr ?  ptr->cast_to<T>() : nullptr;
}

ClassBase* MakeObjectForPathNew::create_from_name(ReadSerializerBackendJson& s, const string& str, const string& parent_path)
{

	//assert(0);
	auto ext = StringUtils::get_extension(str);
	if (ext == ".pfb") {
		PrefabAsset* pfb = cast_to<PrefabAsset>(load.load_asset(&PrefabAsset::StaticType, str));
		
		// this shouldnt happen ever but...
		if (!pfb) {
			sys_print(Error, "MakeObjectForPathNew::create_from_name: %s didnt even return ptr?\n", str.c_str());
			return nullptr;
		}

		auto set_pfb_root_vars = [&](Entity& root) {
			root.set_root_object_prefab(*pfb);
			//root.unique_file_id = parse_fileid(parent_path);
		};

		auto make_fake_pfb_root = [&]() -> Entity* {
			sys_print(Debug, "make_fake_pfb_root\n");
			Entity* root = new Entity();
			set_pfb_root_vars(*root);
			assert(root->get_object_prefab_spawn_type() == EntityPrefabSpawnType::RootOfPrefab);
			return root;

		};
		if (pfb && !pfb->did_load_fail()) {
			sys_print(Debug, "MakeObjectForPathNew::create_from_name(%s): instancing prefab\n",s.get_debug_tag());
			return make_fake_pfb_root();
		}
		else {
			sys_print(Error, "MakeObjectForPathNew::create_from_name(%s): prefab load failed (%s)\n",s.get_debug_tag(), str.c_str());
			
			return make_fake_pfb_root();
		}
	}
	else {
		return ClassBase::create_class<ClassBase>(str.c_str());
	}

	return nullptr;
}

void SerializeEntitiesContainer::serialize(Serializer& s)
{
	int sz = objects.size();
	s.serialize_array("objects", sz);
	if (s.is_saving()) {
		for (auto o : objects) {
			s.serialize_class_ar(o);
		}
	}
	else {
		for (int i = 0; i < sz; i++) {
			ClassBase* c = nullptr;
			s.serialize_class_ar(c);
			if (c) {
				SetUtil::insert_test_exists(objects, (BaseUpdater*)c);
			}
			else {
				sys_print(Warning, "SerializeEntitiesContainer::serialize: null unserialized class in (%s)\n", s.get_debug_tag());
			}
		}
	}
	//if (s.is_loading())
	//	objects.resize(sz);
	//for (int i = 0; i < sz; i++) {
	//	s.serialize_class_ar(objects.at(i));
	//}
	s.end_obj();
}

// factory method for components
// factory method for prefabs
// both defined in lua

#include "Framework/MapUtil.h"
void NewSerialization::unserialize_shared(const char* debug_tag, UnserializedSceneFile& outfile, ReadSerializerBackendJson& reader)
{
	SerializeEntitiesContainer* rootobj = reader.get_root_obj()->cast_to<SerializeEntitiesContainer>();

	assert(rootobj);	//fixme
	
	vector<Entity*> roots;
	for (BaseUpdater* obj : rootobj->objects) {
		if (!obj) {
			sys_print(Warning, "unserialize_from_text(%s): obj is null\n", debug_tag);
			continue;
		}
		const string* path = reader.get_path_for_object(*obj);
		if (!path) {
			sys_print(Warning, "unserialize_from_text(%s): no path\n", debug_tag);
			continue;
		}
		if (auto as_ec = obj->cast_to<Component>()) {
			if (!as_ec->get_owner()) {
				sys_print(Warning, "unserialize_from_text(%s): component (%s) wasnt parented, deleting it (%s)\n", debug_tag,as_ec->get_type().classname,path->c_str());
				reader.path_to_objs.erase(*path);
				delete as_ec;
				continue;
			}
		}
		//assert(obj->unique_file_id == 0);
		//assert(obj->)
		//assert(this_is_a_serializeable_object(obj));
		//if (this_is_a_serializeable_object(obj))
			obj->unique_file_id = parse_fileid(*path);
			MapUtil::insert_test_exists(outfile.file_id_to_obj,obj->unique_file_id, obj);
			outfile.all_obj_vec.push_back(obj);
		//else
		//	obj->unique_file_id = 0;

		Entity* e = obj->cast_to<Entity>();
		if (e && !e->get_parent()) {
			roots.push_back(e);
		}
	}
	for (auto& obj : outfile.all_obj_vec) {
		if (!this_is_a_serializeable_object(obj))
			obj->unique_file_id = 0;
	}
	std::unordered_map<int, BaseUpdater*> fileIds;
	for (auto& obj : outfile.all_obj_vec) {
		if (obj->unique_file_id) {
			MapUtil::insert_test_exists(fileIds, obj->unique_file_id, obj);
		}
	}


#ifdef _DEBUG
	std::unordered_set<BaseUpdater*> inSet;
	for (auto obj : outfile.all_obj_vec) 
		SetUtil::insert_test_exists(inSet,obj);
	auto check_that_objects_are_added_R = [](auto&& self, std::unordered_set<BaseUpdater*>& inSet,Entity* e) -> void {
		assert(SetUtil::contains(inSet, (BaseUpdater*)e));
		for (auto c : e->get_children()) {
			self(self, inSet, c);
		}
		for (auto c : e->get_components()) {
			assert(SetUtil::contains(inSet, (BaseUpdater*)c));
		}
	};
	for (auto obj : outfile.all_obj_vec) {
		if (auto as_ent = obj->cast_to<Entity>()) {
			if (!as_ent->get_parent())
				check_that_objects_are_added_R(check_that_objects_are_added_R, inSet, as_ent);
		}
	}
#endif

	outfile.num_roots = (int)roots.size();

	if (!roots.empty()) {
		outfile.set_root_entity(roots[0]);
	}
	else {
		sys_print(Warning, "unserialize_from_text(%s): found no roots\n", debug_tag);
	}

	delete rootobj;
}
UnserializedSceneFile NewSerialization::unserialize_from_json(const char* debug_tag, SerializedForDiffing& json, IAssetLoadingInterface& load)
{
	UnserializedSceneFile outfile;
	MakeObjectForPathNew objmaker(load, outfile);
	ReadSerializerBackendJson reader(debug_tag, json.jsonObj, objmaker, load);
	unserialize_shared(debug_tag, outfile, reader);
	return std::move(outfile);
}
using std::vector;
UnserializedSceneFile NewSerialization::unserialize_from_text(const char* debug_tag, const std::string& text, IAssetLoadingInterface& load)
{
	//printf("loading prefab\n");
	UnserializedSceneFile outfile;
	MakeObjectForPathNew objmaker(load,outfile);
	ReadSerializerBackendJson reader(debug_tag, text, objmaker,load);
	unserialize_shared(debug_tag, outfile, reader);
	return std::move(outfile);
}

void NewSerialization::add_objects_to_write(const char* debug_tag, SerializeEntitiesContainer& con, Entity& e)
{
	if (e.dont_serialize_or_edit)
		return;
	if (!this_is_a_serializeable_object(&e))
		return;

	auto insert_into_things = [&](BaseUpdater& self) {
		if (self.unique_file_id == 0)
			throw SerializeInputError("add_objects_to_write:  BaseUpdater had null file_id (inst_id=" + std::to_string(e.get_instance_id()) + ")");
		SetUtil::insert_test_exists(con.objects, &self);
	};
	insert_into_things(e);
	const auto& all_comps = e.get_components();
	for (auto c : all_comps) {
		if (!c) {
			sys_print(Warning, "add_objects_to_write(%s): component was null in object (id=%lld)\n", debug_tag, e.get_instance_id());
			continue;
		}
		if (this_is_a_serializeable_object(c)) {
			insert_into_things(*c);
		}
	}
	auto& children = e.get_children();
	for (auto child : children) {
		if (!child) {
			sys_print(Warning, "add_objects_to_write(%s): object had null child (id=%lld)\n", debug_tag, e.get_instance_id());
			continue;
		}
		add_objects_to_write(debug_tag,con,*child);
	}
}

void add_to_extern_parents_new(const BaseUpdater* obj, const BaseUpdater* parent,SerializedSceneFile& output)
{
	SerializedSceneFile::external_parent ext;
	assert(obj->unique_file_id != 0);
	ext.child_id = obj->unique_file_id;// = build_path_for_object_new(*obj);
	ext.external_parent_handle = parent->get_instance_id();
	output.extern_parents.push_back(ext);
}

// this validates
void NewSerialization::add_objects_to_container(const char* debug_tag, const std::vector<Entity*>& input_objs, SerializeEntitiesContainer& container, SerializedSceneFile& output)
{
	auto roots = root_objects_to_write(input_objs);
	for (auto r : roots) {
		if (r->get_parent())
			add_to_extern_parents_new(r, r->get_parent(), output);
		add_objects_to_write(debug_tag,container, *r);
	}
}

static void add_paths_to_put_back(Entity& e, MakePathForObjectNew& pathmaker, unordered_map<int, uint64_t>& path_to_handle)
{
	if (!this_is_a_serializeable_object((BaseUpdater*)&e))
		return;
	if (e.unique_file_id == 0)
		return;

	int uniqueFileId = e.unique_file_id;
	MapUtil::insert_test_exists(path_to_handle, uniqueFileId, e.get_instance_id());
	for (auto child : e.get_children())
		add_paths_to_put_back(*child, pathmaker, path_to_handle);
	for (auto c : e.get_components()) {
		if (c->unique_file_id == 0)
			continue;
		if (!this_is_a_serializeable_object(c))
			return;
		int cUniqueFileId = c->unique_file_id;
		MapUtil::insert_test_exists(path_to_handle, cUniqueFileId, c->get_instance_id());
	}
}
static void add_paths_from_container(const std::vector<Entity*>& input_objs, MakePathForObjectNew& pathmaker, unordered_map<int,uint64_t>& path_to_handle)
{
	auto roots = root_objects_to_write(input_objs);
	for (auto o : roots) {
		if (o) {
			add_paths_to_put_back(*o,pathmaker,path_to_handle);
		}
	}
}

// what it expects: list of entities that are serializable. cant be non serialized objects
// cant be a subobject of a prefab, has to be the prefab root
// will check if prefab object is actually part of the template. this handles cases where the editor screwed up
// for example, a missing object in the prefab. or an object that isnt actually in the prefab. (remember, nodes from instanced prefabs cant be deleted)
// also checks for fully unique ids.

static void validate_container(SerializeEntitiesContainer& con, SerializedSceneFile& out) {

}

#include "Framework/SerializerBinary.h"
SerializedSceneFile NewSerialization::serialize_to_text(const char* debug_tag, const std::vector<Entity*>& input_objs)
{
	double now = GetTime();

	SerializedSceneFile out;
	SerializeEntitiesContainer container;
	add_objects_to_container(debug_tag,input_objs, container, out);
	MakePathForObjectNew pathmaker;
	add_paths_from_container(input_objs, pathmaker, out.path_to_instance_handle);

	validate_container(container, out);

	WriteSerializerBackendJson writer(debug_tag, pathmaker,container);
	out.text = "!json\n"+writer.get_output().dump(1);

	sys_print(Debug, "NewSerialization::serialize_to_text: took %f\n", float(GetTime() - now));
	//std::cout << out.text << '\n';
	now = GetTime();
	BinaryWriterBackend binWrite(container);
	sys_print(Debug, "NewSerialization::serialize_to_text binary: took %f\n", float(GetTime() - now));
	sys_print(Debug, "NewSerialization: bin size: %d\n", int(binWrite.writer.get_size()));

	return out;
}
