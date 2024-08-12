#include "Model.h"
#include "Memory.h"
#include <vector>
#include <map>
#include "glad/glad.h"
#include "glm/gtc/type_ptr.hpp"

#include "Framework/Util.h"
#include "Animation/Runtime/Animation.h"
#include "Animation/AnimationTreePublic.h"
#include "Framework/DictParser.h"
#include "Texture.h"

#include "AssetCompile/Compiliers.h"
#include "Framework/Files.h"
#include "Framework/BinaryReadWrite.h"
#include "Framework/ObjectSerialization.h"
#include "Memory.h"

#include "Framework/Config.h"
#include <algorithm>

#include "Animation/SkeletonData.h"

#include "Physics/Physics2.h"

#include "Render/MaterialPublic.h"

ModelMan mods;

static const char* const model_folder_path = "./Data/Models/";

#include <unordered_set>
#include "AssetCompile/Someutils.h"// string stuff
#include "Assets/AssetRegistry.h"
#include "Assets/AssetLoaderRegistry.h"
extern IEditorTool* g_model_editor;	// defined in AssetCompile/ModelAssetEditorLocal.h

CLASS_IMPL(Model);
REGISTERASSETLOADER_MACRO(Model, &mods);

class ModelAssetMetadata : public AssetMetadata
{
public:
	// Inherited via AssetMetadata
	virtual Color32 get_browser_color() const  override
	{
		return { 20, 125, 245 };
	}

	virtual std::string get_type_name() const  override
	{
		return "Model";
	}

	virtual void index_assets(std::vector<std::string>& filepaths) const  override
	{
		std::unordered_set<std::string> stuff;
		auto find_tree = FileSys::find_files("./Data/Models");
		for (const auto _file : find_tree) {
			auto file = _file.substr(14);
			if (has_extension(file, "cmdl")) {
				auto resource_exists = stuff.find(file) != stuff.end();
				if (!resource_exists) {
					stuff.insert(file);
					filepaths.push_back(file);
				}
			}
			// if a .cmdl hasn't been compilied yet, still include .defs as .cmdls as they will be autocompilied
			else if (has_extension(file, "def")) {
				std::string path = strip_extension(file) + ".cmdl";
				auto resource_exists = stuff.find(path) != stuff.end();
				if (!resource_exists) {
					stuff.insert(path);
					filepaths.push_back(path);
				}
			}
		}
	}
	virtual IEditorTool* tool_to_edit_me() const override { return g_model_editor; }
	virtual std::string root_filepath() const  override
	{
		return model_folder_path;
	}
	virtual const ClassTypeInfo* get_asset_class_type() const { return &Model::StaticType; }
};

REGISTER_ASSETMETADATA_MACRO(ModelAssetMetadata);




Model::~Model() {}

static const int MODEL_FORMAT_VERSION = 7;

static const int STATIC_VERTEX_SIZE = 1'000'000;
static const int STATIC_INDEX_SIZE = 3'000'000;

// FIXME:
bool use_32_bit_indicies = false;

static const int INDEX_TYPE_SIZE = sizeof(uint16_t);


int ModelMan::get_index_type_size() const
{
	return INDEX_TYPE_SIZE;
}

void MainVbIbAllocator::init(uint32_t num_indicies, uint32_t num_verts)
{
	assert(ibuffer.handle == 0 && vbuffer.handle == 0);

	glGenBuffers(1, &vbuffer.handle);
	glBindBuffer(GL_ARRAY_BUFFER, vbuffer.handle);
	glBufferData(GL_ARRAY_BUFFER,
		sizeof(ModelVertex) * STATIC_VERTEX_SIZE /* size */,
		nullptr, GL_STATIC_DRAW);
	vbuffer.allocated = sizeof(ModelVertex) * STATIC_VERTEX_SIZE;
	vbuffer.used = 0;

	const int index_size = INDEX_TYPE_SIZE;
	glGenBuffers(1, &ibuffer.handle);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibuffer.handle);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER,
		index_size * STATIC_INDEX_SIZE /* size */,
		nullptr, GL_STATIC_DRAW);
	ibuffer.allocated = index_size * STATIC_INDEX_SIZE;
	ibuffer.used = 0;

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

