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
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <stdexcept>

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

} // namespace

AGENT_BRIDGE_COMMAND(export_tmap, export_tmap_cmd);
AGENT_BRIDGE_COMMAND(ping, ping_cmd);
