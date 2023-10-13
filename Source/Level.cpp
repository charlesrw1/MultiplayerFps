#include "Level.h"
#include "Model.h"
#include "tiny_gltf.h"
#include "glm/gtc/type_ptr.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "Physics.h"
#include "Texture.h"
#include <array>
static const char* const level_directory = "Data\\Models\\";
static std::array<Level*, 2> loaded_levels;

bool IsMaterialCollidable() {
	return true;
}
static void LoadCollisionData(Level* level, tinygltf::Model& scene, tinygltf::Mesh& mesh, glm::mat4 transform)
{
	Level::CollisionData& out_data = level->collision_data;
	for (int part = 0; part < mesh.primitives.size(); part++)
	{
		const int vertex_offset = out_data.vertex_list.size();

		tinygltf::Primitive& primitive = mesh.primitives[part];
		ASSERT(primitive.attributes.find("POSITION") != primitive.attributes.end());
		tinygltf::Accessor& position_ac = scene.accessors[primitive.attributes["POSITION"]];
		tinygltf::BufferView& position_bv = scene.bufferViews[position_ac.bufferView];
		tinygltf::Buffer& pos_buffer = scene.buffers[position_bv.buffer];
		int pos_stride = position_ac.ByteStride(position_bv);
		ASSERT(position_ac.type == TINYGLTF_TYPE_VEC3 && position_ac.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
		ASSERT(position_ac.byteOffset == 0&&position_bv.byteStride==0);
		unsigned char* pos_start = &pos_buffer.data.at(position_bv.byteOffset);
		for (int v = 0; v < position_ac.count; v++) {
			glm::vec3 data = *(glm::vec3*)(pos_start + v * pos_stride);
			out_data.vertex_list.push_back(data);
		}

		// Transform verts
		if (transform != glm::mat4(1)) {
			for (int v = 0; v < position_ac.count; v++) {
				out_data.vertex_list[vertex_offset + v] = transform * glm::vec4(out_data.vertex_list[vertex_offset + v],1.0);
			}
		}

		tinygltf::Accessor& index_ac = scene.accessors[primitive.indices];
		tinygltf::BufferView& index_bv = scene.bufferViews[index_ac.bufferView];
		tinygltf::Buffer& index_buffer = scene.buffers[index_bv.buffer];
		int index_stride = index_ac.ByteStride(index_bv);
		ASSERT(index_ac.byteOffset == 0 && index_bv.byteStride == 0);
		unsigned char* index_start = &index_buffer.data.at(index_bv.byteOffset);
		for (int i = 0; i < index_ac.count; i+=3) {
			Level::CollisionTri ct;
			if (index_ac.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
				ct.indicies[0] = *(unsigned int*)(index_start + index_stride * i);
				ct.indicies[1] = *(unsigned int*)(index_start + index_stride * (i+1));
				ct.indicies[2] = *(unsigned int*)(index_start + index_stride * (i+2));
			}
			else if (index_ac.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
				ct.indicies[0] = *(unsigned short*)(index_start + index_stride * i);
				ct.indicies[1] = *(unsigned short*)(index_start + index_stride * (i+1));
				ct.indicies[2] = *(unsigned short*)(index_start + index_stride * (i+2));
			}
			ct.indicies[0] += vertex_offset;
			ct.indicies[1] += vertex_offset;
			ct.indicies[2] += vertex_offset;

			glm::vec3 verts[3];
			for (int j = 0; j < 3; j++)
				verts[j] = out_data.vertex_list.at(ct.indicies[j]);
			glm::vec3 face_normal = glm::normalize(glm::cross(verts[1] - verts[0], verts[2] - verts[0]));
			ct.face_normal = face_normal;
			ct.plane_offset = -glm::dot(face_normal, verts[0]);

			// TODO: surface flags
			
			out_data.collision_tris.push_back(ct);
		}
	}
}

static void ParseObject(Level* level, const tinygltf::Node& node, glm::mat4 transform, bool& should_collide, bool& should_render)
{
	should_collide = should_render = false;
	size_t endpos = node.name.find(']');
	if (endpos == std::string::npos || endpos==1) {
		printf("Bad object: %s\n", node.name.c_str());
		return;
	}
	glm::vec3 euler_angles = glm::vec3(0);
	if (node.rotation.size() == 4) {
		glm::quat quat = glm::make_quat<double>(node.rotation.data());
		euler_angles = glm::eulerAngles(quat);
	}
	std::string type_name = node.name.substr(1, endpos-1);
	bool is_trig = type_name.find("trig:") != std::string::npos;
	if (is_trig) {
		type_name = type_name.substr(5);
		Level::Trigger trig{};
		if (type_name == "spawn_zone") {
			trig.type = 0;
		}
		else {
			printf("Bad trigger type: %s\n", node.name.c_str());
			return;
		}
		trig.size = glm::vec3(glm::length(transform[0]), glm::length(transform[1]), glm::length(transform[2]));
		transform[0] /= trig.size.x;
		transform[1] /= trig.size.y;
		transform[2] /= trig.size.z;

		trig.inv_transform = glm::inverse(transform);
		trig.shape_type = 0;	// for later use (spheres)

		level->triggers.push_back(trig);
	}
	else
	{
		if (type_name == "player_spawn")
		{
			Level::PlayerSpawn spawn;
			spawn.angle = euler_angles.y;
			spawn.position = transform[3];
			spawn.team = 0;
			spawn.mode = 0;
			level->spawns.push_back(spawn);
		}
		else
		{
			printf("Unknown obj type: %s\n", node.name.c_str());
			return;
		}

	}
	
}

static void LookThroughNodeTree(Level* level, tinygltf::Model& scene, tinygltf::Node& node, glm::mat4 global_transform)
{
	glm::mat4 local_transform = glm::mat4(1);
	if (node.matrix.size() == 16) {
		double* m = node.matrix.data();
		local_transform = glm::mat4(m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7],
			m[8], m[9], m[10], m[11], m[12], m[13], m[14], m[15]);
	}
	else {
		glm::vec3 translation = glm::vec3(0.f);
		glm::vec3 scale = glm::vec3(1.f);
		glm::quat rot = glm::quat(1.f, 0.f, 0.f, 0.f);
		if (node.translation.size() != 0)
			translation = glm::make_vec3<double>(node.translation.data());
		if (node.rotation.size() != 0)
			rot = glm::make_quat<double>(node.rotation.data());
		if (node.scale.size() != 0)
			scale = glm::make_vec3<double>(node.scale.data());
		local_transform = glm::translate(glm::mat4(1), translation);
		local_transform = local_transform * glm::mat4_cast(rot);
		local_transform = glm::scale(local_transform, scale);
	}
	global_transform = global_transform * local_transform;
	bool statically_drawn = true;
	bool has_collision = true;

	if (node.name.size() > 0 && node.name[0] == '[')
		ParseObject(level,node,global_transform,has_collision, statically_drawn);

	if (node.mesh != -1) {
		if (statically_drawn) {
			Level::StaticInstance sm;
			sm.model_index = node.mesh;
			sm.transform = global_transform;
			level->render_data.instances.push_back(sm);
		}
		if (has_collision) {
			LoadCollisionData(level, scene, scene.meshes[node.mesh], global_transform);
		}
	}

	for (int i = 0; i < node.children.size(); i++)
		LookThroughNodeTree(level, scene, scene.nodes[node.children[i]], global_transform);
}