#if 0
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
#endif
bool Model::has_lightmap_coords() const
{
	return false;
}

bool Model::has_bones() const
{
	return skel!=nullptr;
}

bool Model::has_colors() const
{
	return false;
}

bool Model::has_tangents() const
{
	return true;
}



int Model::bone_for_name(StringName name) const
{
	if (!get_skel())
		return -1;
	return get_skel()->get_bone_index(name);
}
DECLARE_ENGINE_CMD_CAT("gpu.", printusage)
{
	mods.print_usage();
}


#if 0
void ModelMan::compact_memory()
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
#endif

void MainVbIbAllocator::print_usage() const
{
	auto print_facts = [](const char* name, const buffer& b, int element_size) {
		float used_percentage = 1.0;
		if (b.allocated > 0)
			used_percentage = (double)b.used / (double)b.allocated;
		used_percentage *= 100.0;

		int used_elements = b.used / element_size;
		int allocated_elements = b.allocated / element_size;
		sys_print("%s: %d/%d (%.1f%%) (bytes:%d)\n", name, used_elements, allocated_elements, used_percentage, b.used);
	};
	sys_print("---- MainVbIbAllocator::print_usage ----\n");

	print_facts("Index buffer", ibuffer, INDEX_TYPE_SIZE);
	print_facts("Vertex buffer", vbuffer, sizeof(ModelVertex));
}

void MainVbIbAllocator::append_to_v_buffer(const uint8_t* data, size_t size) {
	append_buf_shared(data, size, "Vertex", vbuffer, GL_ARRAY_BUFFER);
}
void MainVbIbAllocator::append_to_i_buffer(const uint8_t* data, size_t size) {
	append_buf_shared(data, size, "Index", ibuffer, GL_ELEMENT_ARRAY_BUFFER);
}

void MainVbIbAllocator::append_buf_shared(const uint8_t* data, size_t size, const char* name, buffer& buf, uint32_t target)
{
	if (size + buf.used > buf.allocated) {
		sys_print("!!! %s buffer overflow %d/%d !!!\n",name, int(size + vbuffer.used), int(vbuffer.allocated));
		std::fflush(stdout);
		std::abort();
	}
	glBindBuffer(target, buf.handle);
	glBufferSubData(target, buf.used, size, data);
	buf.used += size;
}


void ModelMan::print_usage() const
{
	allocator.print_usage();
}


static glm::vec4 bounds_to_sphere(Bounds b)
{
	glm::vec3 center = b.get_center();
	glm::vec3 mindiff = center - b.bmin;
	glm::vec3 maxdiff = b.bmax - center;
	glm::vec3 diff = glm::max(mindiff, maxdiff);
	float radius = diff.x;
	if (diff.y > radius)radius = diff.y;
	if (diff.z > radius)radius = diff.z;
	return glm::vec4(center, radius);
}

