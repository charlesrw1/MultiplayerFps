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


std::string MakePathForObjectNew::make_path(const ClassBase* to)
{
	auto bu = to->cast_to<BaseUpdater>();
	if (!bu)
		return "x" + std::to_string((uintptr_t)to);
	//throw std::runtime_error("make_path request for non baseupdater");
	return build_path_for_object(bu, nullptr);
}

std::string MakePathForObjectNew::make_type_name(ClassBase* obj)
{
	auto bu = obj->cast_to<BaseUpdater>();
	if (!bu)
		return obj->get_type().classname;
	//throw std::runtime_error("make_type_name request for non baseupdater");
	return get_type_for_new_serialized_item(bu, nullptr);
}
nlohmann::json* MakePathForObjectNew::find_diff_for_obj(ClassBase* obj)
{
	return nullptr;
}

template<typename T>
T* cast_to(ClassBase* ptr) {
	return ptr ?  ptr->cast_to<T>() : nullptr;
}

ClassBase* MakeObjectForPathNew::create_from_name(ReadSerializerBackendJson& s, const std::string& str, const string& parent_path)
{
	//assert(0);
	auto ext = StringUtils::get_extension(str);
	if (ext == ".pfb") {
		return nullptr;
		PrefabAsset* pfb = cast_to<PrefabAsset>(load.load_asset(&PrefabAsset::StaticType, str));
		if (pfb && !pfb->did_load_fail()) {
			UnserializedSceneFile out = unserialize_entities_from_text(prefab->text, &load, pfb);
			//out.get_
			Entity* root_of_prefab = out.get_root_entity();
			if (!root_of_prefab) {
				LOG_WARN("prefab didnt have root?");
				out.delete_objs();
			}
			else {
				unordered_map<string,BaseUpdater*>& objs = out.get_objects();
				for (auto o : objs) {
					if (o.second != root_of_prefab) {
						string thepath = parent_path + "/" + o.first;
						s.insert_nested_object(thepath, o.second);
					}
				}
				objs.clear();
			}
			return root_of_prefab;
		}
		else {
			LOG_WARN("prefab load failed");
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
				LOG_WARN("null unserialized class");
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
static void set_object_vars(BaseUpdater& e, std::string path, Entity* opt_source_owner, IAsset* opt_prefab) {
	e.unique_file_id = parse_fileid(path);
	e.creator_source = opt_source_owner;
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
UnserializedSceneFile NewSerialization::unserialize_from_text(const std::string& text, IAssetLoadingInterface& load, PrefabAsset* opt_source_prefab)
{
	UnserializedSceneFile outfile;
	MakeObjectForPathNew objmaker(load,outfile,opt_source_prefab);
	ReadSerializerBackendJson reader(text, objmaker,load);
	SerializeEntitiesContainer* rootobj = reader.get_root_obj()->cast_to<SerializeEntitiesContainer>();

	assert(rootobj);	//fixme
	
	vector<Entity*> roots;
	for (auto obj : rootobj->objects) {
		if (!obj) {
			LOG_WARN("no obj");
			continue;
		}
		auto path = reader.get_path_for_object(*obj);
		if (!path) {
			LOG_WARN("no path");
			continue;
		}

		set_object_vars(*obj, *path, nullptr, opt_source_prefab);
		outfile.get_objects().insert({ *path,obj });
		auto e = obj->cast_to<Entity>();
		if (e&&!e->get_parent()) {
			roots.push_back(e);
		}
	}
	if (!roots.empty()) {
		outfile.set_root_entity(roots[0]);
		if (roots.size() > 1)
			LOG_WARN("more than 1 root");
	}
	else
		LOG_WARN("no roots");

	return std::move(outfile);
}

void NewSerialization::add_objects_to_write(SerializeEntitiesContainer& con, Entity& e, PrefabAsset* for_prefab)
{
	if (e.dont_serialize_or_edit)
		return;

	SetUtil::insert_test_exists(con.objects, (BaseUpdater*)&e);

	if (serialize_this_objects_children(&e, for_prefab)) {
		const auto& all_comps = e.get_components();
		for (auto c : all_comps) {
			if (!c) {
				LOG_WARN("null component");
				continue;
			}

			if (!c->dont_serialize_or_edit)//&& this_is_newly_created(c, for_prefab))
				SetUtil::insert_test_exists(con.objects, (BaseUpdater*)c);
		}
		auto& children = e.get_children();
		for (auto child : children) {
			if (!child) {
				LOG_WARN("null child");
				continue;
			}
			add_objects_to_write(con,*child,for_prefab);
		}
	}
}
void NewSerialization::add_objects_to_container(const std::vector<Entity*>& input_objs, SerializeEntitiesContainer& container, PrefabAsset* for_prefab, SerializedSceneFile& output)
{
	auto roots = root_objects_to_write(input_objs);
	for (auto r : roots) {
		if (r->get_parent())
			add_to_extern_parents(r, r->get_parent(), for_prefab, output);
		add_objects_to_write(container, *r, for_prefab);
	}
}

static void add_paths_from_container(const std::unordered_set<BaseUpdater*>& objects, MakePathForObjectNew& pathmaker, unordered_map<string,uint64_t>& path_to_handle)
{
	for (auto o : objects) {
		if (o) {
			auto makepath = pathmaker.make_path(o);
			MapUtil::insert_test_exists(path_to_handle, makepath, o->get_instance_id());
		}
	}
}

SerializedSceneFile NewSerialization::serialize_to_text(const std::vector<Entity*>& input_objs, PrefabAsset* opt_prefab)
{
	SerializedSceneFile out;
	SerializeEntitiesContainer container;
	MakePathForObjectNew pathmaker(opt_prefab);
	add_objects_to_container(input_objs, container, opt_prefab, out);
	WriteSerializerBackendJson writer(pathmaker,container);
	add_paths_from_container(container.objects, pathmaker, out.path_to_instance_handle);
	out.text = "!json\n"+writer.get_output().dump(1);
	//std::cout << out.text << '\n';
	return out;
}