static Texture* LoadGltfImage(tinygltf::Image& i, tinygltf::Model& scene)
{
	tinygltf::BufferView& bv = scene.bufferViews[i.bufferView];
	tinygltf::Buffer& b = scene.buffers[bv.buffer];
	ASSERT(bv.byteStride == 0);

	return CreateTextureFromImgFormat(&b.data.at(bv.byteOffset), bv.byteLength, i.name);
}

static void GatherRenderData(Level* level, tinygltf::Model& scene)
{
	std::vector<MeshMaterial> mm;

	for (int matidx = 0; matidx < scene.materials.size(); matidx++) {
		tinygltf::Material& mat = scene.materials[matidx];
		MeshMaterial mymat;
		if (mat.pbrMetallicRoughness.baseColorTexture.index != -1) {
			mymat.t1 = LoadGltfImage(scene.images.at(mat.pbrMetallicRoughness.baseColorTexture.index), scene);
		}
		mm.push_back(mymat);
	}

	for (int i = 0; i < scene.meshes.size(); i++) {
		Model* m = new Model;
		std::map<int, int> mapping;
		AppendGltfMeshToModel(m, scene, scene.meshes[i], mapping);
		level->render_data.embedded_meshes.push_back(m);
		// TODO: materials

		for (int i = 0; i < m->parts.size(); i++) {
			auto& part = m->parts[i];
			if (part.material_idx != -1) {
				m->materials.push_back(mm.at(part.material_idx));
				part.material_idx = m->materials.size() - 1;
			}
		}
	}
}

const Level* LoadLevelFile(const char* level_name)
{
	for (int i = 0; i < loaded_levels.size(); i++) {
		if (loaded_levels[i]&&loaded_levels[i]->name == level_name) {
			loaded_levels[i]->ref_count++;
			return loaded_levels[i];
		}
	}
	if (loaded_levels[0] && loaded_levels[1])
		Fatalf("attempting to load 3 levels into memory");
	int open_slot = (loaded_levels[0]) ? 1 : 0;

	std::string path;
	path.reserve(256);
	path += level_directory;
	path += level_name;

	tinygltf::Model scene;
	tinygltf::TinyGLTF loader;
	std::string errStr;
	std::string warnStr;
	bool res = loader.LoadBinaryFromFile(&scene, &errStr, &warnStr, path);
	if (!res) {
		printf("Couldn't load level: %s\n", path.c_str());
		return nullptr;
	}

	Level* level = new Level;
	level->name = level_name;
	GatherRenderData(level, scene);
	tinygltf::Scene& defscene = scene.scenes[scene.defaultScene];
	for (int i = 0; i < defscene.nodes.size(); i++) {
		LookThroughNodeTree(level, scene, scene.nodes[defscene.nodes[i]], glm::mat4(1));
	}
	InitStaticGeoBvh(level);
	level->ref_count = 1;
	loaded_levels[open_slot] = level;
	return level;
}

void FreeLevel(const Level* level)
{
	ASSERT(level->ref_count != 0);
	int i = 0;
	for (; i < loaded_levels.size(); i++) {
		if (loaded_levels[i] == level)
			break;
	}
	if (i == loaded_levels.size())
		Fatalf("free called on already freed level");
	loaded_levels[i]->ref_count--;
	if (loaded_levels[i]->ref_count <= 0) {
		printf("deleting level, %s\n", loaded_levels[i]->name.c_str());
		delete loaded_levels[i];
		loaded_levels[i] = nullptr;
	}
}