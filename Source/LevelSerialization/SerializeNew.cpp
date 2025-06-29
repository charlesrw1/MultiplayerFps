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


MakePathForObjectNew::MakePathForObjectNew(PrefabAsset* opt_prefab)
	:for_prefab(opt_prefab)
{
}

MakeObjectForPathNew::MakeObjectForPathNew(IAssetLoadingInterface& load, UnserializedSceneFile& out, PrefabAsset* for_prefab)
	: load(load), out(out), prefab(for_prefab) {}


using std::to_string;

string build_path_for_object_new(const BaseUpdater& obj)
{
	const Entity* outer = PrefabToolsUtil::get_outer_prefab(obj);
	if (outer) {
		return build_path_for_object_new(*outer) + "/" + to_string(obj.unique_file_id);
	}
	return to_string(obj.unique_file_id);
}

IMakePathForObject::MakePath MakePathForObjectNew::make_path(const ClassBase* to)
{
	const BaseUpdater* bu = to->cast_to<BaseUpdater>();
	if (!bu)
		return "x" + to_string((uintptr_t)to);
	//throw std::runtime_error("make_path request for non baseupdater");
	MakePath mp(build_path_for_object_new(*bu));
	if (!PrefabToolsUtil::is_newly_created(*bu,for_prefab))
		mp.is_subobject = true;
	return mp;
}

string MakePathForObjectNew::make_type_name(ClassBase* obj)
{
	const BaseUpdater* bu = obj->cast_to<BaseUpdater>();
	if (!bu)
		return obj->get_type().classname;
	const bool is_prefab = PrefabToolsUtil::is_part_of_a_prefab(*bu);
	const Entity* as_entity = bu->cast_to<Entity>();
	const bool is_nested_prefab = bu->what_prefab != for_prefab;
	if (is_prefab && as_entity && is_nested_prefab && PrefabToolsUtil::is_this_the_root_of_the_prefab(*as_entity)) {
		return as_entity->what_prefab->get_name();
	}
	return bu->get_type().classname;
}
nlohmann::json* MakePathForObjectNew::find_diff_for_obj(ClassBase* obj)
{
	return nullptr;
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
		if (pfb && !pfb->did_load_fail()) {
			sys_print(Debug, "MakeObjectForPathNew::create_from_name(%s): instancing prefab\n",s.get_debug_tag());

			string debug_tag = s.get_debug_tag() + string("/") + pfb->get_name();
			UnserializedSceneFile out = unserialize_entities_from_text(debug_tag.c_str(),pfb->text, &load, pfb);
			//out.get_
			Entity* root_of_prefab = out.get_root_entity();
			if (!root_of_prefab) {
				sys_print(Warning, "MakeObjectForPathNew::create_from_name(%s): instanced prefab didnt have root?\n",s.get_debug_tag());
				out.delete_objs();
			}
			else {
				unordered_map<string,BaseUpdater*>& objs = out.get_objects();
				for (auto& [objpath,objptr] : objs) {
					if (objptr == root_of_prefab) {
						continue;
					}
					string str = "";
					auto find = objpath.find("/");
					if (find == string::npos) {
						sys_print(Warning, "MakeObjectForPathNew::create_from_name(%s): sub object missing '/' (%s)\n", s.get_debug_tag(), objpath.c_str());
						str = objpath;
					}
					else {
						str = objpath.substr(find + 1);
					}
					s.insert_nested_object(parent_path + "/" + str, objptr);
				}
				objs.clear();

				root_of_prefab->set_nested_owner_prefab(this->prefab);
				root_of_prefab->unique_file_id = parse_fileid(parent_path);

				assert(root_of_prefab->what_prefab == pfb);	// should have been set in unserialize
			}
			return root_of_prefab;
		}
		else {
			sys_print(Error, "MakeObjectForPathNew::create_from_name(%s): prefab load failed (%s)\n",s.get_debug_tag(), str.c_str());
			return nullptr;
		}
		return nullptr;
	}
	else {
		return ClassBase::create_class<ClassBase>(str.c_str());
	}
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
static void set_object_vars(BaseUpdater& e, std::string path, Entity* opt_source_owner, PrefabAsset* opt_prefab) {
	e.unique_file_id = parse_fileid(path);
	e.creator_source = opt_source_owner;
	auto ent = e.cast_to<Entity>();
	if (ent && ent->what_prefab && !ent->get_parent()) {
		ent->set_nested_owner_prefab(opt_prefab);
	}
	else
		e.what_prefab = opt_prefab;

	//e->owner_asset = opt_prefab;
}
// level=multiple roots, prefab=single root
// World
//	Level0/
//		Object
//		Prefab
//			Object
//		
//	Level1/
//		

