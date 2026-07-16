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
#include <unordered_map>
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

	// Parallel to objarr: the built Entity at each array position (nullptr for unknown-type blobs
	// that were preserved instead of instantiated). Lets `__parent` index directly even when an
	// earlier obj failed to build. Captured hierarchy metadata is applied in a second pass once
	// every entity exists.
	std::vector<Entity*> entity_by_index(objarr.size(), nullptr);
	std::vector<int> parent_index(objarr.size(), -1);
	std::vector<bool> is_top_level(objarr.size(), false);
	std::vector<std::string> parent_bone(objarr.size());

	std::unordered_set<uint64_t> seen_ids;
	int obj_index = -1;
	for (auto& ent : objarr) {
		++obj_index;
		require_object(ent, debug_tag);
		require_field(ent, "__typename", debug_tag);
		const auto& typefield = ent["__typename"];
		if (!typefield.is_string())
			throw SerializeInputError(std::string(debug_tag) + ": '__typename' must be a string");
		std::string type = typefield.get<std::string>();

		// Hold both as unique_ptr until fully built so a throw in the reader can't leak.
		auto e = std::make_unique<Entity>();
		std::unique_ptr<Component> c(ClassBase::create_class<Component>(type.c_str()));
		if (!c) {
			// Class is missing from this build (deleted, renamed, branch mismatch). Stash the raw
			// JSON so a later save can splice it back verbatim instead of losing the entity.
			sys_print(Warning,
					  "%s: unknown component type '%s' — preserving as opaque blob for round-trip\n",
					  debug_tag, type.c_str());
			outfile.unknown_objs.push_back(ent);
			continue;
		}
		e->add_component_from_unserialization(c.get());
		std::unordered_set<std::string> consumed;
		{
			ReadSerializerBackendJson2 reader(debug_tag, ent, *e);
			consumed.insert(reader.get_consumed_keys().begin(), reader.get_consumed_keys().end());
		}
		{
			ReadSerializerBackendJson2 reader(debug_tag, ent, *c);
			consumed.insert(reader.get_consumed_keys().begin(), reader.get_consumed_keys().end());
		}
		// Flag any JSON keys neither reader consumed and not a reserved __* meta key — this is
		// almost always a typo (`"radiuss"`) or a stale field from an older version of the type.
		// Warns by default; bumping to throw is gated on the format version (see __version=2 bump).
		for (auto it = ent.begin(); it != ent.end(); ++it) {
			const std::string& key = it.key();
			if (key.size() >= 2 && key[0] == '_' && key[1] == '_')
				continue; // __typename, __retid, __version, etc. handled outside reflection
			if (consumed.count(key) == 0) {
				sys_print(Warning, "%s: unknown field '%s' on '%s' (typo or stale field?)\n",
						  debug_tag, key.c_str(), type.c_str());
				outfile.unknown_field_warnings.push_back(type + "." + key);
			}
		}
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
		// Capture hierarchy metadata (prefabs only; absent on level files). Validated/applied below
		// once every entity is built. `__parent` is an index into this objs array.
		if (ent.contains("__parent")) {
			const auto& pf = ent["__parent"];
			if (!pf.is_number_integer())
				throw SerializeInputError(std::string(debug_tag) + ": '__parent' must be an integer");
			parent_index[obj_index] = pf.get<int>();
		}
		if (ent.contains("__is_top_level") && ent["__is_top_level"].is_boolean())
			is_top_level[obj_index] = ent["__is_top_level"].get<bool>();
		if (ent.contains("__parent_bone") && ent["__parent_bone"].is_string())
			parent_bone[obj_index] = ent["__parent_bone"].get<std::string>();

		entity_by_index[obj_index] = e.get();
		outfile.all_obj_vec.push_back(e.release());
		outfile.all_obj_vec.push_back(c.release());
	}

	// Second pass: resolve parent references to pointers and validate, but DO NOT call parent_to()
	// here. Level::insert_* asserts every scene-file entity is unparented on entry and applies the
	// hierarchy only after all entities are inserted+initialized. We record the links for it to
	// apply. parent_to() there does not touch the child's local pos/rot/scale, so the loaded local
	// transform is preserved as-is.
	for (int i = 0; i < (int)entity_by_index.size(); ++i) {
		Entity* child = entity_by_index[i];
		if (!child)
			continue;
		SceneHierarchyLink link;
		link.child = child;
		const int pidx = parent_index[i];
		if (pidx >= 0) {
			if (pidx == i)
				throw SerializeInputError(std::string(debug_tag) + ": '__parent' points to itself at index " +
										  std::to_string(i));
			if (pidx >= (int)entity_by_index.size() || !entity_by_index[pidx])
				throw SerializeInputError(std::string(debug_tag) + ": '__parent' index " + std::to_string(pidx) +
										  " out of range at index " + std::to_string(i));
			link.parent = entity_by_index[pidx];
		}
		link.is_top_level = is_top_level[i];
		if (!parent_bone[i].empty()) {
			link.has_bone = true;
			link.parent_bone = parent_bone[i];
		}
		if (link.parent || link.is_top_level || link.has_bone)
			outfile.hierarchy.push_back(std::move(link));
	}
	return outfile;
}

