#include "SerializeNew.h"
#include "AssetCompile/Someutils.h"
#include <stdexcept>
#include "Assets/AssetDatabase.h"
#include <unordered_map>
#include <unordered_set>
#include <stdexcept>
#include "Level.h"
#include "Game/Entity.h"
#include "Game/EntityComponent.h"
#include "Game/LevelAssets.h"
#include "GameEnginePublic.h"
#include "Framework/StringUtils.h"
#include "Framework/Log.h"
#include "Framework/ReflectionProp.h"
#include "Framework/DictParser.h"
#include "Framework/ClassBase.h"
#include "Framework/Util.h"
#include "Framework/DictWriter.h"
#include "Framework/ReflectionProp.h"
#include "Framework/SerializedForDiffing.h"
#include "Framework/SerializerJson2.h"
#include "Framework/SerializerBinary.h"

#include <json.hpp>





using std::to_string;


template<typename T>
T* cast_to(ClassBase* ptr) {
	return ptr ?  ptr->cast_to<T>() : nullptr;
}

// factory method for components
// factory method for prefabs
// both defined in lua
UnserializedSceneFile NewSerialization::unserialize_from_json(const char* debug_tag, SerializedForDiffing& json, IAssetLoadingInterface& load, bool keepid)
{
	UnserializedSceneFile outfile;
	auto& obj = json.jsonObj;
	if(obj["objs"].is_array())
	{
		auto& objarr = obj["objs"];
		for (auto& ent : objarr) {
			string type = ent["__typename"];
			Entity* e = new Entity;
			Component* c = ClassBase::create_class<Component>(type.c_str());
			ASSERT(c);
			e->add_component_from_unserialization(c);
			{
				ReadSerializerBackendJson2 reader("", ent, load, *e);
			}
			{
				ReadSerializerBackendJson2 reader("", ent, load, *c);
			}
			if (keepid&&ent["__retid"].is_number()) {
				int64_t instid = ent["__retid"];
				e->post_unserialization(instid);
			}
			outfile.all_obj_vec.push_back(e);
			outfile.all_obj_vec.push_back(c);
		}
		return outfile;
	}
	throw std::runtime_error("invalid json");
}
using std::vector;
UnserializedSceneFile NewSerialization::unserialize_from_text(const char* debug_tag, const std::string& text, IAssetLoadingInterface& load, bool keepid)
{

	SerializedForDiffing sfd;
	sfd.jsonObj = nlohmann::json::parse(text);
	return unserialize_from_json(debug_tag, sfd, load, keepid);
}


// what it expects: list of entities that are serializable. cant be non serialized objects
// cant be a subobject of a prefab, has to be the prefab root
// will check if prefab object is actually part of the template. this handles cases where the editor screwed up
// for example, a missing object in the prefab. or an object that isnt actually in the prefab. (remember, nodes from instanced prefabs cant be deleted)
// also checks for fully unique ids.



SerializedSceneFile NewSerialization::serialize_to_text(const char* debug_tag, const std::vector<Entity*>& input_objs, bool write_ids)
{
	double now = GetTime();

		nlohmann::json obj;
		obj["objs"] = nlohmann::json::array();
		for (auto ent : input_objs) {
			if (ent->get_components().size() == 0) 
				continue;
			if (ent->dont_serialize_or_edit)
				continue;
			if (ent->get_parent())
				continue;

			nlohmann::json out;
			{
				WriteSerializerBackendJson2 writer("", *ent);
				if (writer.get_output().is_object())
					out.update(writer.get_output());
			}
			auto c = ent->get_components().at(0);
			{
				WriteSerializerBackendJson2 writer("", *c);
				if(writer.get_output().is_object())
					out.update(writer.get_output());
			}
			out["__typename"] = c->get_type().classname;
			if(write_ids)
				out["__retid"] = ent->get_instance_id();
			obj["objs"].push_back(out);
			//printf("%s\n", obj.dump(1).c_str());
		}
		SerializedSceneFile outfile;
		outfile.text = "!json\n" + obj.dump(1);


	sys_print(Debug, "NewSerialization::serialize_to_text: took %f\n", float(GetTime() - now));
	//std::cout << out.text << '\n';
	now = GetTime();


	return outfile;
}


class UnserializationWrapper;
Entity* unserialize_entities_from_text_internal(UnserializedSceneFile& scene, const std::string& text, const std::string& rootpath,
	PrefabAsset* prefab, Entity* starting_root, IAssetLoadingInterface* load);


void UnserializedSceneFile::delete_objs()
{
	for (auto& o : all_obj_vec)
		delete o;
	all_obj_vec.clear();
}



UnserializedSceneFile unserialize_entities_from_text(const char* debug_tag, const std::string& text, IAssetLoadingInterface* load, bool keepid)
{
	if (!load)
		load = AssetDatabase::loader;
	if (StringUtils::starts_with(text, "!json\n")) {
		auto fixedtext = text.substr(5);
		return NewSerialization::unserialize_from_text(debug_tag, fixedtext, *load, keepid);
	}
	else {
		sys_print(Error, "unserialize_entities_from_text: old format not supported\n");
		return UnserializedSceneFile();
	}
}


// TODO prefabs
// rules:
// * path based on source



bool serialize_this_objects_children(const Entity* b)
{
	if (b->dont_serialize_or_edit)
		return false;
	return true;
}

bool this_is_a_serializeable_object(const BaseUpdater* b)
{
	assert(b);
	if (b->dont_serialize_or_edit)
		return false;

	if (auto as_comp = b->cast_to<Component>()) {
		assert(as_comp->get_owner());
		if (!serialize_this_objects_children(as_comp->get_owner()))
			return false;
	}
	return true;
}
