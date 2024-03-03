#include "Model.h"
#include "Memory.h"
#include <vector>
#include <map>
#include "glad/glad.h"
#include "glm/gtc/type_ptr.hpp"

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


// screw it, im making this generalized

enum Vertex_Attributes
{
	ATTRIBUTE_POS = 0,
	ATTRIBUTE_UV = 1,
	ATTRIBUTE_NORMAL = 2,
	ATTRIBUTE_JOINT = 3,
	ATTRIBUTE_WEIGHT = 4,
	ATTRIBUTE_COLOR = 5,
	ATTRIBUTE_UV2 = 6,
	ATTRIBUTE_TANGENT = 7,
	MAX_ATTRIBUTES,
};

enum Component_Type
{
	CT_INVALID = cgltf_component_type_invalid,
	CT_S8 = cgltf_component_type_r_8,
	CT_U8 = cgltf_component_type_r_8u,
	CT_S16 = cgltf_component_type_r_16,
	CT_U16 = cgltf_component_type_r_16u,
	CT_U32 = cgltf_component_type_r_32u,
	CT_FLOAT = cgltf_component_type_r_32f,
};

class Format_Descriptor
{
public:
	Format_Descriptor(Component_Type type, int count, bool normalized) :
		is_normalized(normalized), count(count), type(type){}

	Format_Descriptor(cgltf_component_type type, cgltf_type count, bool normalized) :
		type((Component_Type)type), count(count), is_normalized(normalized) {
		ASSERT(count < cgltf_type_mat2 && count != cgltf_type_invalid && type != cgltf_component_type_invalid);
	}
	bool operator==(const Format_Descriptor& other) const {
		return is_normalized == other.is_normalized && count == other.count && type == other.type;
	}

	int get_size() {
		const int bytes[7] = { 0,1,1,2,2,4,4 };
		return bytes[type]*count;
	}
	void convert_this_from_that(char* input_buf, Format_Descriptor& inputfmt, char* output_buf) {
		if (count <= inputfmt.count && type == inputfmt.type) {
			memcpy(output_buf, input_buf, get_size());
			return;
		}
		
		if (inputfmt.type == CT_FLOAT || inputfmt.is_normalized)
			convert_this_from_that_float(input_buf, inputfmt, output_buf);
		else
			convert_this_from_that_int(input_buf, inputfmt, output_buf);
	}
#define CAST_TO_AND_INDEX(index, type, buffer) ((type*)(buffer))[index]
	void convert_this_from_that_int(char* input_buf, Format_Descriptor& inputfmt, char* output_buf) {
		glm::vec4 input;
		for (int i = 0; i < inputfmt.count; i++) {
			if (inputfmt.type == CT_S8)
				input[i] = input_buf[i];
			else if (inputfmt.type == CT_U8)
				input[i] = CAST_TO_AND_INDEX(i, uint8_t, input_buf);
			else if (inputfmt.type == CT_S16)
				input[i] = CAST_TO_AND_INDEX(i, short, input_buf);
			else if (inputfmt.type == CT_U16)
				input[i] = CAST_TO_AND_INDEX(i, uint16_t, input_buf);
			else if (inputfmt.type == CT_U32)
				input[i] = CAST_TO_AND_INDEX(i, uint32_t, input_buf);
			else
				assert(0);
		}

		for (int i = 0; i < count; i++) {
			if (type == CT_S8)
				output_buf[i] = input[i];
			else if (type == CT_U8)
				CAST_TO_AND_INDEX(i, uint8_t, output_buf) = input[i];
			else if (type == CT_S16)
				CAST_TO_AND_INDEX(i, short, output_buf) = input[i];
			else if (type == CT_U16)
				CAST_TO_AND_INDEX(i, uint16_t, output_buf) = input[i];
			else if (type == CT_U32)
				CAST_TO_AND_INDEX(i, uint32_t, output_buf) = input[i];
			else
				assert(0);
		}
	}

