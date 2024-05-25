#include "Model.h"
#include "Memory.h"
#include <vector>
#include <map>
#include "glad/glad.h"
#include "glm/gtc/type_ptr.hpp"

#include "Framework/Util.h"
#include "Animation/Runtime/Animation.h"
#include "Animation/AnimationTreePublic.h"
#include "Texture.h"


#include "Memory.h"




static const char* const model_folder_path = "./Data/Models/";




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

// not exactly small...
struct SmallVertex
{
	float pos[3];	// 12 bytes
	float uv[2];	// 20 bytes
	int16_t normal[3];	// 26 bytes
	int16_t tangent[3];	// 32 bytes
};
struct BigVertex
{
	float pos[3];
	float uv[2];
	int16_t normal[3];
	int16_t tangent[3];
	// bone index or color
	uint8_t color[4];	// 36 bytes
	// bone weight or 2 uint16 lightmap uv
	uint8_t color2[4];	// 40 bytes
};

Format_Descriptor index_attribute_format = Format_Descriptor(CT_U32, 1, false);
bool use_32_bit_indicies = true;

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
Vertex_Descriptor vertex_buffer_formats[(int)mesh_format::COUNT] =
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


#include "Framework/Config.h"
DECLARE_ENGINE_CMD_CAT("gpu.", print_vertex_usage)
{
	mods.print_usage();
}

#include <algorithm>

void Game_Mod_Manager::compact_memory()
{
	return;
	float start = GetTime();

	vector<Mesh*> indexlist;
	vector<Mesh*> vertexlist[3];
	vector<uint32_t> fixups[3];
	for (auto& model : models) {
		indexlist.push_back({ &model.second->mesh });
		vertexlist[(int)model.second->mesh.format].push_back(&model.second->mesh);
	}
	for (auto& prefab : prefabs)
		for (int i = 0; i < prefab.second->meshes.size(); i++) {
			indexlist.push_back({ &prefab.second->meshes[i] });
			vertexlist[prefab.second->meshes[i].format_as_int()].push_back(&prefab.second->meshes[i]);
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
	for (int i = 0; i < (int)mesh_format::COUNT; i++) {
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


static std::string get_extension(const std::string& name)
{
	auto find = name.rfind('.');
	if (find == std::string::npos)
		return {};
	return name.substr(find);
}

static std::string strip_extension(const std::string& name)
{
	auto find = name.rfind('.');
	if (find == std::string::npos)
		return {};
	return name.substr(0,find);
}


#include "Framework/DictParser.h"
bool ModelMan::parse_model_into_memory(Model* m, std::string path)
{
	std::string binpath = path + ".c_msh";

	DictParser in;
	bool good = in.load_from_file(binpath.c_str());
	if (!good) {

	}
}

DECLVAR("developer_mode", developer_mode, 1);
#include "AssetCompile/ModelCompile.h"
Model* ModelMan::find_or_load(const char* filename)
{
	auto find = models.find(filename);
	if (find != models.end()) {
		if (!find->second)
			return error_model;
		return find->second;
	}


	if (developer_mode.integer()) {

		bool good = ModelCompilier::compile(filename);
		if (!good) {
			sys_print("compilier failed on model %s\n", filename);
			return error_model;
		}
	}

	Model* model = new Model;
	model->name = filename;
	model->loaded_in_memory = false;

	string path(model_folder_path);
	path += filename;
	bool good = parse_model_into_memory(model,std::move(path));

	if (!good) {
		delete model;
		return error_model;
	}

	good = upload_model(&model->mesh);

	if (!good) {
		delete model;
		return error_model;
	}

	model->loaded_in_memory = true;
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

	for (int i = 0; i < (int)mesh_format::COUNT; i++) {
		vertex_buffer_formats[i].generate_buffers(global_vertex_buffers[i].attributes);
	}

	// create vaos
	for (int i = 0; i < (int)mesh_format::COUNT; i++) {
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
	mesh->id = cur_mesh_id++;

	// determine what buffer to go to
	ASSERT(mesh->parts.size() > 0);
	int attributes = mesh->attributes;
	mesh_format format = mesh_format::STATIC;
	if (attributes & TO_MASK(ATTRIBUTE_JOINT))
		format = mesh_format::SKINNED;
	else if ((attributes & TO_MASK(ATTRIBUTE_UV2)) || (attributes & TO_MASK(ATTRIBUTE_COLOR)))
		format = mesh_format::STATIC_PLUS;
	mesh->format = format;
	mesh->merged_index_pointer = global_index_buffer.used;
	append_to_buffer(
		global_index_buffer, 
		mesh->data.indicies.data(), 
		mesh->data.indicies.size()
	);
	glCheckError();

	const int format_int = mesh->format_as_int();

	mesh->vao = global_vertex_buffers[format_int].main_vao;

	mesh->merged_vert_offset = global_vertex_buffers[format_int].attributes[0].used / vertex_attribute_formats[0].get_size();
	int num_verticies = mesh->data.buffers[0].size() / vertex_attribute_formats[0].get_size();
	for (int i = 0; i < MAX_ATTRIBUTES; i++) {
		if (vertex_buffer_formats[format_int].mask & (1 << i)) {
			// sanity check
			int this_attribute_verticies = mesh->data.buffers[i].size() / vertex_attribute_formats[i].get_size();
			// this can happen and is allowed, like a lightmap mesh not using vertex colors
			if (this_attribute_verticies == 0) {
				global_vertex_buffers[format_int].attributes[i].used += num_verticies * vertex_attribute_formats[i].get_size();
			}
			else if (this_attribute_verticies != num_verticies){
				//assert(0 && "vertex count mismatch");
				printf("vertex count mismatch\n");
				return false;
			}
			else {
				bool good = append_to_buffer(
					global_vertex_buffers[format_int].attributes[i],
					mesh->data.buffers[i].data(),
					mesh->data.buffers[i].size()
				);
			}
		}
	}
	glCheckError();
	return true;
}