using std::vector;
UnserializedSceneFile NewSerialization::unserialize_from_text(const char* debug_tag, const std::string& text, IAssetLoadingInterface& load, PrefabAsset* opt_source_prefab)
{
	//printf("loading prefab\n");
	UnserializedSceneFile outfile;
	MakeObjectForPathNew objmaker(load,outfile,opt_source_prefab);
	ReadSerializerBackendJson reader(debug_tag, text, objmaker,load);
	SerializeEntitiesContainer* rootobj = reader.get_root_obj()->cast_to<SerializeEntitiesContainer>();

	assert(rootobj);	//fixme
	
	vector<Entity*> roots;
	for (auto obj : rootobj->objects) {
		if (!obj) {
			sys_print(Warning, "unserialize_from_text(%s): obj is null\n", debug_tag);
			continue;
		}
		auto path = reader.get_path_for_object(*obj);
		if (!path) {
			sys_print(Warning, "unserialize_from_text(%s): no path\n", debug_tag);
			continue;
		}

		if (!obj->what_prefab) {
			obj->unique_file_id = parse_fileid(*path);
			obj->what_prefab = opt_source_prefab;
		}

		auto e = obj->cast_to<Entity>();
		if (e&&!e->get_parent()) {
			roots.push_back(e);
		}
	}
	for (auto& [path, obj] : reader.path_to_objs) {
		auto bu = obj->cast_to<BaseUpdater>();
		if(bu)
			outfile.get_objects().insert({ path,bu });
	}

	if (!roots.empty()) {
		outfile.set_root_entity(roots[0]);
		if (roots.size() > 1)
			sys_print(Warning, "unserialize_from_text(%s): more than 1 root (found %d)\n", debug_tag, (int)roots.size());
	}
	else {
		sys_print(Warning, "unserialize_from_text(%s): found no roots\n", debug_tag);
	}
	//printf("finished prefab\n");

	delete rootobj;
	return std::move(outfile);
}

void NewSerialization::add_objects_to_write(const char* debug_tag, SerializeEntitiesContainer& con, Entity& e, PrefabAsset* for_prefab)
{
	if (e.dont_serialize_or_edit)
		return;

	SetUtil::insert_test_exists(con.objects, (BaseUpdater*)&e);

	if (serialize_this_objects_children(&e, for_prefab)) {
		const auto& all_comps = e.get_components();
		for (auto c : all_comps) {
			if (!c) {
				sys_print(Warning, "add_objects_to_write(%s): component was null in object (id=%lld)\n", debug_tag, e.get_instance_id());
				continue;
			}

			if (!c->dont_serialize_or_edit)//&& this_is_newly_created(c, for_prefab))
				SetUtil::insert_test_exists(con.objects, (BaseUpdater*)c);
		}
		auto& children = e.get_children();
		for (auto child : children) {
			if (!child) {
				sys_print(Warning, "add_objects_to_write(%s): object had null child (id=%lld)\n", debug_tag, e.get_instance_id());
				continue;
			}
			add_objects_to_write(debug_tag,con,*child,for_prefab);
		}
	}
}

void add_to_extern_parents_new(const BaseUpdater* obj, const BaseUpdater* parent, const PrefabAsset* for_prefab, SerializedSceneFile& output)
{
	SerializedSceneFile::external_parent ext;
	ext.child_path = build_path_for_object_new(*obj);
	ext.external_parent_handle = parent->get_instance_id();
	output.extern_parents.push_back(ext);
}
void NewSerialization::add_objects_to_container(const char* debug_tag, const std::vector<Entity*>& input_objs, SerializeEntitiesContainer& container, PrefabAsset* for_prefab, SerializedSceneFile& output)
{
	auto roots = root_objects_to_write(input_objs);
	for (auto r : roots) {
		if (r->get_parent())
			add_to_extern_parents_new(r, r->get_parent(), for_prefab, output);
		add_objects_to_write(debug_tag,container, *r, for_prefab);
	}
}

static void add_paths_to_put_back(Entity& e, MakePathForObjectNew& pathmaker, unordered_map<string, uint64_t>& path_to_handle)
{
	if (e.unique_file_id == 0)
		return;

	string makepath = pathmaker.make_path(&e).path;
	MapUtil::insert_test_exists(path_to_handle, makepath, e.get_instance_id());
	for (auto child : e.get_children())
		add_paths_to_put_back(*child, pathmaker, path_to_handle);
	for (auto c : e.get_components()) {
		if (c->unique_file_id == 0)
			continue;
		string cmakepath = pathmaker.make_path(c).path;
		MapUtil::insert_test_exists(path_to_handle, cmakepath, c->get_instance_id());
	}
}
static void add_paths_from_container(const std::vector<Entity*>& input_objs, MakePathForObjectNew& pathmaker, unordered_map<string,uint64_t>& path_to_handle)
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


SerializedSceneFile NewSerialization::serialize_to_text(const char* debug_tag, const std::vector<Entity*>& input_objs, PrefabAsset* opt_prefab)
{
	SerializedSceneFile out;
	SerializeEntitiesContainer container;
	MakePathForObjectNew pathmaker(opt_prefab);
	add_objects_to_container(debug_tag,input_objs, container, opt_prefab, out);
	WriteSerializerBackendJson writer(debug_tag, pathmaker,container);
	add_paths_from_container(input_objs, pathmaker, out.path_to_instance_handle);
	out.text = "!json\n"+writer.get_output().dump(1);
	//std::cout << out.text << '\n';
	return out;
}