	void convert_this_from_that_float(char* input_buf, Format_Descriptor& inputfmt, char* output_buf) {
		assert(is_normalized);

		glm::vec4 input;
		if (inputfmt.type == CT_FLOAT) {
			float* input_buf_f = (float*)input_buf;
			for (int i = 0; i < inputfmt.count; i++) {
				input[i] = input_buf_f[i];
			}
		}
		else {
			for (int i = 0; i < inputfmt.count; i++) {
				if (type == CT_U8) {
					int normalized = CAST_TO_AND_INDEX(i, uint8_t, input_buf);
					input[i] = normalized * 255.f;
				}
				else if (type == CT_U16) {
					int normalized = CAST_TO_AND_INDEX(i, uint16_t, input_buf);
					input[i] = normalized * (float)UINT16_MAX;
				}
				else
					assert(0);
			}
		}
		
		for (int i = 0; i < count; i++) {
			float f = input[i];
			if (type == CT_U8) {
				int normalized = glm::clamp(int(f * 255), 0, (int)255);
				CAST_TO_AND_INDEX(i, uint8_t, output_buf) = normalized;
			}
			else if (type == CT_U16) {
				int normalized = glm::clamp(int(f * UINT16_MAX), 0, (int)UINT16_MAX);
				CAST_TO_AND_INDEX(i, uint16_t, output_buf) = normalized;
			}
			else if (type == CT_S16){
				int normalized = glm::clamp(int(f * INT16_MAX), (int)INT16_MIN, (int)INT16_MAX);
				CAST_TO_AND_INDEX(i, int16_t, output_buf) = normalized;
			}
			else
				assert(0);
		}
	}

	GLenum get_gl_type() {
		static GLenum to_glenum[] = {
			0,
			GL_BYTE,
			GL_UNSIGNED_BYTE,
			GL_SHORT,
			GL_UNSIGNED_SHORT,
			GL_UNSIGNED_INT,
			GL_FLOAT
		};
		return to_glenum[type];
	}

	void opengl_set_vertex_attribute(int location)
	{
		glEnableVertexAttribArray(location);
		GLenum type = get_gl_type();
		int stride = get_size();
		if (this->type == CT_FLOAT || is_normalized) {
			glVertexAttribPointer(location, count,type,
				is_normalized, stride, (void*)0);
		}
		else {
			glVertexAttribIPointer(location, count, type,
				stride, (void*)0);
		}
	}


	bool is_normalized = false;
	int count = 1;
	Component_Type type = CT_INVALID;
};
#undef CAST_TO_AND_INDEX

Format_Descriptor vertex_attribute_formats[MAX_ATTRIBUTES] =
{
	Format_Descriptor(CT_FLOAT, 3, false),	// position
	Format_Descriptor(CT_FLOAT, 2, false),	// uv
	Format_Descriptor(CT_S16, 3, true),	// normal
	Format_Descriptor(CT_U8, 4, false),		// joint
	Format_Descriptor(CT_U8, 4, true),	// weight
	Format_Descriptor(CT_U8, 3, true),		// color
	Format_Descriptor(CT_FLOAT, 2, false),		// uv2
	Format_Descriptor(CT_S16, 3, true) // tangent
};
Format_Descriptor index_attribute_format = Format_Descriptor(CT_U16, 1, false);

class Vertex_Descriptor
{
public:
	Vertex_Descriptor(int default_size, int mask_of_attributes)
		: mask(mask_of_attributes), default_buffer_size(default_size){}

	void set_all_contained_attributes(Game_Mod_Manager::Gpu_Buffer* buffers) {
		for (int i = 0; i < MAX_ATTRIBUTES; i++) {
			if (mask & (1 << i)) {
				glBindBuffer(GL_ARRAY_BUFFER, buffers[i].handle);
				vertex_attribute_formats[i].opengl_set_vertex_attribute(i);
				glBindBuffer(GL_ARRAY_BUFFER, 0);
			}
		}
	}

