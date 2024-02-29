#include "Model.h"
#include "Memory.h"
#include <vector>
#include <map>
#include "glad/glad.h"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tiny_gltf.h"

#include "Util.h"
#include "Animation.h"
#include "Texture.h"

#define _CRT_SECURE_NO_WARNINGS
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"


#include "Memory.h"

#define USE_CGLTF


static const char* const model_folder_path = "./Data/Models/";
static std::vector<Model*> models;

// Hardcoded attribute locations for shaders
const int POSITION_LOC = 0;
const int UV_LOC = 1;
const int NORMAL_LOC = 2;
const int JOINT_LOC = 3;
const int WEIGHT_LOC = 4;
const int COLOR_LOC = 5;
const int UV2_LOC = 6;
const int TANGENT_LOC = 7;

struct Super_Vert
{
	glm::vec3 position;
	glm::vec2 uv0;
	glm::vec3 normal;
	glm::ivec4 joint;
	glm::vec4 weight;
	glm::vec3 color;
	glm::vec2 uv1;
	glm::vec3 tangent;
	glm::vec3 bitangent;
};


bool MeshPart::has_lightmap_coords() const
{
	return attributes & (1<<UV2_LOC);
}

bool MeshPart::has_bones() const
{
	return attributes & (1<<JOINT_LOC);
}

bool MeshPart::has_colors() const
{
	return attributes & (1<<COLOR_LOC);
}

bool MeshPart::has_tangents() const
{
	return attributes & (1 << TANGENT_LOC);
}



static uint32_t MakeOrFindGpuBuffer(Model* m, int buf_view_index, tinygltf::Model& model, std::map<int, int>& buffer_view_to_buffer)
{
	if (buffer_view_to_buffer.find(buf_view_index) != buffer_view_to_buffer.end())
		return buffer_view_to_buffer.find(buf_view_index)->second;

	tinygltf::BufferView buffer_view = model.bufferViews.at(buf_view_index);
	tinygltf::Buffer& gltf_buffer = model.buffers.at(buffer_view.buffer);

	Model::GpuBuffer buffer;
	glGenBuffers(1, &buffer.handle);
	glBindBuffer(buffer_view.target, buffer.handle);
	glBufferData(buffer_view.target, buffer_view.byteLength, &gltf_buffer.data.at(buffer_view.byteOffset), GL_STATIC_DRAW);
	glBindBuffer(buffer_view.target, 0);
	buffer.size = buffer_view.byteLength;
	buffer.target = buffer_view.target;

	m->buffers.push_back(buffer);
	int index = m->buffers.size() - 1;

	buffer_view_to_buffer[buf_view_index] = index;
	return index;
}