// Format definied in ModelCompilier.cpp
bool ModelMan::read_model_into_memory(Model* m, std::string path)
{
	std::string binpath = path;

	auto file = FileSys::open_read(binpath.c_str());
	if (!file) {
		sys_print("!!! model %s does not exist\n", binpath.c_str());
		return false;
	}

	BinaryReader read(file.get());

	uint32_t magic = read.read_int32();
	if (magic != 'CMDL') {
		sys_print("bad model format\n");
		return false;
	}
	uint32_t version = read.read_int32();
	if (version != MODEL_FORMAT_VERSION) {
		sys_print("out of date format\n");
		return false;
	}
	read.read_struct(&m->skeleton_root_transform);

	read.read_struct(&m->aabb);
	m->bounding_sphere = bounds_to_sphere(m->aabb);

	uint32_t num_lods = read.read_int32();
	m->lods.reserve(num_lods);
	for (int i = 0; i < num_lods; i++) {
		MeshLod mlod;
		read.read_struct(&mlod);
		m->lods.push_back(mlod);
	}
	uint32_t num_parts = read.read_int32();
	m->parts.reserve(num_parts);
	for (int i = 0; i < num_parts; i++) {
		Submesh submesh;
		read.read_struct(&submesh);
		m->parts.push_back(submesh);
	}

	uint32_t DEBUG_MARKER = read.read_int32();
	assert(DEBUG_MARKER == 'HELP');

	uint32_t num_materials = read.read_int32();
	m->materials.reserve(num_materials);
	std::string buffer;
	for (int i = 0; i < num_materials; i++) {
		read.read_string(buffer);
		m->materials.push_back(imaterials->find_material_instance(buffer.c_str()));

		if (!m->materials.back()) {
			sys_print("!!! model doesn't have material %s\n", buffer.c_str());
			m->materials.back() = imaterials->get_fallback();
		}
	}


	uint32_t num_locators = read.read_int32();
	m->tags.reserve(num_locators);
	for (int i = 0; i < num_locators; i++) {
		ModelTag tag;
		read.read_string(tag.name);
		read.read_struct(&tag.transform);
		tag.bone_index = read.read_int32();
		m->tags.push_back(tag);
	}


	uint32_t num_indicies = read.read_int32();
	m->data.indicies.resize(num_indicies);
	read.read_bytes_ptr(
		m->data.indicies.data(), 
		num_indicies * sizeof(uint16_t)
	);

	uint32_t num_verticies = read.read_int32();
	m->data.verts.resize(num_verticies);
	read.read_bytes_ptr(
		m->data.verts.data(), 
		num_verticies * sizeof(ModelVertex)
	);

	DEBUG_MARKER = read.read_int32();
	assert(DEBUG_MARKER == 'HELP');

	// **PHYSICS DATA:
// bool has_physics (if false, skip this section)
// bool can_be_dynamic
// int num_shapes_main
// int num_bodies
// PSubBodyDef bodies[num_bodies]
// int num_shapes
// physics_shape_def shapes[num_shapes] (pointers are serialized as an offset to { int size, data[] })
// int num_constraints
// PhysicsBodyConstraintDef constrains[num_contraints]

	bool has_physics = read.read_byte();
	if (has_physics) {
		m->collision = std::make_unique<PhysicsBody>();
		auto& body = *m->collision.get();
		body.can_be_dynamic = read.read_byte();
		body.num_shapes_of_main = read.read_int32();
		body.subbodies.resize(read.read_int32());
		read.read_bytes_ptr(body.subbodies.data(), body.subbodies.size() * sizeof(PSubBodyDef));
		body.shapes.resize(read.read_int32());
		for (int i = 0; i < body.shapes.size(); i++) {
			read.read_bytes_ptr(&body.shapes[i], sizeof(physics_shape_def));
			g_physics->load_physics_into_shape(read, body.shapes[i]);
			DEBUG_MARKER = read.read_int32();
			assert(DEBUG_MARKER == 'HELP');
		}
		body.constraints.resize(read.read_int32());
		read.read_bytes_ptr(body.constraints.data(), body.constraints.size() * sizeof(PhysicsBodyConstraintDef));
	}


	DEBUG_MARKER = read.read_int32();
	assert(DEBUG_MARKER == 'HELP');

	uint32_t num_bones = read.read_int32();
	if (num_bones > 0) {

		m->skel = std::make_unique<MSkeleton>();
		m->skel->bone_dat.reserve(num_bones);
		for (int i = 0; i < num_bones; i++) {
			BoneData bd;
			read.read_string(bd.strname);
			bd.name = StringName(bd.strname.c_str());
			bd.parent = read.read_int32();
			bd.retarget_type = (RetargetBoneType)read.read_int32();
			read.read_struct(&bd.posematrix);
			read.read_struct(&bd.invposematrix);
			read.read_struct(&bd.localtransform);
			read.read_struct(&bd.rot);
			m->skel->bone_dat.push_back(bd);
		}

		uint32_t num_anims = read.read_int32();
		for (int i = 0; i < num_anims; i++) {

			uint32_t DEBUG_MARKER = read.read_int32();
			assert(DEBUG_MARKER == 'HELP');

			AnimationSeq* aseq = new AnimationSeq;
			std::string name;
			read.read_string(name);
			aseq->duration = read.read_float();
			aseq->average_linear_velocity = read.read_float();
			aseq->num_frames = read.read_int32();
			aseq->is_additive_clip = read.read_byte();
			
			aseq->channel_offsets.resize(num_bones);
			read.read_bytes_ptr(aseq->channel_offsets.data(), num_bones * sizeof(ChannelOffset));
			uint32_t packed_size = read.read_int32();
			aseq->pose_data.resize(packed_size);
			read.read_bytes_ptr(aseq->pose_data.data(), packed_size * sizeof(float));

			uint32_t num_events = read.read_int32();
			std::string buffer;
			for (int j = 0; j < num_events; j++) {
				read.read_string(buffer);
				DictParser parser;
				StringView tok;
				parser.read_string(tok);
				parser.load_from_memory((uint8_t*)buffer.c_str(), buffer.size(), "abc");
				AnimationEvent* event = read_object_properties<AnimationEvent>(
					nullptr, parser, tok
					);
				if (!event) {
					sys_print("??? couldn't load animation event '%s'\n", buffer.c_str());
				}
				else
					aseq->events.push_back(std::unique_ptr<AnimationEvent>(event));
			}
			MSkeleton::refed_clip rc;
			rc.ptr = aseq;
			rc.remap_idx = -1;
			rc.skeleton_owns_clip = true;
			m->skel->clips.insert({ std::move(name),rc });
		}

		uint32_t num_includes = read.read_int32();
		for (int i = 0; i < num_includes; i++) {
			std::string str;
			read.read_string(str);
		}

		bool has_mirror_map = read.read_byte();
		if (has_mirror_map) {
			m->skel->mirroring_table.resize(num_bones);
			read.read_bytes_ptr(m->skel->mirroring_table.data(), num_bones * sizeof(int16_t));
		}

		uint32_t num_masks = read.read_int32();
		m->skel->masks.resize(num_masks);
		for (int i = 0; i < num_masks; i++) {
			read.read_string(m->skel->masks[i].strname);
			m->skel->masks[i].idname = m->skel->masks[i].strname.c_str();
			m->skel->masks[i].weight.resize(num_bones);
			read.read_bytes_ptr(m->skel->masks[i].weight.data(), num_bones * sizeof(float));
		}

		 DEBUG_MARKER = read.read_int32();
		assert(DEBUG_MARKER == 'E');
	}

	// collision data goes here
	return true;
}

