#include "SerializeNew.h"
#include "Framework/SerializerJson.h"
#include "Framework/StringUtils.h"
#include "Game/BaseUpdater.h"
#include "Game/Entity.h"
#include "Game/EntityComponent.h"
#include "Game/LevelAssets.h"
#include "Assets/AssetDatabase.h"

class MakePathForObjectNew : public IMakePathForObject
{
public:
	// Inherited via IMakePathForObject
	virtual std::string make_path(const ClassBase* to) override
	{
		auto bu = to->cast_to<BaseUpdater>();
		if (!bu)
			return "x" + std::to_string((uintptr_t)to);
			//throw std::runtime_error("make_path request for non baseupdater");
		return build_path_for_object(bu, nullptr);
	}
	virtual std::string make_type_name(ClassBase* obj) override
	{
		auto bu = obj->cast_to<BaseUpdater>();
		if (!bu)
			return obj->get_type().classname;
			//throw std::runtime_error("make_type_name request for non baseupdater");
		return get_type_for_new_serialized_item(bu, nullptr);
	}
	virtual ClassBase* create_from_name(Serializer& s, const std::string& str) override
	{
		//assert(0);
		auto ext= StringUtils::get_extension(str);
		if (ext == ".pfb") {
			//auto pfb = load->load_asset(&PrefabAsset::StaticType, str);
			//PrefabAsset* asset = pfb->cast_to<PrefabAsset>();
			//if (!asset)
			//	throw std::runtime_error("couldnt find scene file: " + str);

			//Entity* this_prefab_root = unserialize_entities_from_text_internal(*out, asset->text, root_path + id + "/", asset, inout_root_entity, load);
			//this_prefab_root->is_root_of_prefab = true;
			return nullptr;
		}
		else {
			return ClassBase::create_class<ClassBase>(str.c_str());
		}

	}
	virtual nlohmann::json* find_diff_for_obj(ClassBase* obj) override
	{
		return nullptr;
	}
	UnserializedSceneFile* out = nullptr;
	IAssetLoadingInterface* load= nullptr;
	IAsset* for_prefab = nullptr;
};
void SerializeEntitiesContainer::serialize(Serializer& s)
{
	int sz = objects.size();
	s.serialize_array("objects", sz);
	if (s.is_loading())
		objects.resize(sz);
	for (int i = 0; i < sz; i++) {
		s.serialize_class_ar(objects.at(i));
	}
	s.end_obj();
}
static void set_object_vars(BaseUpdater* e, std::string path, Entity* opt_source_owner, IAsset* opt_prefab) {
	e->unique_file_id = parse_fileid(path);
	e->creator_source = opt_source_owner;
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
UnserializedSceneFile NewSerialization::unserialize_from_text(const std::string& text, IAssetLoadingInterface* load, PrefabAsset* opt_source_prefab)
{
	UnserializedSceneFile outfile;
	MakePathForObjectNew pathmaker;
	pathmaker.load = load;
	pathmaker.out = &outfile;
	pathmaker.for_prefab = opt_source_prefab;
	ReadSerializerBackendJson reader(text, &pathmaker,load);
	SerializeEntitiesContainer* rootobj = reader.rootobj->cast_to<SerializeEntitiesContainer>();
	assert(rootobj);	//fixme
	
	vector<Entity*> roots;
	for (auto obj : rootobj->objects) {
		if (!obj)
			continue;
		auto path = reader.get_path_for_object(obj);
		set_object_vars(obj, path.value(), nullptr, opt_source_prefab);
		outfile.get_objects().insert({ path.value(),obj });
		auto e = obj->cast_to<Entity>();
		if (e&&!e->get_parent()) {
			roots.push_back(e);
		}
	}
	if (!roots.empty())
		outfile.set_root_entity(roots[0]);
	return std::move(outfile);
}

void NewSerialization::add_objects_to_write(SerializeEntitiesContainer& con, Entity* e,PrefabAsset* for_prefab)
{
	if (e->dont_serialize_or_edit)
		return;
	con.objects.push_back(e);
	if (serialize_this_objects_children(e, for_prefab)) {
		auto& all_comps = e->get_components();
		for (auto c : all_comps) {
			if (!c->dont_serialize_or_edit)//&& this_is_newly_created(c, for_prefab))
				con.objects.push_back(c);
		}
		auto& children = e->get_children();
		for (auto child : children)
			add_objects_to_write(con,child,for_prefab);
	}
}
void NewSerialization::add_objects_to_container(const std::vector<Entity*>& input_objs, SerializeEntitiesContainer& container, PrefabAsset* for_prefab, SerializedSceneFile& output)
{
	auto roots = root_objects_to_write(input_objs);
	for (auto r : roots) {
		if (r->get_parent())
			add_to_extern_parents(r, r->get_parent(), for_prefab, output);
		add_objects_to_write(container, r, for_prefab);
	}
}
#include <iostream>

SerializedSceneFile NewSerialization::serialize_to_text(const std::vector<Entity*>& input_objs, PrefabAsset* opt_prefab)
{
	double start = GetTime();
	SerializedSceneFile out;
	SerializeEntitiesContainer container;
	MakePathForObjectNew pathmaker;
	pathmaker.for_prefab = opt_prefab;
	add_objects_to_container(input_objs, container, opt_prefab, out);
	WriteSerializerBackendJson writer(&pathmaker,&container);
	double end = GetTime();

	out.text = "!json\n"+writer.obj.dump(1);
	std::cout << out.text << '\n';
	for (auto obj : container.objects) {
		out.path_to_instance_handle.insert({ pathmaker.make_path(obj),obj->get_instance_id() });
	}

	//NewSerialization::unserialize_from_text(out.text, nullptr, opt_prefab);

	double end2 = GetTime();
	printf("%f %f\n", (end - start), (end2 - end));
	return out;
}

// serialize a full scene 
// serialize just a prefab
// serialize 1 component
// 