// Built-in AgentBridge commands. Kept separate from AgentBridge.cpp (the transport) so new
// commands can be added here (or in further files) without touching the bridge itself.
#include "AgentBridge.h"
#include "Game/LevelAssets.h"
#include "Game/Prefab.h"
#include "LevelSerialization/SerializeNew.h"
#include "Game/Entity.h"
#include "Game/Components/MeshComponent.h"
#include "Render/Model.h"
#include "Render/MaterialPublic.h"
#include "AssetCompile/GltfExport.h"
#include "Framework/Config.h"
#include "GameEnginePublic.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <stdexcept>
#include <fstream>
#include <unordered_map>

#ifdef _WIN32
#include <windows.h>
#endif

extern ConfigVar g_project_base;
extern ConfigVar g_agentbridge_port;

namespace {

glm::vec3 unpack_normal(const ModelVertex& mv) {
	return glm::normalize(glm::vec3(double(mv.normal[0]) / INT16_MAX, double(mv.normal[1]) / INT16_MAX,
									double(mv.normal[2]) / INT16_MAX));
}

// Reads an arbitrary .tmap/.tprefab's MeshComponents directly from its deserialized-but-never-
// activated UnserializedSceneFile (see load_level_asset): no Level is spawned, nothing gets
// registered with the renderer/GI, and whatever level the editor currently has open is completely
// untouched. Bakes each entity's transform into engine-space vertex positions and writes them out
// as a real, engine-authoritative .gltf, grouped one node per MeshComponent, so an external tool
// (the Blender viewer addon) can import it with a standard glTF importer and get correct
// coordinates for free, without needing to know or guess this engine's axis conventions.
nlohmann::json export_tmap_cmd(const nlohmann::json& args) {
	if (!args.contains("tmap") || !args["tmap"].is_string())
		throw std::runtime_error("export_tmap requires a string 'tmap' arg (game-relative path, e.g. \"maps/foo.tmap\")");
	if (!args.contains("output") || !args["output"].is_string())
		throw std::runtime_error("export_tmap requires a string 'output' arg (absolute output .gltf path)");

	const std::string tmap_path = args["tmap"];
	const std::string output_path = args["output"];

	auto file = load_level_asset(tmap_path);
	if (!file)
		throw std::runtime_error("could not load '" + tmap_path + "'");

	// Flat .tmap files have no hierarchy (file->hierarchy is empty, so this is a no-op there).
	// .tprefab files commonly nest MeshComponents under a parent entity; without wiring the
	// parent pointers, every entity's position/rotation/scale reads as LOCAL-to-parent instead
	// of world, silently placing/skew-ing nested MeshComponents wrong (or piling them up near
	// the origin, looking like they were never exported at all). parent_to() is pure linked-list
	// bookkeeping - safe here since these entities are never inserted into any Level.
	PrefabAsset::wire_hierarchy(*file);

	std::vector<ExportVertex> vertices;
	std::vector<uint32_t> indices;
	std::vector<Part> parts;
	nlohmann::json nodes = nlohmann::json::array();
	int mesh_count = 0;

	for (BaseUpdater* obj : file->all_obj_vec) {
		Entity* ent = obj ? obj->cast_to<Entity>() : nullptr;
		if (!ent)
			continue;
		MeshComponent* mesh = ent->get_component<MeshComponent>();
		if (!mesh)
			continue;
		const Model* model = mesh->get_model();
		if (!model || model->get_num_lods() == 0)
			continue;

		// get_ws_transform() composes the full parent chain via matrix multiply (correct even
		// for non-uniform scale); the separate get_ws_position/rotation/scale() convenience
		// getters are NOT used here since get_ws_scale() has a known bug that just returns
		// (1,1,1) whenever the entity has a parent.
		const glm::mat4 transform = ent->get_ws_transform();
		const glm::mat3 normal_transform = glm::mat3(transform);

		const std::string node_name =
			!ent->get_editor_name().empty() ? ent->get_editor_name() : ("mesh_" + std::to_string(mesh_count));
		mesh_count++;

		nlohmann::json materials = nlohmann::json::array();
		for (auto& mat : mesh->get_material_overrides()) {
			if (mat.get())
				materials.push_back(mat.get()->get_name());
		}
		nlohmann::json node_info;
		node_info["name"] = node_name;
		node_info["materials"] = materials;
		nodes.push_back(node_info);

		auto& lod = model->get_lod(0);
		const RawMeshData* meshData = model->get_raw_mesh_data();
		for (int partI = lod.part_ofs; partI < lod.part_ofs + lod.part_count; partI++) {
			auto& part = model->get_part(partI);
			Part p;
			p.node_name = node_name;
			const int new_offset = (int)vertices.size();
			const int new_index_start = (int)indices.size();

			for (int v = 0; v < part.vertex_count; v++) {
				auto& vertex = meshData->get_vertex_at_index(part.base_vertex + v);
				ExportVertex ev{};
				ev.position = glm::vec3(transform * glm::vec4(vertex.pos, 1.0f));
				ev.normal = normal_transform * unpack_normal(vertex);
				ev.uv = vertex.uv;
				vertices.push_back(ev);
			}
			for (int i = 0; i < part.element_count; i++) {
				const int start = part.element_offset / sizeof(int16_t);
				const uint16_t index = meshData->get_index_at_index(start + i);
				// Local to this part's own vertex accessor (which starts at vert_ofs=new_offset
				// in the shared buffer) - NOT offset by new_offset. glTF index values are always
				// relative to their primitive's own vertex accessor, 0-based within its own
				// count, regardless of where that accessor's data physically sits in the
				// underlying buffer. (Mirrors export_one_model, not export_level_scene, which
				// merges everything into a single Part and so needs global-offset indices.)
				indices.push_back((uint32_t)index);
			}

			p.index_count = (int)indices.size() - new_index_start;
			p.index_ofs = new_index_start;
			p.vert_count = (int)vertices.size() - new_offset;
			p.vert_ofs = new_offset;
			parts.push_back(p);
		}
	}

	if (parts.empty())
		throw std::runtime_error("'" + tmap_path + "' has no exportable MeshComponents");

	if (!export_to_gltf(parts, vertices, indices, output_path.c_str(), /*separate_nodes=*/true))
		throw std::runtime_error("failed writing '" + output_path + "'");

	nlohmann::json result;
	result["output"] = output_path;
	result["nodes"] = nodes;
	return result;
}

nlohmann::json ping_cmd(const nlohmann::json&) {
	nlohmann::json result;
	result["pong"] = true;
	return result;
}

struct CommandDoc
{
	std::string description;
	std::string usage;
};

// Command descriptions/usage live outside the C++ registration API entirely, in
// docs/console_commands.json - generated by Scripts/gen_command_docs.py from `@cmd:`/`@usage:`
// comments above registration call sites (see that script + docs/tooling/cscli.md). Loaded lazily,
// relative to cwd (same convention as vars.txt/init.txt); a missing file just means blank
// descriptions, not an error.
const std::unordered_map<std::string, CommandDoc>& command_docs() {
	static std::unordered_map<std::string, CommandDoc> docs = [] {
		std::unordered_map<std::string, CommandDoc> map;
		std::ifstream f("docs/console_commands.json");
		if (!f)
			return map;
		try {
			nlohmann::json j;
			f >> j;
			for (auto it = j.begin(); it != j.end(); ++it) {
				CommandDoc d;
				if (it->contains("description"))
					d.description = (*it)["description"].get<std::string>();
				if (it->contains("usage"))
					d.usage = (*it)["usage"].get<std::string>();
				map[it.key()] = std::move(d);
			}
		} catch (...) {
			// malformed sidecar - proceed with whatever was parsed before the exception, blank is fine
		}
		return map;
	}();
	return docs;
}

nlohmann::json match_to_json(const Cmd_Manager::MatchType& m) {
	nlohmann::json e;
	e["name"] = m.name;
	e["is_command"] = m.is_cmd;
	auto it = command_docs().find(m.name);
	e["description"] = it != command_docs().end() ? it->second.description : "";
	e["usage"] = it != command_docs().end() ? it->second.usage : "";
	return e;
}

nlohmann::json status_cmd(const nlohmann::json&) {
	nlohmann::json r;
	r["mode"] = (eng && eng->is_editor_app()) ? "editor" : "game";
#ifdef _WIN32
	r["pid"] = (int)GetCurrentProcessId();
#endif
	r["project_base"] = g_project_base.get_string();
	r["port"] = g_agentbridge_port.get_integer();
	return r;
}

nlohmann::json run_command_cmd(const nlohmann::json& args) {
	if (!args.contains("command") || !args["command"].is_string())
		throw std::runtime_error("run_command requires a string 'command' arg");
	std::string command = args["command"];

	bool had_error = false;
	std::string output = Cmd_Manager::get()->execute_capture(command.c_str(), &had_error);

	nlohmann::json r;
	r["output"] = output;
	r["had_error"] = had_error;
	return r;
}

nlohmann::json list_commands_cmd(const nlohmann::json& args) {
	std::string filter = (args.contains("filter") && args["filter"].is_string()) ? args["filter"].get<std::string>() : "";
	auto matches = Cmd_Manager::get()->get_matches(filter.c_str());

	nlohmann::json arr = nlohmann::json::array();
	for (auto& m : matches)
		arr.push_back(match_to_json(m));

	nlohmann::json r;
	r["commands"] = arr;
	return r;
}

nlohmann::json describe_command_cmd(const nlohmann::json& args) {
	if (!args.contains("name") || !args["name"].is_string())
		throw std::runtime_error("describe_command requires a string 'name' arg");
	std::string name = args["name"];

	for (auto& m : Cmd_Manager::get()->get_matches(name.c_str())) {
		if (m.name == name)
			return match_to_json(m);
	}
	throw std::runtime_error("unknown command or variable '" + name + "'");
}

} // namespace