ConfigVar developer_mode("developer_mode", "1", CVAR_DEV | CVAR_BOOL);

void ModelMan::reload_this_model(Model* m)
{
	auto saved_id = m->uid;
	auto filenamestr = m->path;

	m->~Model();
	m = new(m)(Model);
	m->uid = saved_id;
	m->is_loaded = false;
	m->path = filenamestr;

	string path(model_folder_path);
	path += filenamestr;
	bool good = read_model_into_memory(m, std::move(path));

	if (!good) {
		sys_print("!!! reload_this_model failed\n");
		return;
	}

	good = upload_model(m);

	if (!good) {
		sys_print("!!! reload_this_model upload_model failed\n");
		return;
	}

	m->is_loaded = true;
}

Model* ModelMan::find_or_load(const char* filename)
{
	std::string filenamestr = filename;

	auto find = models.find(filenamestr);
	if (find != models.end()) {
		if (!find->second)
			return error_model;
		return find->second;
	}


	if (developer_mode.get_bool()) {
		std::string model_def = model_folder_path + strip_extension(filename);
		model_def += ".mis";

		bool good = ModelCompilier::compile(model_def.c_str());
		if (!good) {
			sys_print("compilier failed on model %s\n", model_def.c_str());
			return error_model;
		}
	}

	Model* model = new Model;
	model->path = filenamestr;
	model->is_loaded = false;

	string path(model_folder_path);
	path += filenamestr;
	bool good = read_model_into_memory(model,std::move(path));

	if (!good) {
		delete model;
		return error_model;
	}

	good = upload_model(model);

	if (!good) {
		delete model;
		return error_model;
	}

	model->is_loaded = true;
	models.insert({ std::move(filenamestr), model });

	return model;
}