	void generate_buffers(Game_Mod_Manager::Gpu_Buffer* out_buffers) {
		for (int i = 0; i < MAX_ATTRIBUTES; i++) {
			if (mask & (1 << i)) {
				glGenBuffers(1, &out_buffers[i].handle);
				glBindBuffer(GL_ARRAY_BUFFER, out_buffers[i].handle);
				glBufferData(GL_ARRAY_BUFFER,
					default_buffer_size * vertex_attribute_formats[i].get_size(),
					nullptr,
					GL_STATIC_DRAW);
				out_buffers[i].allocated = default_buffer_size*vertex_attribute_formats[i].get_size();
				out_buffers[i].target = GL_ARRAY_BUFFER;
				glBindBuffer(GL_ARRAY_BUFFER, 0);
			}
		}
	}

	int default_buffer_size = 1'000'000;
	int mask = 0;
};

#define TO_MASK(x) (1<<x)
Vertex_Descriptor vertex_buffer_formats[Game_Mod_Manager::NUM_FMT] =
{
	// skinned format
	Vertex_Descriptor(
		100'000,
		TO_MASK(ATTRIBUTE_POS) |
		TO_MASK(ATTRIBUTE_UV) |
		TO_MASK(ATTRIBUTE_NORMAL) |
		TO_MASK(ATTRIBUTE_JOINT) |
		TO_MASK(ATTRIBUTE_WEIGHT) |
		TO_MASK(ATTRIBUTE_TANGENT)),
	// static format
	Vertex_Descriptor(
		500'000,
		TO_MASK(ATTRIBUTE_POS) |
		TO_MASK(ATTRIBUTE_UV) |
		TO_MASK(ATTRIBUTE_NORMAL)|
		TO_MASK(ATTRIBUTE_TANGENT)),
	// static plus format
	Vertex_Descriptor(
		500'000,
		TO_MASK(ATTRIBUTE_POS) |
		TO_MASK(ATTRIBUTE_UV) |
		TO_MASK(ATTRIBUTE_NORMAL) |
		TO_MASK(ATTRIBUTE_TANGENT) |
		TO_MASK(ATTRIBUTE_COLOR) |
		TO_MASK(ATTRIBUTE_UV2)),
};
static const int default_index_buffer_size = 3'000'000;

bool Mesh::has_lightmap_coords() const
{
	return attributes & (1 << UV2_LOC);
}

bool Mesh::has_bones() const
{
	return attributes & (1 << JOINT_LOC);
}

bool Mesh::has_colors() const
{
	return attributes & (1 << COLOR_LOC);
}

bool Mesh::has_tangents() const
{
	return attributes & (1 << TANGENT_LOC);
}


bool write_out2(vector<char>& out, const cgltf_accessor* in, Format_Descriptor outfmt)
{
	Format_Descriptor infmt(in->component_type, in->type, in->normalized);
	int new_bytes = outfmt.get_size() * in->count;
	int start = out.size();
	char* in_buffer_start = (char*)in->buffer_view->buffer->data + in->buffer_view->offset + in->offset;
	out.resize(start + new_bytes);
	if (infmt == outfmt && in->stride == infmt.get_size()) {
		memcpy(out.data() + start, in_buffer_start, new_bytes);
		return true;
	}

	int out_stride = outfmt.get_size();
	int in_stride = in->stride;
	for (int i = 0; i < in->count; i++) {
		outfmt.convert_this_from_that(in_buffer_start + i*in_stride, infmt, out.data() + start + out_stride*i);
	}
	return true;
}

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