void append_collision_data(Model* m, tinygltf::Model& scene, tinygltf::Mesh& mesh, std::vector<Game_Shader*>& materials, 
	Physics_Mesh* phys, const glm::mat4& transform)
{
	if (!phys && !m->collision)
		m->collision = std::make_unique<Physics_Mesh>();

	Physics_Mesh* pm = (phys) ? phys : m->collision.get();

	for (int part = 0; part < mesh.primitives.size(); part++)
	{
		const int vertex_offset = pm->verticies.size();

		tinygltf::Primitive& primitive = mesh.primitives[part];
		ASSERT(primitive.attributes.find("POSITION") != primitive.attributes.end());
		tinygltf::Accessor& position_ac = scene.accessors[primitive.attributes["POSITION"]];
		tinygltf::BufferView& position_bv = scene.bufferViews[position_ac.bufferView];
		tinygltf::Buffer& pos_buffer = scene.buffers[position_bv.buffer];
		int pos_stride = position_ac.ByteStride(position_bv);
		ASSERT(position_ac.type == TINYGLTF_TYPE_VEC3 && position_ac.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
		ASSERT(position_ac.byteOffset == 0 && position_bv.byteStride == 0);
		unsigned char* pos_start = &pos_buffer.data.at(position_bv.byteOffset);
		for (int v = 0; v < position_ac.count; v++) {
			glm::vec3 data = *(glm::vec3*)(pos_start + v * pos_stride);
			pm->verticies.push_back(data);
		}

		// Transform verts
		if (transform != glm::mat4(1)) {
			for (int v = 0; v < position_ac.count; v++) {
				pm->verticies[vertex_offset + v] = transform * glm::vec4(pm->verticies[vertex_offset + v], 1.0);
			}
		}

		tinygltf::Accessor& index_ac = scene.accessors[primitive.indices];
		tinygltf::BufferView& index_bv = scene.bufferViews[index_ac.bufferView];
		tinygltf::Buffer& index_buffer = scene.buffers[index_bv.buffer];
		int index_stride = index_ac.ByteStride(index_bv);
		ASSERT(index_ac.byteOffset == 0 && index_bv.byteStride == 0);
		unsigned char* index_start = &index_buffer.data.at(index_bv.byteOffset);
		for (int i = 0; i < index_ac.count; i += 3) {
			Physics_Triangle ct;
			if (index_ac.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
				ct.indicies[0] = *(unsigned int*)(index_start + index_stride * i);
				ct.indicies[1] = *(unsigned int*)(index_start + index_stride * (i + 1));
				ct.indicies[2] = *(unsigned int*)(index_start + index_stride * (i + 2));
			}
			else if (index_ac.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
				ct.indicies[0] = *(unsigned short*)(index_start + index_stride * i);
				ct.indicies[1] = *(unsigned short*)(index_start + index_stride * (i + 1));
				ct.indicies[2] = *(unsigned short*)(index_start + index_stride * (i + 2));
			}
			ct.indicies[0] += vertex_offset;
			ct.indicies[1] += vertex_offset;
			ct.indicies[2] += vertex_offset;

			glm::vec3 verts[3];
			for (int j = 0; j < 3; j++)
				verts[j] = pm->verticies.at(ct.indicies[j]);
			glm::vec3 face_normal = glm::normalize(glm::cross(verts[1] - verts[0], verts[2] - verts[0]));
			ct.face_normal = face_normal;
			ct.plane_offset = -glm::dot(face_normal, verts[0]);	
			if (primitive.material != -1) {
				ct.surf_type = materials.at(primitive.material)->physics;
			}
			pm->tris.push_back(ct);
		}
	}
}

// for level meshes: default renderable = true, collidable = true (so detail props should be XXX_!r!nc)
// for level entities: default renderable = false, collidable = false (so a door should be XXX_!r!c)
// for regular meshes: default renderable = true, collidable = false (so collision_mesh should be XXX_!nr!c)

// physics_mesh can be nullptr, then it creates/uses model->collision
void add_node_mesh_to_model(Model* model, tinygltf::Model& inputMod, tinygltf::Node& node, 
	std::map<int,int>& buffer_view_to_buffer, bool render_default, bool collide_default, std::vector<Game_Shader*>& materials, 
	Physics_Mesh* physics, const glm::mat4& phys_transform)
{
	ASSERT(node.mesh != -1);
	bool renderable = render_default;
	bool collidable = collide_default;

	if (node.name.find("_!r!c") != std::string::npos) {
		renderable = true;
		collidable = true;
	}
	else if (node.name.find("_!nr!c") != std::string::npos) {
		renderable = false;
		collidable = true;
	}
	else if (node.name.find("_!r!nc") != std::string::npos) {
		renderable = true;
		collidable = false;
	}

	if (!renderable && !collidable)
		return;

	tinygltf::Mesh& mesh = inputMod.meshes[node.mesh];

	if (collidable)
		append_collision_data(model, inputMod, mesh, materials, physics, phys_transform);
	if (!renderable)
		return;

	for (int i = 0; i < mesh.primitives.size(); i++)
	{
		const tinygltf::Primitive& prim = mesh.primitives[i];

		MeshPart part;
		glGenVertexArrays(1, &part.vao);
		glBindVertexArray(part.vao);

		// index buffer
		const tinygltf::Accessor& indicies_accessor = inputMod.accessors[prim.indices];
		int index_buffer_index = MakeOrFindGpuBuffer(model, indicies_accessor.bufferView, inputMod, buffer_view_to_buffer);
		ASSERT(model->buffers[index_buffer_index].target == GL_ELEMENT_ARRAY_BUFFER);
		model->buffers[index_buffer_index].Bind();
		part.element_offset = indicies_accessor.byteOffset;
		part.element_count = indicies_accessor.count;
		part.element_type = indicies_accessor.componentType;
		part.base_vertex = 0;
		part.material_idx = prim.material;

		glCheckError();

		// Now do all the attributes
		bool found_joints_attrib = false;
		for (const auto& attrb : prim.attributes)
		{
			const tinygltf::Accessor& accessor = inputMod.accessors[attrb.second];
			int byte_stride = accessor.ByteStride(inputMod.bufferViews[accessor.bufferView]);
			int buffer_index = MakeOrFindGpuBuffer(model, accessor.bufferView, inputMod, buffer_view_to_buffer);
			ASSERT(model->buffers[buffer_index].target == GL_ARRAY_BUFFER);
			model->buffers[buffer_index].Bind();

			int location = -1;
			if (attrb.first == "POSITION") location = POSITION_LOC;
			else if (attrb.first == "TEXCOORD_0") location = UV_LOC;
			else if (attrb.first == "TEXCOORD_1") location = UV2_LOC;
			else if (attrb.first == "NORMAL") location = NORMAL_LOC;
			else if (attrb.first == "JOINTS_0") location = JOINT_LOC;
			else if (attrb.first == "WEIGHTS_0") location = WEIGHT_LOC;
			else if (attrb.first == "COLOR_0") location = COLOR_LOC;
			else if (attrb.first == "TANGENT") location = TANGENT_LOC;


			if (location == -1) continue;
			
			if (location == JOINT_LOC)
				found_joints_attrib = true;

			glEnableVertexAttribArray(location);
			if (location == JOINT_LOC) {
				glVertexAttribIPointer(location, accessor.type, accessor.componentType,
					byte_stride, (void*)accessor.byteOffset);
			}
			else {
				glVertexAttribPointer(location, accessor.type, accessor.componentType,
					accessor.normalized, byte_stride, (void*)accessor.byteOffset);
			}

			part.attributes |= (1 << location);

			glCheckError();
		}

		glBindVertexArray(0);


		if (!(part.attributes & (1<< POSITION_LOC)) || !(part.attributes & (1<<UV_LOC)) || !(part.attributes & (1<<NORMAL_LOC))) {
			sys_print("Model %s is missing nessecary vertex attributes\n", model->name.c_str());
		}

		model->parts.push_back(part);

		glCheckError();
	}


}


static uint32_t MakeOrFindGpuBuffer2(Model* m, bool index_buffer, cgltf_buffer_view* bv, cgltf_data* data, std::map<int, int>& buffer_view_to_buffer)
{
	int bv_index = cgltf_buffer_view_index(data, bv);
	if (buffer_view_to_buffer.find(bv_index) != buffer_view_to_buffer.end())
		return buffer_view_to_buffer.find(bv_index)->second;

	cgltf_buffer* gltf_buffer = bv->buffer;
	char* byte_buffer = (char*)gltf_buffer->data;

	Model::GpuBuffer buffer;
	glGenBuffers(1, &buffer.handle);

	uint32_t target = (!index_buffer) ? GL_ARRAY_BUFFER : GL_ELEMENT_ARRAY_BUFFER;

	glBindBuffer(target, buffer.handle);
	glBufferData(target, bv->size, &byte_buffer[bv->offset], GL_STATIC_DRAW);
	glBindBuffer(target, 0);
	buffer.size = bv->size;
	buffer.target = target;

	m->buffers.push_back(buffer);
	int index = m->buffers.size() - 1;

	buffer_view_to_buffer[bv_index] = index;
	return index;
}


GLenum cgltf_component_type_to_gl(cgltf_component_type ct)
{
	switch (ct)
	{
	case cgltf_component_type_r_8: return GL_BYTE; /* BYTE */
	case cgltf_component_type_r_8u: return GL_UNSIGNED_BYTE; /* UNSIGNED_BYTE */
	case cgltf_component_type_r_16: return GL_SHORT; /* SHORT */
	case cgltf_component_type_r_16u: return GL_UNSIGNED_SHORT; /* UNSIGNED_SHORT */
	case cgltf_component_type_r_32u: return GL_UNSIGNED_INT; /* UNSIGNED_INT */
	case cgltf_component_type_r_32f: return GL_FLOAT; /* FLOAT */
	default: return 0;
	}
};


void append_collision_data2(Model* m, cgltf_data* data, cgltf_mesh* mesh, std::vector<Game_Shader*>& materials,
	Physics_Mesh* phys, const glm::mat4& transform)
{
	if (!phys && !m->collision)
		m->collision = std::make_unique<Physics_Mesh>();

	Physics_Mesh* pm = (phys) ? phys : m->collision.get();

	for (int part = 0; part < mesh->primitives_count; part++)
	{
		const int vertex_offset = pm->verticies.size();
		cgltf_primitive& primitive = mesh->primitives[part];
		
		cgltf_attribute* position_at = nullptr;
		for (int j = 0; j < primitive.attributes_count; j++) {
			if (strcmp("POSITION", primitive.attributes[j].name) == 0) {
				position_at = primitive.attributes + j;
				break;
			}
		}
		ASSERT(position_at);
		cgltf_accessor* position_ac = position_at->data;
		cgltf_buffer_view* position_bv = position_ac->buffer_view;
		cgltf_buffer* pos_buffer = position_bv->buffer;
		uint8_t* byte_buffer = (uint8_t*)pos_buffer->data;

		//int pos_stride = position_ac.ByteStride(position_bv);
		int pos_stride = position_ac->stride;

		ASSERT(position_ac->type == cgltf_type_vec3 && position_ac->component_type == cgltf_component_type_r_32f);
		ASSERT(position_ac->offset == 0 && position_bv->stride == 0);
		unsigned char* pos_start = &byte_buffer[position_bv->offset];
		for (int v = 0; v < position_ac->count; v++) {
			glm::vec3 data = *(glm::vec3*)(pos_start + v * pos_stride);
			pm->verticies.push_back(data);
		}

		// Transform verts
		if (transform != glm::mat4(1)) {
			for (int v = 0; v < position_ac->count; v++) {
				pm->verticies[vertex_offset + v] = transform * glm::vec4(pm->verticies[vertex_offset + v], 1.0);
			}
		}

		cgltf_accessor* index_ac = primitive.indices;
		cgltf_buffer_view* index_bv = index_ac->buffer_view;
		cgltf_buffer* index_buffer = index_bv->buffer;
		int index_stride = index_ac->stride;
		byte_buffer = (uint8_t*)index_buffer->data;

		ASSERT(index_ac->offset == 0 && index_bv->stride == 0);
		unsigned char* index_start = &byte_buffer[index_bv->offset];
		for (int i = 0; i < index_ac->count; i += 3) {
			Physics_Triangle ct;
			if (index_ac->component_type == cgltf_component_type_r_32u) {
				ct.indicies[0] = *(unsigned int*)(index_start + index_stride * i);
				ct.indicies[1] = *(unsigned int*)(index_start + index_stride * (i + 1));
				ct.indicies[2] = *(unsigned int*)(index_start + index_stride * (i + 2));
			}
			else if (index_ac->component_type == cgltf_component_type_r_16u) {
				ct.indicies[0] = *(unsigned short*)(index_start + index_stride * i);
				ct.indicies[1] = *(unsigned short*)(index_start + index_stride * (i + 1));
				ct.indicies[2] = *(unsigned short*)(index_start + index_stride * (i + 2));
			}
			ct.indicies[0] += vertex_offset;
			ct.indicies[1] += vertex_offset;
			ct.indicies[2] += vertex_offset;

			glm::vec3 verts[3];
			for (int j = 0; j < 3; j++)
				verts[j] = pm->verticies.at(ct.indicies[j]);
			glm::vec3 face_normal = glm::normalize(glm::cross(verts[1] - verts[0], verts[2] - verts[0]));
			ct.face_normal = face_normal;
			ct.plane_offset = -glm::dot(face_normal, verts[0]);
			if (primitive.material) {
				int mat_index = cgltf_material_index(data, primitive.material);

				ct.surf_type = materials.at(mat_index)->physics;
			}
			pm->tris.push_back(ct);
		}
	}
}



// for level meshes: default renderable = true, collidable = true (so detail props should be XXX_!r!nc)
// for level entities: default renderable = false, collidable = false (so a door should be XXX_!r!c)
// for regular meshes: default renderable = true, collidable = false (so collision_mesh should be XXX_!nr!c)

// physics_mesh can be nullptr, then it creates/uses model->collision
void add_node_mesh_to_model2(Model* model, cgltf_data* data, cgltf_node* node,
	std::map<int, int>& buffer_view_to_buffer, bool render_default, bool collide_default, std::vector<Game_Shader*>& materials,
	Physics_Mesh* physics, const glm::mat4& phys_transform)
{
	ASSERT(node->mesh);
	bool renderable = render_default;
	bool collidable = collide_default;

	std::string node_name = node->name;

	if (node_name.find("_!r!c") != std::string::npos) {
		renderable = true;
		collidable = true;
	}
	else if (node_name.find("_!nr!c") != std::string::npos) {
		renderable = false;
		collidable = true;
	}
	else if (node_name.find("_!r!nc") != std::string::npos) {
		renderable = true;
		collidable = false;
	}

	if (!renderable && !collidable)
		return;

	cgltf_mesh* mesh = node->mesh;

	if (collidable)
		append_collision_data2(model, data, mesh, materials, physics, phys_transform);
	if (!renderable)
		return;

	for (int i = 0; i < mesh->primitives_count; i++)
	{
		const cgltf_primitive& prim = mesh->primitives[i];

		MeshPart part;
		glGenVertexArrays(1, &part.vao);
		glBindVertexArray(part.vao);

		// index buffer
		//const tinygltf::Accessor& indicies_accessor = inputMod.accessors[prim.indices];
		cgltf_accessor* indicies_accessor = prim.indices;

		int index_buffer_index = MakeOrFindGpuBuffer2(model, true, indicies_accessor->buffer_view, data, buffer_view_to_buffer);
		ASSERT(model->buffers[index_buffer_index].target == GL_ELEMENT_ARRAY_BUFFER);
		model->buffers[index_buffer_index].Bind();
		part.element_offset = indicies_accessor->offset;
		part.element_count = indicies_accessor->count;
		part.element_type = cgltf_component_type_to_gl(indicies_accessor->component_type);
		part.base_vertex = 0;
		part.material_idx = cgltf_material_index(data, prim.material);

		glCheckError();

		// Now do all the attributes
		bool found_joints_attrib = false;
		for (int at_index=0; at_index <prim.attributes_count; at_index++)
		{
			cgltf_attribute& attribute = prim.attributes[at_index];
			cgltf_accessor& accessor = *attribute.data;

			//const tinygltf::Accessor& accessor = inputMod.accessors[attrb.second];
			//int byte_stride = accessor.ByteStride(inputMod.bufferViews[accessor.bufferView]);
			int byte_stride = accessor.stride;
			//int buffer_index = MakeOrFindGpuBuffer(model, accessor.bufferView, inputMod, buffer_view_to_buffer);
			int buffer_index = MakeOrFindGpuBuffer2(model, false, accessor.buffer_view, data, buffer_view_to_buffer);
			ASSERT(model->buffers[buffer_index].target == GL_ARRAY_BUFFER);
			model->buffers[buffer_index].Bind();

			int location = -1;
			if (strcmp(attribute.name,"POSITION")==0) location = POSITION_LOC;
			else if (strcmp(attribute.name,"TEXCOORD_0")== 0)location = UV_LOC;
			else if (strcmp(attribute.name,"TEXCOORD_1")== 0)location = UV2_LOC;
			else if (strcmp(attribute.name,"NORMAL")    == 0)location = NORMAL_LOC;
			else if (strcmp(attribute.name,"JOINTS_0")  == 0)location = JOINT_LOC;
			else if (strcmp(attribute.name,"WEIGHTS_0") == 0)location = WEIGHT_LOC;
			else if (strcmp(attribute.name,"COLOR_0")   == 0)location = COLOR_LOC;
			else if (strcmp(attribute.name,"TANGENT")   == 0) location = TANGENT_LOC;


			if (location == -1) continue;

			if (location == JOINT_LOC)
				found_joints_attrib = true;

			glEnableVertexAttribArray(location);
			if (location == JOINT_LOC) {
				glVertexAttribIPointer(location, accessor.type, cgltf_component_type_to_gl(accessor.component_type),
					byte_stride, (void*)accessor.offset);
			}
			else {
				glVertexAttribPointer(location, accessor.type, cgltf_component_type_to_gl(accessor.component_type),
					accessor.normalized, byte_stride, (void*)accessor.offset);
			}

			part.attributes |= (1 << location);

			glCheckError();
		}

		glBindVertexArray(0);


		if (!(part.attributes & (1 << POSITION_LOC)) || !(part.attributes & (1 << UV_LOC)) || !(part.attributes & (1 << NORMAL_LOC))) {
			sys_print("Model %s is missing nessecary vertex attributes\n", model->name.c_str());
		}

		model->parts.push_back(part);

		glCheckError();
	}


}


static Texture* LoadGltfImage(tinygltf::Image& i, tinygltf::Model& scene)
{
	tinygltf::BufferView& bv = scene.bufferViews[i.bufferView];
	tinygltf::Buffer& b = scene.buffers[bv.buffer];
	ASSERT(bv.byteStride == 0);

	return CreateTextureFromImgFormat(&b.data.at(bv.byteOffset), bv.byteLength, i.name);
}

void load_model_materials(std::vector<Game_Shader*>& materials, const std::string& fallbackname, tinygltf::Model& scene)
{
	for (int matidx = 0; matidx < scene.materials.size(); matidx++) {
		tinygltf::Material& mat = scene.materials[matidx];
		size_t find = mat.name.rfind('.');	// remove the .001 shit that blender adds
		if (find != std::string::npos) {
			mat.name = mat.name.substr(0, find);
		}

		Game_Shader* gs = mats.find_for_name(mat.name.c_str());
		if(!gs) {
			int baseindex = mat.pbrMetallicRoughness.baseColorTexture.index;
			if (baseindex != -1 && baseindex < scene.images.size()) {
				gs = mats.create_temp_shader((fallbackname + mat.name).c_str());
				gs->images[Game_Shader::BASE1] = LoadGltfImage(scene.images.at(mat.pbrMetallicRoughness.baseColorTexture.index), scene);
			}
			else
				gs = &mats.fallback;
		}
		materials.push_back(gs);
	}
}
static Texture* LoadGltfImage2(cgltf_image* i, cgltf_data* data)
{
	cgltf_buffer_view& bv = *i->buffer_view;
	cgltf_buffer& b = *bv.buffer;

	ASSERT(bv.stride == 0);
	uint8_t* buffer_bytes = (uint8_t*)b.data;

	return CreateTextureFromImgFormat(buffer_bytes + bv.offset, bv.size, i->name);
}

void load_model_materials2(std::vector<Game_Shader*>& materials, const std::string& fallbackname, cgltf_data* data)
{
	for (int matidx = 0; matidx < data->materials_count; matidx++) {
		cgltf_material& mat = data->materials[matidx];
		std::string mat_name = mat.name;
		size_t find = mat_name.rfind('.');	// remove the .001 shit that blender adds
		if (find != std::string::npos) {
			mat_name = mat_name.substr(0, find);
		}

		Game_Shader* gs = mats.find_for_name(mat_name.c_str());
		if (!gs) {
			if (mat.has_pbr_metallic_roughness) {
				cgltf_pbr_metallic_roughness& base = mat.pbr_metallic_roughness;
				if (base.base_color_texture.texture) {

					gs = mats.create_temp_shader((fallbackname + mat_name).c_str());
					gs->images[Game_Shader::BASE1] = LoadGltfImage2(base.base_color_texture.texture->image, data);
				}
			}

			if (!gs) {
				gs = &mats.fallback;
			}
		}
		materials.push_back(gs);
	}
}


#ifdef USE_CGLTF
static void RecursiveAddSkeleton2(std::map<std::string, int>& bone_to_index, cgltf_data* data, Model* m, cgltf_node* node)
{
	std::string name = node->name;
	if (bone_to_index.find(name) != bone_to_index.end())
	{
		int my_index = bone_to_index[name];
		for (int i = 0; i < node->children_count; i++) {
			cgltf_node* child = node->children[i];
			std::string cname = child->name;
			if (bone_to_index.find(cname) != bone_to_index.end()) {
				m->bones[bone_to_index[cname]].parent = my_index;
			}
		}
	}
	for (int i = 0; i < node->children_count; i++) {
		RecursiveAddSkeleton2(bone_to_index, data, m, node->children[i]);
	}
}

#endif

static void RecursiveAddSkeleton(std::map<std::string, int>& bone_to_index, tinygltf::Model& scene, Model* m, tinygltf::Node& node)
{
	if (bone_to_index.find(node.name) != bone_to_index.end())
	{
		int my_index = bone_to_index[node.name];
		for (int i = 0; i < node.children.size(); i++) {
			tinygltf::Node& child = scene.nodes[node.children[i]];
			if (bone_to_index.find(child.name) != bone_to_index.end()) {
				m->bones[bone_to_index[child.name]].parent = my_index;
			}
		}
	}
	for (int i = 0; i < node.children.size(); i++) {
		RecursiveAddSkeleton(bone_to_index, scene, m, scene.nodes[node.children[i]]);
	}
}

static void LoadGltfSkeleton(tinygltf::Model& scene, Model* model, tinygltf::Skin& skin)
{
	std::map<std::string, int> bone_to_index;
	ASSERT(skin.inverseBindMatrices != -1);
	tinygltf::Accessor& invbind_acc = scene.accessors[skin.inverseBindMatrices];
	tinygltf::BufferView& invbind_bv = scene.bufferViews[invbind_acc.bufferView];
	for (int i = 0; i < skin.joints.size(); i++) {
		tinygltf::Node& node = scene.nodes[skin.joints[i]];
		Bone b;
		b.parent = -1;
		float* start =(float*)(&scene.buffers[invbind_bv.buffer].data.at(invbind_bv.byteOffset) + sizeof(float)*16*i);
		b.invposematrix = glm::mat4(start[0],start[1],start[2],start[3],
									start[4],start[5],start[6],start[7],
									start[8],start[9],start[10],start[11],
									start[12],start[13],start[14],start[15]);
		b.posematrix = glm::inverse(glm::mat4(b.invposematrix));
		
		b.name_table_ofs = model->bone_string_table.size();
		for (auto c : node.name)
			model->bone_string_table.push_back(c);
		model->bone_string_table.push_back('\0');

		// needed when animations dont have any keyframes, cause gltf exports nothing when its euler angles ???
		b.rot = glm::quat_cast(glm::mat4(b.posematrix));

		bone_to_index.insert({ node.name, model->bones.size() });
		model->bones.push_back(b);
	}
	tinygltf::Scene& defscene = scene.scenes[scene.defaultScene];
	for (int i = 0; i < defscene.nodes.size(); i++) {
		RecursiveAddSkeleton(bone_to_index, scene, model, scene.nodes[defscene.nodes.at(i)]);
	}
}
#ifdef USE_CGLTF
static void LoadGltfSkeleton2(cgltf_data* data, Model* model, cgltf_skin* skin)
{
	std::map<std::string, int> bone_to_index;
	ASSERT(skin->inverse_bind_matrices != nullptr);
	//tinygltf::Accessor& invbind_acc = scene.accessors[skin.inverseBindMatrices];
	cgltf_accessor* invbind_acc = skin->inverse_bind_matrices;
	//tinygltf::BufferView& invbind_bv = scene.bufferViews[invbind_acc.bufferView];
	cgltf_buffer_view* invbind_bv = invbind_acc->buffer_view;

	uint8_t* byte_buffer = (uint8_t*)invbind_bv->buffer->data;
	for (int i = 0; i <skin->joints_count; i++) {
		//tinygltf::Node& node = scene.nodes[skin.joints[i]];
		cgltf_node* node = skin->joints[i];
		Bone b;
		b.parent = -1;
		//float* start = (float*)(&scene.buffers[invbind_bv.buffer].data.at(invbind_bv.byteOffset) + sizeof(float) * 16 * i);
		float* start = (float*)(&byte_buffer[invbind_bv->offset] + sizeof(float) * 16 * i);
		b.invposematrix = glm::mat4(start[0], start[1], start[2], start[3],
			start[4], start[5], start[6], start[7],
			start[8], start[9], start[10], start[11],
			start[12], start[13], start[14], start[15]);
		b.posematrix = glm::inverse(glm::mat4(b.invposematrix));

		b.name_table_ofs = model->bone_string_table.size();
		char* c = node->name;
		while (*c) {
			model->bone_string_table.push_back(*c);
			c++;//ebic!!
		}
		model->bone_string_table.push_back('\0');

		// needed when animations dont have any keyframes, cause gltf exports nothing when its euler angles ???
		b.rot = glm::quat_cast(glm::mat4(b.posematrix));
		bone_to_index.insert({ std::string(node->name), model->bones.size() });
		model->bones.push_back(b);
	}
	//tinygltf::Scene& defscene = scene.scenes[scene.defaultScene];
	cgltf_scene* defscene = data->scene;
	for (int i = 0; i < defscene->nodes_count; i++) {
		RecursiveAddSkeleton2(bone_to_index, data, model, defscene->nodes[i]);
		//RecursiveAddSkeleton(bone_to_index, scene, model, scene.nodes[defscene.nodes.at(i)]);
	}
}
#endif
static Animation_Set* LoadGltfAnimations(tinygltf::Model& scene, tinygltf::Skin& skin)
{
	Animation_Set* set = new Animation_Set;
	std::map<int, int> node_to_index;
	for (int i = 0; i < skin.joints.size(); i++) {
		node_to_index[skin.joints[i]] = i;
	}
	set->num_channels = skin.joints.size();
	for (int a = 0; a < scene.animations.size(); a++)
	{
		tinygltf::Animation& gltf_anim = scene.animations[a];

		Animation my_anim{};
		my_anim.name = std::move(gltf_anim.name);
		my_anim.channel_offset = set->channels.size();
		my_anim.pos_offset = set->positions.size();
		my_anim.rot_offset = set->rotations.size();
		my_anim.scale_offset = set->scales.size();
		set->channels.resize(set->channels.size() + skin.joints.size());
		for (int c = 0; c < gltf_anim.channels.size(); c++) {
			tinygltf::AnimationChannel& gltf_channel = gltf_anim.channels[c];
			int channel_idx = node_to_index[gltf_channel.target_node];
			AnimChannel& my_channel = set->channels[my_anim.channel_offset + channel_idx];
			
			int type = -1;
			if (gltf_channel.target_path == "translation") type = 0;
			else if (gltf_channel.target_path == "rotation") type = 1;
			else if (gltf_channel.target_path == "scale") type = 2;
			else continue;

			tinygltf::AnimationSampler& sampler = gltf_anim.samplers[gltf_channel.sampler];
			tinygltf::Accessor& timevals = scene.accessors[sampler.input];
			tinygltf::Accessor& vals = scene.accessors[sampler.output];
			ASSERT(timevals.count == vals.count);
			tinygltf::BufferView& time_bv = scene.bufferViews[timevals.bufferView];
			tinygltf::BufferView& val_bv = scene.bufferViews[vals.bufferView];
			ASSERT(time_bv.buffer == val_bv.buffer);
			tinygltf::Buffer& buffer = scene.buffers[time_bv.buffer];
			ASSERT(timevals.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);// just make life easier
			ASSERT(time_bv.byteStride == 0);

			my_anim.total_duration = glm::max(my_anim.total_duration, (float)timevals.maxValues.at(0));

			float* time_buffer = (float*)(&buffer.data.at(time_bv.byteOffset));
			if (type == 0) {
				my_channel.pos_start = set->positions.size();
				ASSERT(vals.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT && vals.type == TINYGLTF_TYPE_VEC3);
				ASSERT(val_bv.byteStride == 0);
				glm::vec3* pos_buffer = (glm::vec3*)(&buffer.data.at(val_bv.byteOffset));
				for (int t = 0; t < timevals.count; t++) {
					PosKeyframe pkf;
					pkf.time = time_buffer[t];
					pkf.val = pos_buffer[t];
					set->positions.push_back(pkf);
				}
				my_channel.num_positions = set->positions.size() - my_channel.pos_start;

			}
			else if (type == 1) {
				my_channel.rot_start = set->rotations.size();
				ASSERT(vals.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT && vals.type == TINYGLTF_TYPE_VEC4);
				ASSERT(val_bv.byteStride == 0);
				glm::quat* rot_buffer = (glm::quat*)(&buffer.data.at(val_bv.byteOffset));
				for (int t = 0; t < timevals.count; t++) {
					RotKeyframe rkf;
					rkf.time = time_buffer[t];
					rkf.val = rot_buffer[t];
					set->rotations.push_back(rkf);
				}
				my_channel.num_rotations = set->rotations.size() - my_channel.rot_start;
			}
			else if (type == 2) {
				my_channel.scale_start = set->scales.size();
				ASSERT(vals.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT && vals.type == TINYGLTF_TYPE_VEC3);
				ASSERT(val_bv.byteStride == 0);
				glm::vec3* scale_buffer = (glm::vec3*)(&buffer.data.at(val_bv.byteOffset));
				for (int t = 0; t < timevals.count; t++) {
					ScaleKeyframe skf;
					skf.time = time_buffer[t];
					skf.val = scale_buffer[t];
					set->scales.push_back(skf);
				}
				my_channel.num_scales = set->scales.size() - my_channel.scale_start;
			}
		}

		my_anim.num_pos = set->positions.size() - my_anim.pos_offset;
		my_anim.num_rot = set->rotations.size() - my_anim.rot_offset;
		my_anim.num_scale = set->scales.size() - my_anim.scale_offset;
		my_anim.fps = 1.0;
		set->clips.push_back(my_anim);
	}

	return set;
}

#ifdef USE_CGLTF
static Animation_Set* LoadGltfAnimations2(cgltf_data* data, cgltf_skin* skin)
{
	Animation_Set* set = new Animation_Set;
	std::map<int, int> node_to_index;

	for (int i = 0; i < skin->joints_count; i++) {
		node_to_index[cgltf_node_index(data,skin->joints[i])] = i;
	}
	set->num_channels = skin->joints_count;// skin.joints.size();
	for (int a = 0; a < data->animations_count; a++)
	{
		//tinygltf::Animation& gltf_anim = scene.animations[a];
		cgltf_animation* gltf_anim = &data->animations[a];

		Animation my_anim{};
		my_anim.name = gltf_anim->name;
		my_anim.channel_offset = set->channels.size();
		my_anim.pos_offset = set->positions.size();
		my_anim.rot_offset = set->rotations.size();
		my_anim.scale_offset = set->scales.size();
		set->channels.resize(set->channels.size() + skin->joints_count);
		for (int c = 0; c < gltf_anim->channels_count; c++) {
			//tinygltf::AnimationChannel& gltf_channel = gltf_anim.channels[c];
			cgltf_animation_channel* gltf_channel = &gltf_anim->channels[c];
			//int channel_idx = node_to_index[gltf_channel.target_node];
			int channel_idx = node_to_index[cgltf_node_index(data, gltf_channel->target_node)];
			AnimChannel& my_channel = set->channels[my_anim.channel_offset + channel_idx];

			int type = -1;
			//if (gltf_channel.target_path == "translation") type = 0;
			//else if (gltf_channel.target_path == "rotation") type = 1;
			//else if (gltf_channel.target_path == "scale") type = 2;
			//else continue;
			if (gltf_channel->target_path == cgltf_animation_path_type_translation) type = 0;
			else if (gltf_channel->target_path == cgltf_animation_path_type_rotation) type = 1;
			else if (gltf_channel->target_path == cgltf_animation_path_type_scale) type = 2;
			else continue;


			//tinygltf::AnimationSampler& sampler = gltf_anim.samplers[gltf_channel.sampler];
			cgltf_animation_sampler* sampler = gltf_channel->sampler;
			cgltf_accessor* timevals = sampler->input;
			//tinygltf::Accessor& timevals = scene.accessors[sampler.input];
			//tinygltf::Accessor& vals = scene.accessors[sampler.output];
			cgltf_accessor* vals = sampler->output;
			ASSERT(timevals->count == vals->count);
			//tinygltf::BufferView& time_bv = scene.bufferViews[timevals.bufferView];
			cgltf_buffer_view* time_bv = timevals->buffer_view;
			cgltf_buffer_view* val_bv = vals->buffer_view;
			//tinygltf::BufferView& val_bv = scene.bufferViews[vals.bufferView];
			ASSERT(time_bv->buffer == val_bv->buffer);
			//tinygltf::Buffer& buffer = scene.buffers[time_bv.buffer];
			cgltf_buffer* buffer = time_bv->buffer;
			ASSERT(timevals->component_type == cgltf_component_type_r_32f);// just make life easier
			ASSERT(time_bv->stride == 0);

			//my_anim.total_duration = glm::max(my_anim.total_duration, (float)timevals.maxValues.at(0));
			my_anim.total_duration = glm::max(my_anim.total_duration, (float)timevals->max[0]);

			char* buffer_byte_data = (char*)buffer->data;

			//float* time_buffer = (float*)(&buffer.data.at(time_bv.byteOffset));
			float* time_buffer = (float*)&buffer_byte_data[time_bv->offset];
			if (type == 0) {
				my_channel.pos_start = set->positions.size();
				ASSERT(vals->component_type == cgltf_component_type_r_32f && vals->type == cgltf_type_vec3);
				ASSERT(val_bv->stride == 0);
				//glm::vec3* pos_buffer = (glm::vec3*)(&buffer.data.at(val_bv.byteOffset));

				glm::vec3* pos_buffer = (glm::vec3*)&buffer_byte_data[val_bv->offset];

				for (int t = 0; t < timevals->count; t++) {
					PosKeyframe pkf;
					pkf.time = time_buffer[t];
					pkf.val = pos_buffer[t];
					set->positions.push_back(pkf);
				}
				my_channel.num_positions = set->positions.size() - my_channel.pos_start;

			}
			else if (type == 1) {
				my_channel.rot_start = set->rotations.size();
				ASSERT(vals->component_type == cgltf_component_type_r_32f && vals->type == cgltf_type_vec4);
				ASSERT(val_bv->stride == 0);
				//glm::quat* rot_buffer = (glm::quat*)(&buffer.data.at(val_bv.byteOffset));
				glm::quat* rot_buffer = (glm::quat*)&buffer_byte_data[val_bv->offset];
				for (int t = 0; t < timevals->count; t++) {
					RotKeyframe rkf;
					rkf.time = time_buffer[t];
					rkf.val = rot_buffer[t];
					set->rotations.push_back(rkf);
				}
				my_channel.num_rotations = set->rotations.size() - my_channel.rot_start;
			}
			else if (type == 2) {
				my_channel.scale_start = set->scales.size();
				ASSERT(vals->component_type == cgltf_component_type_r_32f && vals->type == cgltf_type_vec3);
				ASSERT(val_bv->stride == 0);
				//glm::vec3* scale_buffer = (glm::vec3*)(&buffer.data.at(val_bv.byteOffset));
				glm::vec3* scale_buffer = (glm::vec3*)&buffer_byte_data[val_bv->offset];
				for (int t = 0; t < timevals->count; t++) {
					ScaleKeyframe skf;
					skf.time = time_buffer[t];
					skf.val = scale_buffer[t];
					set->scales.push_back(skf);
				}
				my_channel.num_scales = set->scales.size() - my_channel.scale_start;
			}
		}

		my_anim.num_pos = set->positions.size() - my_anim.pos_offset;
		my_anim.num_rot = set->rotations.size() - my_anim.rot_offset;
		my_anim.num_scale = set->scales.size() - my_anim.scale_offset;
		my_anim.fps = 1.0;
		set->clips.push_back(my_anim);
	}

	return set;
}
#endif

static void traverse_model_nodes(Model* model, tinygltf::Model& scene, tinygltf::Node& node, std::map<int, int>& buffer_view_to_buffer)
{
	if (node.mesh != -1)
		add_node_mesh_to_model(model, scene, node, buffer_view_to_buffer, true, false, model->materials, nullptr, glm::mat4(1));
	for (int i = 0; i < node.children.size(); i++)
		traverse_model_nodes(model, scene, scene.nodes[node.children[i]], buffer_view_to_buffer);
}

#ifdef USE_CGLTF

static void traverse_model_nodes2(Model* model, cgltf_data* data, cgltf_node* node, std::map<int, int>& buffer_view_to_buffer)
{
	if (node->mesh)
		add_node_mesh_to_model2(model, data, node, buffer_view_to_buffer, true, false, model->materials, nullptr, glm::mat4(1));
	for (int i = 0; i < node->children_count; i++)
		traverse_model_nodes2(model, data, node->children[i], buffer_view_to_buffer);
}

static bool load_gltf_model2(const std::string& filepath, Model* model)
{
	cgltf_options options = {};
	cgltf_data* data = NULL;

	File_Buffer* infile = Files::open(filepath.c_str());
	if (!infile) {
		printf("no such model %s\n", filepath.c_str());
		return false;
	}

	cgltf_result result = cgltf_parse(&options, infile->buffer, infile->length, &data);
	Files::close(infile);

	//cgltf_result result = cgltf_parse_file(&options, filepath.c_str(), &data);
	if (result != cgltf_result_success)
	{
		printf("Couldn't load gltf model\n");
		return false;
	}

	cgltf_load_buffers(&options, data, filepath.c_str());

	if (data->skins_count >= 1)
		LoadGltfSkeleton2(data, model, &data->skins[0]);
	if (data->animations_count >= 1 && data->skins_count >= 1) {
		Animation_Set* set = LoadGltfAnimations2(data, &data->skins[0]);
		model->animations = std::unique_ptr<Animation_Set>(set);
	}

	load_model_materials2(model->materials, model->name, data);
	std::map<int, int> buf_view_to_buffers;
	cgltf_scene* scene = data->scene;
	for (int i = 0; i < scene->nodes_count; i++) {
		cgltf_node* node = scene->nodes[i];
		traverse_model_nodes2(model, data, node, buf_view_to_buffers);
	}
	if (model->collision)
		model->collision->build();


	cgltf_free(data);

	return true;
}
#endif

static bool DoLoadGltfModel(const std::string& filepath, Model* model)
{
	tinygltf::Model scene;
	tinygltf::TinyGLTF loader;
	std::string errStr;
	std::string warnStr;
	bool res = loader.LoadBinaryFromFile(&scene, &errStr, &warnStr, filepath);
	if (!res) {
		printf("Couldn't load gltf model: %s\n", errStr.c_str());
		return false;
	}

	if (scene.skins.size() >= 1)
		LoadGltfSkeleton(scene, model, scene.skins[0]);
	if (scene.animations.size() >= 1 && scene.skins.size() >= 1) {
		Animation_Set* set = LoadGltfAnimations(scene, scene.skins[0]);
		model->animations = std::unique_ptr<Animation_Set>(set);
	}

	load_model_materials(model->materials, model->name, scene);
	std::map<int, int> buf_view_to_buffers;
	tinygltf::Scene& defscene = scene.scenes[scene.defaultScene];
	for (int i = 0; i < defscene.nodes.size(); i++) {
		traverse_model_nodes(model, scene, scene.nodes.at(defscene.nodes.at(i)), buf_view_to_buffers);
	}
	if (model->collision)
		model->collision->build();



	return res;
}


void Model::GpuBuffer::Bind()
{
	glBindBuffer(target, handle);
}

int Model::BoneForName(const char* name) const
{
	for (int i = 0; i < bones.size(); i++) {
		if (strcmp(&bone_string_table.at(bones[i].name_table_ofs), name) == 0)
			return i;
	}
	return -1;
}

void FreeLoadedModels()
{
	for (int i = 0; i < models.size(); i++) {
		Model* m = models[i];
		printf("Freeing model: %s\n", m->name.c_str());
		for (int p = 0; p < m->parts.size(); p++) {
			glDeleteVertexArrays(1, &m->parts[p].vao);
		}
		for (int b = 0; b < m->buffers.size(); b++) {
			glDeleteBuffers(1, &m->buffers[b].handle);
		}

		delete m;
	}
	models.clear();
}
void ReloadModel(Model* m)
{
	std::string path;
	path.reserve(256);
	path += model_folder_path;
	path += m->name;

	for (int p = 0; p < m->parts.size(); p++) {
		glDeleteVertexArrays(1, &m->parts[p].vao);
	}
	for (int b = 0; b < m->buffers.size(); b++) {
		glDeleteBuffers(1, &m->buffers[b].handle);
	}

	*m = Model{};
	bool res = load_gltf_model2(path, m);
}

void benchmark_gltf()
{
	const int RUNS = 10;
	std::string filepath = "./Data/Models/arms.glb";
	double start = GetTime();
	for (int i = 0; i < RUNS; i++) {
		Model m;
		load_gltf_model2(filepath, &m);
	}
	printf("cgltf %f\n", GetTime() - start);
	start = GetTime();

	for (int i = 0; i < RUNS; i++) {
		Model m;
		DoLoadGltfModel(filepath, &m);
	}
	printf("tinygltf %f\n", GetTime() - start);
}

Model* FindOrLoadModel(const char* filename)
{
	for (int i = 0; i < models.size(); i++) {
		if (models[i]->name == filename)
			return models[i];
	}

	std::string path;
	path.reserve(256);
	path += model_folder_path;
	path += filename;

	Model* model = new Model;
	model->name = filename;
	bool res = load_gltf_model2(path, model);
	if (!res) {
		delete model;
		return nullptr;
	}

	models.push_back(model);

	return model;
}

struct Attribute_Info { int type; int component; int stride; };
Attribute_Info at_info[Model_Manager::NUM_ATTRIBUTES]{
	{3,GL_FLOAT, 12},
	{2, GL_FLOAT, 8},
	{3, GL_FLOAT,12},
	{4, GL_UNSIGNED_BYTE,4}
};

void Model_Manager::check_vaos()
{
	for (int i = 0; i < NUM_VERT_FORMATS; i++) {
		if (formats[i].vao_16 == 0) {
			glGenVertexArrays(1, &formats[i].vao_16);
		}
		if (formats[i].vao_32 == 0) {
			glGenVertexArrays(1, &formats[i].vao_32);
		}
		for(int k=0;k<2;k++) {
			if (k == 0)
				glBindVertexArray(formats[i].vao_16);
			else
				glBindVertexArray(formats[i].vao_32);
			for (int j = 0; j < NUM_ATTRIBUTES; j++) {
				auto& b = formats[i].buffers[j];
				if (b.dirty) {
					glBindBuffer(GL_ARRAY_BUFFER, b.id);

					glEnableVertexAttribArray(j);
					if (j == INDICIES) {
						glVertexAttribIPointer(j, at_info[j].type, at_info[j].component,
							at_info[j].stride, (void*)0);
					}
					else {
						glVertexAttribPointer(j, at_info[j].type, at_info[j].component, GL_FALSE,
							at_info[j].stride, (void*)0);
					}

					if (k == 1) b.dirty = false;
				}
			}

			glBindVertexArray(0);
		}
		if (index[INT16].dirty) {
			glBindVertexArray(formats[i].vao_16);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index[INT16].id);
			index[INT16].dirty = false;
			glBindVertexArray(0);
		}
		if (index[INT32].dirty) {
			glBindVertexArray(formats[i].vao_32);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index[INT32].id);
			index[INT32].dirty = false;
			glBindVertexArray(0);
		}

	}
}