// @cmd: exports a .tmap/.tprefab's MeshComponents to a standalone .gltf, with baked world-space
// transforms, for external tools (e.g. the Blender viewer addon). Reads the asset directly without
// spawning a Level, so it never touches any level currently open in the editor.
// @usage: export_tmap {"tmap": "<game-relative .tmap/.tprefab path>", "output": "<absolute output .gltf path>"}
AGENT_BRIDGE_COMMAND(export_tmap, export_tmap_cmd);
// @cmd: connectivity check - returns {"pong": true} if the bridge is alive.
AGENT_BRIDGE_COMMAND(ping, ping_cmd);
// @cmd: reports which process (editor or game), pid, project base, and agent bridge port this
// bridge instance belongs to. Used by `cscli status`/`cscli instances` to identify a connection.
AGENT_BRIDGE_COMMAND(status, status_cmd);
// @cmd: runs a console command line exactly as if typed at the in-game console, and returns
// everything it printed. `had_error` is true if any part of the line logged at Error level.
// @usage: run_command {"command": "<command line, e.g. \"give_weapon rifle 30\">"}
AGENT_BRIDGE_COMMAND(run_command, run_command_cmd);
// @cmd: lists registered console commands and cvars (optionally substring-filtered), each with
// whatever description/usage docs/console_commands.json has for it.
// @usage: list_commands {"filter": "<optional substring, empty = everything>"}
AGENT_BRIDGE_COMMAND(list_commands, list_commands_cmd);
// @cmd: describes a single console command/cvar by exact name (name, is_command, description, usage).
// @usage: describe_command {"name": "<exact command or cvar name>"}
AGENT_BRIDGE_COMMAND(describe_command, describe_command_cmd);