void append_collision_data3(unique_ptr<Physics_Mesh>& phys, cgltf_data* data, cgltf_mesh* mesh, std::vector<Game_Shader*>& materials,
	const glm::mat4& transform)
{
	if (!phys)
		phys = std::make_unique<Physics_Mesh>();

	Physics_Mesh* pm = phys.get();

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


bool add_node_mesh_to_new_mesh(
	Mesh& outmesh,
	cgltf_data* data, 
	cgltf_node* node,
	bool render_default, 
	bool collide_default, 
	vector<Game_Shader*>& materials,
	unique_ptr<Physics_Mesh>& physics,
	const glm::mat4& phys_transform)
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
		return false;

	cgltf_mesh* mesh = node->mesh;

	if (collidable)
		append_collision_data3(physics, data, mesh, materials, phys_transform);
	if (!renderable)
		return false;

	for (int i = 0; i < mesh->primitives_count; i++)
	{
		const cgltf_primitive& prim = mesh->primitives[i];

		Submesh part;

		const cgltf_accessor* indicies_accessor = prim.indices;

		// get base vertex of this new mesh part, uses position vertex buffer to determine size
		part.base_vertex = outmesh.data.buffers[ATTRIBUTE_POS].size() / vertex_attribute_formats[ATTRIBUTE_POS].get_size();
		part.element_offset = indicies_accessor->offset + outmesh.data.indicies.size();
		part.element_count = indicies_accessor->count;
		
		// add indicies to the raw mesh buffer, make sure type is unsigned short (the only type allowed for this engine)
		bool good = write_out2(outmesh.data.indicies, indicies_accessor, index_attribute_format);
		if (!good) {
			printf("Bad model index buffer format\n");
			return false;
		}
	
		part.material_idx = -1;
		if (prim.material)
			part.material_idx = cgltf_material_index(data, prim.material);

		// Now do all the attributes of this mesh
		bool found_joints_attrib = false;
		for (int at_index = 0; at_index < prim.attributes_count; at_index++)
		{
			cgltf_attribute& attribute = prim.attributes[at_index];
			cgltf_accessor& accessor = *attribute.data;
			int byte_stride = accessor.stride;

			int location = -1;
			if (strcmp(attribute.name, "POSITION") == 0) location = ATTRIBUTE_POS;
			else if (strcmp(attribute.name, "TEXCOORD_0") == 0)location = ATTRIBUTE_UV;
			else if (strcmp(attribute.name, "TEXCOORD_1") == 0)location = ATTRIBUTE_UV2;
			else if (strcmp(attribute.name, "NORMAL") == 0)location = ATTRIBUTE_NORMAL;
			else if (strcmp(attribute.name, "JOINTS_0") == 0)location = ATTRIBUTE_JOINT;
			else if (strcmp(attribute.name, "WEIGHTS_0") == 0)location = ATTRIBUTE_WEIGHT;
			else if (strcmp(attribute.name, "COLOR_0") == 0)location = ATTRIBUTE_COLOR;
			else if (strcmp(attribute.name, "TANGENT") == 0) location = ATTRIBUTE_TANGENT;

			if (location == -1) continue;

			if (location == ATTRIBUTE_POS) {
				outmesh.aabb = bounds_union(outmesh.aabb, glm::vec3(accessor.min[0], accessor.min[1], accessor.min[2]));
				outmesh.aabb = bounds_union(outmesh.aabb, glm::vec3(accessor.max[0], accessor.max[1], accessor.max[2]));
			}

			// write out data to raw buffer, this will be uploaded later
			bool good = write_out2(outmesh.data.buffers[location], &accessor, vertex_attribute_formats[location]);
			if (!good) {
				printf("Bad vertex format for model, skipping mesh\n");
				return false;
			}

			outmesh.attributes |= (1 << location);

		}
		if (!(outmesh.attributes & (1 << ATTRIBUTE_POS)) || 
			!(outmesh.attributes & (1 << ATTRIBUTE_UV)) || 
			!(outmesh.attributes & (1 << ATTRIBUTE_NORMAL)) ||
			!(outmesh.attributes & (1 << ATTRIBUTE_TANGENT))) {
			sys_print("Model %s is missing nessecary vertex attributes\n", mesh->name);
		}

		outmesh.parts.push_back(part);
	}

	return true;
}