#if 0
bool ModelMan::append_to_buffer(Gpu_Buffer& buf,  char* input_data, uint32_t input_length)
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
#endif

void ModelMan::set_v_attributes()
{
	assert(allocator.vbuffer.handle != 0);

	glBindBuffer(GL_ARRAY_BUFFER, allocator.vbuffer.handle);




	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void ModelMan::init()
{
	allocator.init(STATIC_INDEX_SIZE, STATIC_VERTEX_SIZE);

	glGenVertexArrays(1, &animated_vao);
	glBindVertexArray(animated_vao);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, allocator.ibuffer.handle);
	glBindBuffer(GL_ARRAY_BUFFER, allocator.vbuffer.handle);

	// POSITION
	glVertexAttribPointer(POSITION_LOC, 3, GL_FLOAT, GL_FALSE, sizeof(ModelVertex), (void*)offsetof(ModelVertex, pos));
	glEnableVertexAttribArray(POSITION_LOC);
	// UV
	glVertexAttribPointer(UV_LOC, 2, GL_FLOAT, GL_FALSE, sizeof(ModelVertex), (void*)offsetof(ModelVertex, uv));
	glEnableVertexAttribArray(UV_LOC);
	// NORMAL
	glVertexAttribPointer(NORMAL_LOC, 3, GL_SHORT, GL_TRUE, sizeof(ModelVertex), (void*)offsetof(ModelVertex, normal[0]));
	glEnableVertexAttribArray(NORMAL_LOC);
	// Tangent
	glVertexAttribPointer(TANGENT_LOC, 3, GL_SHORT, GL_TRUE, sizeof(ModelVertex), (void*)offsetof(ModelVertex, tangent[0]));
	glEnableVertexAttribArray(TANGENT_LOC);
	// Bone index
	glVertexAttribIPointer(JOINT_LOC, 4, GL_UNSIGNED_BYTE, sizeof(ModelVertex), (void*)offsetof(ModelVertex, color[0]));
	glEnableVertexAttribArray(JOINT_LOC);
	// Bone weight
	glVertexAttribPointer(WEIGHT_LOC, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(ModelVertex), (void*)offsetof(ModelVertex, color2[0]));
	glEnableVertexAttribArray(WEIGHT_LOC);

	glBindVertexArray(0);


	create_default_models();

	LIGHT_CONE = mods.find_or_load("LIGHT_CONE.cmdl");
	LIGHT_SPHERE = mods.find_or_load("LIGHT_SPHERE.cmdl");
	LIGHT_DOME = mods.find_or_load("LIGHT_DOME.cmdl");
	LIGHT_CONE->system_asset = true;
	LIGHT_SPHERE->system_asset = true;
	LIGHT_DOME->system_asset = true;

	if (!LIGHT_CONE || !LIGHT_SPHERE || !LIGHT_DOME)
		Fatalf("!!! ModelMan::init: couldn't load default LIGHT_x volumes (used for gbuffer lighting)\n");
}

