#include "SerializeNew.h"
#include "AssetCompile/Someutils.h"
#include "Assets/AssetDatabase.h"
#include "Level.h"
#include "Game/Entity.h"
#include "Game/EntityComponent.h"
#include "Game/LevelAssets.h"
#include "GameEnginePublic.h"
#include "Framework/StringUtils.h"
#include "Framework/Log.h"
#include "Framework/ReflectionProp.h"
#include "Framework/ClassBase.h"
#include "Framework/Util.h"
#include "Framework/SerializedForDiffing.h"
#include "Framework/SerializerJson2.h"

#include <json.hpp>
#include <stdexcept>
#include <vector>

UnserializedSceneFile NewSerialization::unserialize_from_json(const char* debug_tag, SerializedForDiffing& json,
															  bool keepid) {
	UnserializedSceneFile outfile;
	auto& obj = json.jsonObj;
	if (obj["objs"].is_array()) {
		auto& objarr = obj["objs"];
		for (auto& ent : objarr) {
			std::string type = ent["__typename"];
			// Hold both as unique_ptr until fully built so a throw in the reader can't leak.
			auto e = std::make_unique<Entity>();
			std::unique_ptr<Component> c(ClassBase::create_class<Component>(type.c_str()));
			ASSERT(c);
			e->add_component_from_unserialization(c.get());
			{ ReadSerializerBackendJson2 reader("", ent, *e); }
			{ ReadSerializerBackendJson2 reader("", ent, *c); }
			if (keepid && ent["__retid"].is_number()) {
				int64_t instid = ent["__retid"];
				e->post_unserialization(instid);
			}
			outfile.all_obj_vec.push_back(e.release());
			outfile.all_obj_vec.push_back(c.release());
		}
		return outfile;
	}
	throw std::runtime_error("invalid json");
}

UnserializedSceneFile NewSerialization::unserialize_from_text(const char* debug_tag, const std::string& text,
															  bool keepid) {

	SerializedForDiffing sfd;
	sfd.jsonObj = nlohmann::json::parse(text);
	return unserialize_from_json(debug_tag, sfd, keepid);
}

// what it expects: list of entities that are serializable. cant be non serialized objects
// cant be a subobject of a prefab, has to be the prefab root
// will check if prefab object is actually part of the template. this handles cases where the editor screwed up
// for example, a missing object in the prefab. or an object that isnt actually in the prefab. (remember, nodes from
// instanced prefabs cant be deleted) also checks for fully unique ids.

SerializedSceneFile NewSerialization::serialize_to_text(const char* debug_tag, const std::vector<Entity*>& input_objs,
														bool write_ids, const char* prefab_name) {
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
		auto c = ent->get_components()[0];
		{
			WriteSerializerBackendJson2 writer("", *c);
			if (writer.get_output().is_object())
				out.update(writer.get_output());
		}
		out["__typename"] = c->get_type().classname;
		if (write_ids)
			out["__retid"] = ent->get_instance_id();
		obj["objs"].push_back(out);
	}
	SerializedSceneFile outfile;
	outfile.text = "!json\n" + obj.dump(1);

	sys_print(Debug, "NewSerialization::serialize_to_text: took %f\n", float(GetTime() - now));

	return outfile;
}

void UnserializedSceneFile::delete_objs() {
	if (!ownership_transferred) {
		for (auto& o : all_obj_vec)
			delete o;
	}
	all_obj_vec.clear();
}

UnserializedSceneFile unserialize_entities_from_text(const char* debug_tag, const std::string& text, bool keepid) {
	if (StringUtils::starts_with(text, "!json\n")) {
		auto fixedtext = text.substr(5);
		return NewSerialization::unserialize_from_text(debug_tag, fixedtext, keepid);
	} else {
		sys_print(Error, "unserialize_entities_from_text: old format not supported\n");
		return UnserializedSceneFile();
	}
}

// TODO prefabs
// rules:
// * path based on source

bool serialize_this_objects_children(const Entity* b) {
	if (b->dont_serialize_or_edit)
		return false;
	return true;
}

bool this_is_a_serializeable_object(const BaseUpdater* b) {
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