static Texture* LoadGltfImage2(cgltf_image* i, cgltf_data* data)
{
	cgltf_buffer_view& bv = *i->buffer_view;
	cgltf_buffer& b = *bv.buffer;

	ASSERT(bv.stride == 0);
	uint8_t* buffer_bytes = (uint8_t*)b.data;

	const char* name = i->name;
	if (!name) name = "";

	return CreateTextureFromImgFormat(buffer_bytes + bv.offset, bv.size,name, false);
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
					gs->images[Game_Shader::DIFFUSE] = LoadGltfImage2(base.base_color_texture.texture->image, data);
					if (base.metallic_roughness_texture.texture) {
						gs->images[Game_Shader::ROUGHNESS] = LoadGltfImage2(base.metallic_roughness_texture.texture->image, data);
					}
					if (mat.normal_texture.texture) {
						gs->images[Game_Shader::NORMAL] = LoadGltfImage2(mat.normal_texture.texture->image, data);
					}
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

static void RecursiveAddSkeleton3(std::unordered_map<std::string, int>& bone_to_index, cgltf_data* data, Model* m, cgltf_node* node)
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
		RecursiveAddSkeleton3(bone_to_index, data, m, node->children[i]);
	}
}
#endif

#ifdef USE_CGLTF

static void LoadGltfSkeleton3(cgltf_data* data, Model* model, cgltf_skin* skin)
{
	std::unordered_map<std::string, int> bone_to_index;
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

		b.name = node->name;

		// needed when animations dont have any keyframes, cause gltf exports nothing when its euler angles ???
		b.rot = glm::quat_cast(glm::mat4(b.posematrix));
		bone_to_index.insert({ std::string(node->name), model->bones.size() });
		model->bones.push_back(b);
	}
	//tinygltf::Scene& defscene = scene.scenes[scene.defaultScene];
	cgltf_scene* defscene = data->scene;
	for (int i = 0; i < defscene->nodes_count; i++) {
		RecursiveAddSkeleton3(bone_to_index, data, model, defscene->nodes[i]);
		//RecursiveAddSkeleton(bone_to_index, scene, model, scene.nodes[defscene.nodes.at(i)]);
	}
}
#endif

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


#ifdef USE_CGLTF

static void traverse_model_nodes3(Model* model, cgltf_data* data, cgltf_node* node)
{
	if (node->mesh) {
		bool good = add_node_mesh_to_new_mesh(model->mesh, data, node, true, false, model->mats, model->collision, glm::mat4(1));
	}
	for (int i = 0; i < node->children_count; i++)
		traverse_model_nodes3(model, data, node->children[i]);
}

static void traverse_model_nodes3_prefab(Prefab_Model* model, cgltf_data* data, 
	cgltf_node* node, glm::mat4 global_transform, std::unordered_map<int,int> cgltf_mesh_to_mesh, 
	Game_Mod_Manager::prefab_callback callback, void* callback_data)
{
	glm::mat4 local_transform = glm::mat4(1);
	if (node->has_matrix) {
		float* m = node->matrix;
		local_transform = glm::mat4(m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7],
			m[8], m[9], m[10], m[11], m[12], m[13], m[14], m[15]);
	}
	else {
		glm::vec3 translation = glm::vec3(0.f);
		glm::vec3 scale = glm::vec3(1.f);
		glm::quat rot = glm::quat(1.f, 0.f, 0.f, 0.f);
		if (node->has_translation)
			translation = glm::make_vec3<float>(node->translation);
		if (node->has_rotation)
			rot = glm::make_quat<float>(node->rotation);
		if (node->has_scale)
			scale = glm::make_vec3<float>(node->scale);
		local_transform = glm::translate(glm::mat4(1), translation);
		local_transform = local_transform * glm::mat4_cast(rot);
		local_transform = glm::scale(local_transform, scale);
	}
	global_transform = global_transform * local_transform;

	if (callback)
		callback(callback_data, data, node, global_transform);

	if (node->mesh) {
		int mesh_index = -1;
		auto find = cgltf_mesh_to_mesh.find(cgltf_mesh_index(data, node->mesh));
		if (find != cgltf_mesh_to_mesh.end())
			mesh_index = find->second;

		if (mesh_index == -1) {
			model->meshes.push_back(Mesh());
			bool good = add_node_mesh_to_new_mesh(model->meshes.back(), data, node, 
				true, false, model->mats, model->physics, global_transform);
			if (good) {
				mesh_index = model->meshes.size() - 1;
				cgltf_mesh_to_mesh[cgltf_mesh_index(data, node->mesh)] = mesh_index;
			}
			else
				model->meshes.pop_back();
		}
		if (mesh_index != -1) {
			Prefab_Model::Node node;
			node.mesh_idx = mesh_index;
			node.transform = global_transform;
			model->nodes.push_back(node);
		}
	}

	for (int i = 0; i < node->children_count; i++)
		traverse_model_nodes3_prefab(model, data, node->children[i], global_transform, 
			cgltf_mesh_to_mesh, callback, callback_data);
}

