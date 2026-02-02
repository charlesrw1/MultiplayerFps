#include "SerializeNew.h"
#include "Framework/SerializerJson.h"
#include "Framework/StringUtils.h"
#include "Game/BaseUpdater.h"
#include "Game/Entity.h"
#include "Game/EntityComponent.h"
#include "Game/LevelAssets.h"
#include "Assets/AssetDatabase.h"
#include "Framework/MapUtil.h"


#include "Framework/Log.h"
#include "SerializationAPI.h"





using std::to_string;


#include "Framework/SerializedForDiffing.h"

template<typename T>
T* cast_to(ClassBase* ptr) {
	return ptr ?  ptr->cast_to<T>() : nullptr;
}

// factory method for components
// factory method for prefabs
// both defined in lua

#include "Framework/MapUtil.h"

#include <json.hpp>
#include "Framework/SerializerJson2.h"

#include "Framework/SerializerBinary.h"
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