void Model_Manager::Buffer::append_data(void* d, int in_stride, uint32_t in_len, int out_stride)
{
	ASSERT(in_stride == out_stride);
	// FIXME:
	uint32_t in_size_bytes = in_len * out_stride;
	uint32_t target = is_index_target ? GL_ELEMENT_ARRAY_BUFFER : GL_ARRAY_BUFFER;
	if (id == 0) {
		glGenBuffers(1, &id);
		glBindBuffer(target, id);
		size = glm::max((int)pow(ceil(log2f(in_size_bytes)), 2.f), 16384);
		glBufferData(target, size, NULL, GL_STATIC_DRAW);

		dirty = true;
	}
	if (in_size_bytes + used > size) {
		if (used != 0) {
			uint32_t next_buffer;
			uint32_t next_size = pow(ceil(log2f(used + in_size_bytes)),2.f);
			glGenBuffers(1, &next_buffer);
			glBindBuffer(is_index_target, next_buffer);
			glBufferData(target, next_size, NULL, GL_STATIC_DRAW);
			glCopyBufferSubData(id, next_buffer, 0, 0, used);

			glDeleteBuffers(1, &id);
			id = next_buffer;
			size = next_size;

			dirty = true;
		}
	}
	glBufferSubData(target, used, in_size_bytes, d);
	used += in_size_bytes;

	glBindBuffer(target, 0);
}