static bool load_gltf_model3(const std::string& filepath, Model* model)
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
		LoadGltfSkeleton3(data, model, &data->skins[0]);
	if (data->animations_count >= 1 && data->skins_count >= 1) {
		Animation_Set* set = LoadGltfAnimations2(data, &data->skins[0]);
		model->animations = std::unique_ptr<Animation_Set>(set);
	}

	load_model_materials2(model->mats, model->name, data);
	cgltf_scene* scene = data->scene;
	for (int i = 0; i < scene->nodes_count; i++) {
		cgltf_node* node = scene->nodes[i];
		traverse_model_nodes3(model, data, node);
	}

	if (model->collision)
		model->collision->build();

	// ensure all materials arent null
	bool appended_null_material = false;
	for(int i=0;i<model->mesh.parts.size(); i++) {
		if (model->mesh.parts[i].material_idx == -1) {
			if (!appended_null_material) {
				model->mats.push_back(&mats.fallback);
				appended_null_material = true;
			}
			model->mesh.parts[i].material_idx = model->mats.size() - 1;
		}
	}

	cgltf_free(data);

	return true;
}

static bool load_gltf_prefab(const std::string& filepath, Prefab_Model* model, 
	Game_Mod_Manager::prefab_callback callback, void* callback_data)
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
	load_model_materials2(model->mats, model->name, data);

	// i do it this way since theres some collision mesh branching, instead of just looping through cgltf meshes
	std::unordered_map<int, int> cgltf_mesh_to_my_mesh;
	cgltf_scene* scene = data->scene;
	for (int i = 0; i < scene->nodes_count; i++) {
		cgltf_node* node = scene->nodes[i];
		traverse_model_nodes3_prefab(model, data, node, glm::mat4(1), cgltf_mesh_to_my_mesh, callback, callback_data);
	}

	// ensure all materials arent null
	bool appended_null_material = false;
	for (int i = 0; i < model->meshes.size(); i++) {
		for (int j = 0; j < model->meshes[i].parts.size(); j++) {
			if (model->meshes[i].parts[j].material_idx == -1) {
				if (!appended_null_material) {
					model->mats.push_back(&mats.fallback);
					appended_null_material = true;
				}
				model->meshes[i].parts[j].material_idx = model->mats.size() - 1;
			}
		}
	}

	cgltf_free(data);

	return true;
}

#endif


int Model::bone_for_name(const char* name) const
{
	for (int i = 0; i < bones.size(); i++) {
		if (bones[i].name == name)
			return i;
	}
	return -1;
}

Game_Mod_Manager mods;

Model* FindOrLoadModel(const char* filename)
{
	return mods.find_or_load(filename);
}


#include "Config.h"
DECLARE_ENGINE_CMD_CAT("gpu.", print_vertex_usage)
{
	mods.print_usage();
}

#include <algorithm>