nlohmann::json NewSerialization::parse_scene_json(const char* debug_tag, const std::string& text) {
	// Tolerate any newline style after the "!json" marker (\n, \r\n, \r) — files
	// authored on Windows or round-tripped through git's autocrlf land as \r\n,
	// and a strict "!json\n" match silently rejects them as "unsupported scene
	// format prefix". Skip past the marker and the first newline run.
	static constexpr const char* kMarker = "!json";
	static constexpr size_t kMarkerLen = 5;
	if (!StringUtils::starts_with(text, kMarker))
		throw SerializeInputError(std::string(debug_tag) + ": unsupported scene format prefix");
	size_t bodyStart = kMarkerLen;
	while (bodyStart < text.size() && (text[bodyStart] == '\r' || text[bodyStart] == '\n'))
		++bodyStart;
	if (bodyStart == kMarkerLen)
		throw SerializeInputError(std::string(debug_tag) + ": unsupported scene format prefix");
	try {
		return nlohmann::json::parse(text.begin() + bodyStart, text.end());
	} catch (const nlohmann::json::exception& e) {
		throw SerializeInputError(std::string(debug_tag) + ": malformed JSON: " + e.what());
	}
}

UnserializedSceneFile NewSerialization::unserialize_from_text(const char* debug_tag, const std::string& text,
															  bool keepid) {
	SerializedForDiffing sfd;
	sfd.jsonObj = parse_scene_json(debug_tag, text);
	return unserialize_from_json(debug_tag, sfd, keepid);
}

// what it expects: list of entities that are serializable. cant be non serialized objects
// cant be a subobject of a prefab, has to be the prefab root
// will check if prefab object is actually part of the template. this handles cases where the editor screwed up
// for example, a missing object in the prefab. or an object that isnt actually in the prefab. (remember, nodes from
// instanced prefabs cant be deleted) also checks for fully unique ids.

SerializedSceneFile NewSerialization::serialize_to_text(const char* debug_tag, const std::vector<Entity*>& input_objs,
														bool write_ids, const char* prefab_name,
														const std::vector<nlohmann::json>* preserved_unknown_objs,
														bool serialize_hierarchy) {
	double now = GetTime();

	// An entity is emitted if it has a component and isn't marked non-serializeable. In the
	// legacy flat format (serialize_hierarchy==false) parented entities are additionally skipped,
	// preserving the exact .tmap layout. With hierarchy on, children are emitted too.
	auto should_emit = [&](const Entity* ent) -> bool {
		if (ent->get_components().size() == 0)
			return false;
		if (ent->dont_serialize_or_edit || ent->dont_serialize)
			return false;
		if (!serialize_hierarchy && ent->get_parent())
			return false;
		return true;
	};

	// Pass 1 (hierarchy only): assign each emitted entity its position in the `objs` array so a
	// child can reference its parent by index. Parents that aren't themselves emitted (e.g. a
	// non-serializeable prefab root) are absent from the map and the child serializes as a root.
	std::unordered_map<const Entity*, int> index_of;
	if (serialize_hierarchy) {
		int next_index = 0;
		for (auto ent : input_objs)
			if (should_emit(ent))
				index_of[ent] = next_index++;
	}

	nlohmann::json obj;
	obj["__version"] = kSerializeFormatVersion;
	obj["objs"] = nlohmann::json::array();
	for (auto ent : input_objs) {
		if (!should_emit(ent))
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
		// Hierarchy metadata: only written for prefabs. Old readers ignore any `__`-prefixed key,
		// so these are additive and don't require a format version bump.
		if (serialize_hierarchy) {
			if (Entity* p = ent->get_parent()) {
				auto it = index_of.find(p);
				if (it != index_of.end())
					out["__parent"] = it->second;
			}
			if (ent->get_is_top_level())
				out["__is_top_level"] = true;
			if (ent->has_parent_bone())
				out["__parent_bone"] = ent->get_parent_bone().get_c_str();
		}
		obj["objs"].push_back(out);
	}
	// Splice preserved unknown-typename blobs back in verbatim. Opt-in (nullptr default)
	// so command/undo serializers don't drag every preserved blob into a partial snapshot.
	if (preserved_unknown_objs) {
		for (const auto& blob : *preserved_unknown_objs)
			obj["objs"].push_back(blob);
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

// TODO prefabs
// rules:
// * path based on source

bool serialize_this_objects_children(const Entity* b) {
	if (b->dont_serialize_or_edit || b->dont_serialize)
		return false;
	return true;
}

bool this_is_a_serializeable_object(const BaseUpdater* b) {
	assert(b);
	if (b->dont_serialize_or_edit || b->dont_serialize)
		return false;

	if (auto as_comp = b->cast_to<Component>()) {
		assert(as_comp->get_owner());
		if (!serialize_this_objects_children(as_comp->get_owner()))
			return false;
	}
	return true;
}
