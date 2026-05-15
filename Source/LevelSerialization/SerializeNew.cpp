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
#include <unordered_set>
#include <vector>

namespace {
// Validation helpers — throw SerializeInputError with the failing field path on mismatch.
void require_object(const nlohmann::json& j, const char* what) {
	if (!j.is_object())
		throw SerializeInputError(std::string(what) + ": expected JSON object");
}
void require_field(const nlohmann::json& j, const char* key, const char* what) {
	if (!j.is_object() || !j.contains(key))
		throw SerializeInputError(std::string(what) + ": missing field '" + key + "'");
}
} // namespace

// On-disk scene/prefab schema version. Bump when the layout changes; readers
// reject versions newer than this. Missing __version is treated as v1.
static constexpr int kSerializeFormatVersion = 1;

UnserializedSceneFile NewSerialization::unserialize_from_json(const char* debug_tag, SerializedForDiffing& json,
															  bool keepid) {
	UnserializedSceneFile outfile;
	auto& obj = json.jsonObj;
	require_object(obj, debug_tag);

	int version = 1;
	if (obj.contains("__version")) {
		const auto& vf = obj["__version"];
		if (!vf.is_number_integer())
			throw SerializeInputError(std::string(debug_tag) + ": '__version' must be an integer");
		version = vf.get<int>();
	}
	if (version < 1 || version > kSerializeFormatVersion)
		throw SerializeInputError(std::string(debug_tag) + ": unsupported scene file version " +
								  std::to_string(version));

	require_field(obj, "objs", debug_tag);
	auto& objarr = obj["objs"];
	if (!objarr.is_array())
		throw SerializeInputError(std::string(debug_tag) + ": 'objs' must be an array");

	std::unordered_set<uint64_t> seen_ids;
	for (auto& ent : objarr) {
		require_object(ent, debug_tag);
		require_field(ent, "__typename", debug_tag);
		const auto& typefield = ent["__typename"];
		if (!typefield.is_string())
			throw SerializeInputError(std::string(debug_tag) + ": '__typename' must be a string");
		std::string type = typefield.get<std::string>();

		// Hold both as unique_ptr until fully built so a throw in the reader can't leak.
		auto e = std::make_unique<Entity>();
		std::unique_ptr<Component> c(ClassBase::create_class<Component>(type.c_str()));
		if (!c)
			throw SerializeInputError(std::string(debug_tag) + ": unknown component type '" + type + "'");
		e->add_component_from_unserialization(c.get());
		{ ReadSerializerBackendJson2 reader(debug_tag, ent, *e); }
		{ ReadSerializerBackendJson2 reader(debug_tag, ent, *c); }
		if (keepid && ent.contains("__retid")) {
			const auto& idfield = ent["__retid"];
			if (!idfield.is_number_integer())
				throw SerializeInputError(std::string(debug_tag) + ": '__retid' must be an integer");
			uint64_t instid = idfield.get<uint64_t>();
			if (instid == 0)
				throw SerializeInputError(std::string(debug_tag) + ": '__retid' may not be 0");
			if (!seen_ids.insert(instid).second)
				throw SerializeInputError(std::string(debug_tag) + ": duplicate '__retid' " +
										  std::to_string(instid));
			e->post_unserialization(instid);
		}
		outfile.all_obj_vec.push_back(e.release());
		outfile.all_obj_vec.push_back(c.release());
	}
	return outfile;
}

UnserializedSceneFile NewSerialization::unserialize_from_text(const char* debug_tag, const std::string& text,
															  bool keepid) {
	SerializedForDiffing sfd;
	try {
		sfd.jsonObj = nlohmann::json::parse(text);
	} catch (const nlohmann::json::exception& e) {
		throw SerializeInputError(std::string(debug_tag) + ": malformed JSON: " + e.what());
	}
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
	obj["__version"] = kSerializeFormatVersion;
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
			WriteSerializerBackendJson2 writer(debug_tag, *ent);
			if (writer.get_output().is_object())
				out.update(writer.get_output());
		}
		auto c = ent->get_components()[0];
		{
			WriteSerializerBackendJson2 writer(debug_tag, *c);
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
	if (!StringUtils::starts_with(text, "!json\n"))
		throw SerializeInputError(std::string(debug_tag) + ": unsupported scene format prefix");
	return NewSerialization::unserialize_from_text(debug_tag, text.substr(6), keepid);
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