void Game_Mod_Manager::compact_memory()
{
	float start = GetTime();

	vector<Mesh*> indexlist;
	vector<Mesh*> vertexlist[3];
	vector<uint32_t> fixups[3];
	for (auto& model : models) {
		indexlist.push_back({ &model.second->mesh });
		vertexlist[model.second->mesh.format].push_back(&model.second->mesh);
	}
	for (auto& prefab : prefabs)
		for (int i = 0; i < prefab.second->meshes.size(); i++) {
			indexlist.push_back({ &prefab.second->meshes[i] });
			vertexlist[prefab.second->meshes[i].format].push_back(&prefab.second->meshes[i]);
		}
	for (int i = 0; i < 3; i++)fixups[i].resize(vertexlist[i].size());


	std::sort(indexlist.begin(), indexlist.end(), [&](const Mesh* a, const Mesh* b) {
		return a->merged_index_pointer < b->merged_index_pointer;
		});
	for (int i = 0; i < 3; i++) {
		std::sort(vertexlist[i].begin(), vertexlist[i].end(), [&](const Mesh* a, const Mesh* b) {
			return a->merged_vert_offset < b->merged_vert_offset;
			});
	}

	uint32_t first_free = 0;
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, global_index_buffer.handle);
	for (int i = 0; i < indexlist.size(); i++) {
		Mesh* m = indexlist[i];
		if (m->merged_index_pointer > first_free) {
			glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, first_free, m->data.indicies.size(), m->data.indicies.data());
		}
		m->merged_index_pointer = first_free;
		first_free += m->data.indicies.size();
	}
	global_index_buffer.used = first_free;

	for (int j = 0; j < 3; j++) {
		auto& vb = global_vertex_buffers[j];
		auto& vl = vertexlist[j];
		for (int a = 0; a < MAX_ATTRIBUTES; a++) {
			if (!(vertex_buffer_formats[j].mask & (1<<a)))
				continue;
			int a_size = vertex_attribute_formats[a].get_size();

			first_free = 0;
			glBindBuffer(GL_ARRAY_BUFFER, vb.attributes[a].handle);
			for (int i = 0; i < vl.size(); i++) {
				Mesh* m = vl[i];
				uint32_t vertex_pointer = m->merged_vert_offset * a_size;
				if (vertex_pointer > first_free && m->data.buffers[a].size() > 0) {
					glBufferSubData(GL_ARRAY_BUFFER, first_free, m->data.buffers[a].size(), m->data.buffers[a].data());
				}
				if (a == 0)
					fixups[j][i] = first_free / a_size;
				first_free += m->data.buffers[a].size();
			}
			vb.attributes[a].used = first_free;
		}

		for (int i = 0; i < vl.size(); i++)
			vl[i]->merged_vert_offset = fixups[j][i];
	}
	float end = GetTime();

	printf("compact geometry time: %f\n", end - start);
}

void Game_Mod_Manager::print_usage()
{
	int total_memory_usage = global_index_buffer.allocated;
	sys_print("Index buffer: %d/%d", global_index_buffer.used, global_index_buffer.allocated);
	for (int i = 0; i < NUM_FMT; i++) {
		sys_print("Vertex buffer %i\n", i);
		int vertex_count = global_vertex_buffers[i].attributes[0].used / 12;
		int vertex_allocated = global_vertex_buffers[i].attributes[0].allocated / 12;

		sys_print("-Vertex count %d/%d\n", vertex_count, vertex_allocated);

		for (int j = 0; j < MAX_ATTRIBUTES; j++) {
			total_memory_usage += global_vertex_buffers[i].attributes[j].allocated;
		}
	}
	sys_print("Total memory for ib+vbs: %d\n", total_memory_usage);
}


Model* Game_Mod_Manager::find_or_load(const char* filename)
{
	auto find = models.find(filename);
	if (find != models.end()) {
		return find->second;
	}
	string path(model_folder_path);
	path += filename;

	Model* model = new Model;
	model->name = filename;

	bool good = load_gltf_model3(path, model);
	if (!good) {
		delete model;
		return nullptr;
	}
	good = upload_mesh(&model->mesh);
	if (!good) {
		delete model;
		return nullptr;
	}

	models[filename] = model;
	return model;
}

void Game_Mod_Manager::free_prefab(Prefab_Model* deleteprefab)
{
	for (auto& prefab : prefabs) {
		if (deleteprefab == prefab.second) {
			std::string val = prefab.first;
			prefabs.erase(val);
			return;
			// todo: free the buffer memory
		}
	}
}