void ModelMan::create_default_models()
{
	error_model = find_or_load("question.cmdl");
	if (!error_model)
		Fatalf("couldnt load error model (question.cmdl)\n");
	defaultPlane = find_or_load("plane.cmdl");
	if (!defaultPlane)
		Fatalf("couldnt load defaultPlane model\n");

	_sprite = new Model;
	_sprite->aabb = Bounds(glm::vec3(-0.5), glm::vec3(0.5));
	_sprite->bounding_sphere = bounds_to_sphere(_sprite->aabb);
	_sprite->materials.push_back(imaterials->get_fallback());
	{
		ModelVertex corners[4];
		corners[0].pos = glm::vec3(0.5, 0.5, 0.0);
		corners[0].uv = glm::vec2(1.0, 0.0);
		corners[0].normal[0] = 0;
		corners[0].normal[1] = 0;
		corners[0].normal[2] = INT16_MAX;

		corners[1].pos = glm::vec3(-0.5, 0.5, 0.0);
		corners[1].uv = glm::vec2(0.0, 0.0);
		corners[1].normal[0] = 0;
		corners[1].normal[1] = 0;
		corners[1].normal[2] = INT16_MAX;

		corners[2].pos = glm::vec3(-0.5, -0.5, 0.0);
		corners[2].uv = glm::vec2(0.0, 1.0);
		corners[2].normal[0] = 0;
		corners[2].normal[1] = 0;
		corners[2].normal[2] = INT16_MAX;

		corners[3].pos = glm::vec3(0.5, -0.5, 0.0);
		corners[3].uv = glm::vec2(1.0, 1.0);
		corners[3].normal[0] = 0;
		corners[3].normal[1] = 0;
		corners[3].normal[2] = INT16_MAX;

		_sprite->data.verts.push_back(corners[0]);
		_sprite->data.verts.push_back(corners[1]);
		_sprite->data.verts.push_back(corners[2]);
		_sprite->data.verts.push_back(corners[3]);
		_sprite->data.indicies.push_back(0);
		_sprite->data.indicies.push_back(1);
		_sprite->data.indicies.push_back(2);
		_sprite->data.indicies.push_back(0);
		_sprite->data.indicies.push_back(2);
		_sprite->data.indicies.push_back(3);

		Submesh sm;
		sm.base_vertex = 0;
		sm.element_count = 6;
		sm.element_offset = 0;
		sm.material_idx = 0;
		sm.vertex_count = 4;
		MeshLod lod;
		lod.end_percentage = 1.0;
		lod.part_count = 1;
		lod.part_ofs = 0;

		_sprite->parts.push_back(sm);
		_sprite->lods.push_back(lod);
		_sprite->is_loaded = true;

		_sprite->path = "_SPRITE";
		models["_SPRITE.cmdl"] = _sprite;

		upload_model(_sprite);
	}
}

// Uploads the models vertex and index data to the gpu
// and sets the models ptrs/offsets into the global vertex buffer
bool ModelMan::upload_model(Model* mesh)
{
	mesh->uid = cur_mesh_id++;

	if (mesh->parts.size() == 0)
		return false;

	mesh->merged_index_pointer = allocator.ibuffer.used;

	size_t indiciesbufsize{};
	const uint8_t* const ibufferdata = mesh->data.get_index_data(&indiciesbufsize);
	allocator.append_to_i_buffer(ibufferdata, indiciesbufsize);

	// vertex start offset
	mesh->merged_vert_offset = allocator.vbuffer.used / sizeof(ModelVertex);

	size_t vertbufsize{};
	const uint8_t* const v_bufferdata = mesh->data.get_vertex_data(&vertbufsize);
	allocator.append_to_v_buffer(v_bufferdata, vertbufsize);

	return true;
}

#include "AssetCompile/ModelAsset2.h"
#include <fstream>
DECLARE_ENGINE_CMD(IMPORT_MODEL)
{
	if (args.size() != 2) {
		sys_print("!!! usage: IMPORT_MODEL <.glb path>");
		return;
	}

	std::string savepath = "./Data/Models/";
	savepath += strip_extension(args.at(1)) + ".mis";
	{
		//auto existingFile = FileSys::open_read_os(savepath.c_str());
		//if (existingFile) {
		//	sys_print("??? IMPORT_MODE: import settings file already exists. Try editing it in the model editor instead or delete the file and try again\n");
		//	return;
		//}
	}


	ModelImportSettings mis;
	mis.srcGlbFile = args.at(1);


	DictWriter dw;
	write_object_properties(&mis, nullptr, dw);

	// save as text
	std::ofstream outfile(savepath);
	outfile.write(dw.get_output().data(), dw.get_output().size());
	outfile.close();

	std::string misF = strip_extension(args.at(1)) + ".mis";
	ModelCompilier::compile(savepath.c_str());
}