Prefab_Model* Game_Mod_Manager::find_or_load_prefab(const char* file, bool dont_append_path,prefab_callback callback, void* callback_data)
{
	auto find = prefabs.find(file);
	if (find != prefabs.end()) {
		return find->second;
	}
	string path;
	if (!dont_append_path)
		path += model_folder_path;
	path += file;

	Prefab_Model* model = new Prefab_Model;
	model->name = file;

	bool good = load_gltf_prefab(path, model, callback, callback_data);
	if (!good) {
		delete model;
		return nullptr;
	}
	for (int i = 0; i < model->meshes.size(); i++)
		upload_mesh(&model->meshes[i]);

	prefabs[file] = model;
	return model;
}

bool Game_Mod_Manager::append_to_buffer(Gpu_Buffer& buf,  char* input_data, uint32_t input_length)
{
	if (input_length + buf.used > buf.allocated) {
		printf("Index buffer overflow\n");
		ASSERT(0);
		return false;
	}
	glBindBuffer(buf.target, buf.handle);
	glBufferSubData(buf.target, buf.used, input_length, input_data);
	buf.used += input_length;
	return true;
}

void Game_Mod_Manager::init()
{
	global_index_buffer.allocated = default_index_buffer_size * index_attribute_format.get_size();
	global_index_buffer.target = GL_ELEMENT_ARRAY_BUFFER;
	glGenBuffers(1, &global_index_buffer.handle);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, global_index_buffer.handle);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, default_index_buffer_size * index_attribute_format.get_size(), NULL, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	for (int i = 0; i < NUM_FMT; i++) {
		vertex_buffer_formats[i].generate_buffers(global_vertex_buffers[i].attributes);
	}

	// create vaos
	for (int i = 0; i < NUM_FMT; i++) {
		glGenVertexArrays(1, &global_vertex_buffers[i].main_vao);
		glBindVertexArray(global_vertex_buffers[i].main_vao);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, global_index_buffer.handle);
		vertex_buffer_formats[i].set_all_contained_attributes(global_vertex_buffers[i].attributes);
		glBindVertexArray(0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}
}

// does the actual uploading to the gpu
bool Game_Mod_Manager::upload_mesh(Mesh* mesh)
{
	// determine what buffer to go to
	ASSERT(mesh->parts.size() > 0);
	int attributes = mesh->attributes;
	Formats format = FMT_STATIC;
	if (attributes & TO_MASK(ATTRIBUTE_JOINT))
		format = FMT_SKINNED;
	else if ((attributes & TO_MASK(ATTRIBUTE_UV2)) || (attributes & TO_MASK(ATTRIBUTE_COLOR)))
		format = FMT_STATIC_PLUS;
	mesh->format = format;
	mesh->merged_index_pointer = global_index_buffer.used;
	append_to_buffer(
		global_index_buffer, 
		mesh->data.indicies.data(), 
		mesh->data.indicies.size()
	);
	glCheckError();

	mesh->vao = global_vertex_buffers[format].main_vao;

	mesh->merged_vert_offset = global_vertex_buffers[format].attributes[0].used / vertex_attribute_formats[0].get_size();
	int num_verticies = mesh->data.buffers[0].size() / vertex_attribute_formats[0].get_size();
	for (int i = 0; i < MAX_ATTRIBUTES; i++) {
		if (vertex_buffer_formats[format].mask & (1 << i)) {
			// sanity check
			int this_attribute_verticies = mesh->data.buffers[i].size() / vertex_attribute_formats[i].get_size();
			// this can happen and is allowed, like a lightmap mesh not using vertex colors
			if (this_attribute_verticies == 0) {
				global_vertex_buffers[format].attributes[i].used += num_verticies * vertex_attribute_formats[i].get_size();
			}
			else if (this_attribute_verticies != num_verticies){
				//assert(0 && "vertex count mismatch");
				printf("vertex count mismatch\n");
				return false;
			}
			else {
				bool good = append_to_buffer(
					global_vertex_buffers[format].attributes[i],
					mesh->data.buffers[i].data(),
					mesh->data.buffers[i].size()
				);
			}
		}
	}
	glCheckError();
	return true;
}

