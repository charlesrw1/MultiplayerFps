#include "Model.h"
#include "ModelManager.h"

#include "Memory.h"
#include <vector>
#include <map>
#include "glad/glad.h"
#include "glm/gtc/type_ptr.hpp"

#include "Framework/Util.h"
#include "Animation/Runtime/Animation.h"

#include "Framework/DictParser.h"
#include "Texture.h"

#include "AssetCompile/Compiliers.h"
#include "Framework/Files.h"
#include "Framework/BinaryReadWrite.h"

#include "Memory.h"

#include "Framework/Config.h"
#include <algorithm>

#include "Animation/SkeletonData.h"

#include "Physics/Physics2.h"

#include "Render/MaterialPublic.h"



#include <unordered_set>
#include "AssetCompile/Someutils.h"// string stuff
#include "Assets/AssetRegistry.h"

#include "Assets/AssetDatabase.h"

ModelMan g_modelMgr;


#ifdef EDITOR_BUILD
//extern IEditorTool* g_model_editor;	// defined in AssetCompile/ModelAssetEditorLocal.h
class ModelAssetMetadata : public AssetMetadata
{
public:
	ModelAssetMetadata() {
		extensions.push_back("cmdl");
	}
	// Inherited via AssetMetadata
	virtual Color32 get_browser_color() const  override
	{
		return Color32(20, 125, 245);
	}

	virtual std::string get_type_name() const  override
	{
		return "Model";
	}

	//virtual IEditorTool* tool_to_edit_me() const override { return g_model_editor; }

	virtual const ClassTypeInfo* get_asset_class_type() const { return &Model::StaticType; }
};

REGISTER_ASSETMETADATA_MACRO(ModelAssetMetadata);
#endif

Model::~Model() {
	if (get_name() == "eng/cube.cmdl") {
		printf("");
	}
}
Model* Model::load(std::string path)
{
	return g_assets.find_sync<Model>(path).get();
}
Model::Model() {}

static const int MODEL_FORMAT_VERSION = 14;

static const int STATIC_VERTEX_SIZE = 1'000'000;
static const int STATIC_INDEX_SIZE = 3'000'000;

void MainVbIbAllocator::init(uint32_t num_indicies, uint32_t num_verts)
{
	assert(ibuffer.handle == 0 && vbuffer.handle == 0);

	glGenBuffers(1, &vbuffer.handle);
	glBindBuffer(GL_ARRAY_BUFFER, vbuffer.handle);
	glBufferData(GL_ARRAY_BUFFER,
		sizeof(ModelVertex) * STATIC_VERTEX_SIZE /* size */,
		nullptr, GL_STATIC_DRAW);
	vbuffer.allocated = sizeof(ModelVertex) * STATIC_VERTEX_SIZE;
	vbuffer.used_total = 0;
	vbuffer.tail = 0;
	vbuffer.head = 0;



	const int index_size = MODEL_BUFFER_INDEX_TYPE_SIZE;
	glGenBuffers(1, &ibuffer.handle);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibuffer.handle);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER,
		index_size * STATIC_INDEX_SIZE /* size */,
		nullptr, GL_STATIC_DRAW);
	ibuffer.allocated = index_size * STATIC_INDEX_SIZE;
	ibuffer.used_total = 0;
	ibuffer.tail = 0;
	ibuffer.head = 0;

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

bool Model::has_lightmap_coords() const
{
	return isLightmapped != Model::LightmapType::None;
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


void ModelMan::compact_memory()
{
	sys_print(Debug, "ModelMan::compact_memory\n");
	std::vector<Model*> models;
	models.reserve(all_models.num_used);
	for (auto m : all_models) {
		ASSERT(m->uid != 0);
		models.push_back(m);
	}

	// [.AA.B...CCC.D.....]
	// pick last item (D)
	// for item in list:
	//		move to end
	//		wrap ptr
	// [CCC.........DAAB..]
	
	// theres an edge case where no models are loaded, but that will never happen (default models)
	
	// do indices first
	std::sort(models.begin(), models.end(), [](Model* a, Model* b)->bool
		{
			return a->merged_index_pointer < b->merged_index_pointer;
		});
	allocator.ibuffer.used_total = models.back()->data.get_num_index_bytes();
	for (int i = 0; i < models.size() - 1/* skip last */; i++) {
		int index_ptr = models[i]->merged_index_pointer;
		allocator.ibuffer.tail = index_ptr;
		//models[i]->merged_index_pointer = allocator.move_append_i_buffer(index_ptr, models[i]->data.get_num_index_bytes());

		// okay just do it this way, i guess just keep models in CPU memory
		size_t indiciesbufsize{};
		const uint8_t* const ibufferdata = models[i]->data.get_index_data(&indiciesbufsize);
		models[i]->merged_index_pointer = allocator.append_to_i_buffer(ibufferdata, indiciesbufsize);	// dont divide by sizeof(uint16_2), this is an pointer

	}

	// then verts
	std::sort(models.begin(), models.end(), [](Model* a, Model* b)->bool
		{
			return a->merged_vert_offset < b->merged_vert_offset;
		});
	allocator.vbuffer.used_total = models.back()->data.get_num_vertex_bytes();

	for (int i = 0; i < models.size() - 1/* skip last */; i++) {
		int vertex_ptr = models[i]->merged_vert_offset * sizeof(ModelVertex);
		allocator.vbuffer.tail = vertex_ptr;
		//models[i]->merged_vert_offset = allocator.move_append_v_buffer(vertex_ptr, models[i]->data.get_num_vertex_bytes());
		//models[i]->merged_vert_offset /= sizeof(ModelVertex);

		size_t vertbufsize{};
		const uint8_t* const v_bufferdata = models[i]->data.get_vertex_data(&vertbufsize);
		models[i]->merged_vert_offset = allocator.append_to_v_buffer(v_bufferdata, vertbufsize);
		models[i]->merged_vert_offset /= sizeof(ModelVertex);
	}
}

#ifdef EDITOR_BUILD
#include "AssetCompile/ModelAsset2.h"
#include <fstream>
#include "Framework/ReflectionProp.h"
#include "Framework/DictWriter.h"
#include "Framework/SerializerJson.h"

void write_model_import_settings(ModelImportSettings* mis, const std::string& savepath) {

	MakePathForGenericObj pathmaker;
	WriteSerializerBackendJson writer("write_mis", pathmaker, *mis, true);

	auto fileptr = FileSys::open_write_game(savepath);
	if (fileptr) {
		sys_print(Info, "new_import_settings_to_modeldef_data: writing new MIS JSON version %s\n", savepath.c_str());

		string out = "!json\n" + writer.get_output().dump(1);
		fileptr->write(out.data(), out.size());
	}
	else {
		sys_print(Error, "new_import_settings_to_modeldef_dataL Couldnt open file to write out new version of mis %s\n", savepath.c_str());
	}
}
void IMPORT_MODEL_FUNC(const Cmd_Args& args)
{
	if (args.size() != 2) {
		sys_print(Error, "usage: IMPORT_MODEL <.glb path>");
		return;
	}

	auto savepath = strip_extension(args.at(1)) + ".mis";
	ModelImportSettings mis;
	mis.srcGlbFile = args.at(1);
	write_model_import_settings(&mis, savepath);
	
	ModelCompilier::compile(savepath.c_str(), AssetDatabase::loader);
}
void import_model_lightmapped(const Cmd_Args& args)
{
	if (args.size() != 4) {
		sys_print(Error, "usage: import_model_lightmapped <.glb path> <lm x> <lm y>");
		return;
	}
	try {
		auto savepath = strip_extension(args.at(1)) + ".mis";
		ModelImportSettings mis;
		mis.srcGlbFile = args.at(1);
		mis.withLightmap = true;
		mis.lightmapSizeX = std::stoi(args.at(2));
		mis.lightmapSizeY = std::stoi(args.at(3));
		write_model_import_settings(&mis, savepath);
		ModelCompilier::compile(savepath.c_str(), AssetDatabase::loader);
	}
	catch (...) {
		sys_print(Error, "usage: import_model_lightmapped err\n");
	}
}


#endif
#include "Framework/StringUtils.h"
void export_one_model(const Model& model, const char* export_path);

void ModelMan::add_commands(ConsoleCmdGroup& group)
{
	group.add("compact_vertex_buffer", [this](const Cmd_Args&) { compact_memory(); });
	group.add("print_vertex_usage", [this](const Cmd_Args&) { print_usage(); });

#ifdef EDITOR_BUILD
	group.add("IMPORT_MODEL", IMPORT_MODEL_FUNC);
	group.add("import_model_lightmapped", import_model_lightmapped);

	group.add("export_model", [](const Cmd_Args & args) {
		if (args.size() != 2) {
			sys_print(Error, "usage: export_model <model>\n");
			return;
		}
		auto mod = Model::load(args.at(1));
		if (!mod) {
			sys_print(Error, "model doesn't exist\n");
			return;
		}
		auto outPath = StringUtils::alphanumeric_hash(args.at(1))+".glb";
		sys_print(Info, "writing out to %s\n", outPath.c_str());
		export_one_model(*mod, outPath.c_str());
	});
	group.add("print_bones", [](const Cmd_Args& args) {
		if (args.size() != 2) {
			sys_print(Error, "usage: print_bones <model>\n");
			return;
		}
		auto mod = Model::load(args.at(1));
		if (!mod) {
			sys_print(Error, "model doesn't exist\n");
			return;
		}
		if (!mod->get_skel()) {
			sys_print(Error, "model doesn't have skeleton\n");
			return;
		}
		auto skel = mod->get_skel();
		auto& bones = skel->get_all_bones();
		sys_print(Info, "numBones=%d\n", int(bones.size()));
		for (int i = 0; i < bones.size(); i++) {
			sys_print(Info, "%s = %d (parent=%d)\n", bones[i].strname.c_str(), i, int(bones[i].parent));
		}

		});
	group.add("print_anims", [](const Cmd_Args& args) {
		if (args.size() != 2) {
			sys_print(Error, "usage: print_bones <model>\n");
			return;
		}
		auto mod = Model::load(args.at(1));
		if (!mod) {
			sys_print(Error, "model doesn't exist\n");
			return;
		}
		if (!mod->get_skel()) {
			sys_print(Error, "model doesn't have skeleton\n");
			return;
		}
		auto skel = mod->get_skel();
		auto& clips = skel->get_all_clips();
		sys_print(Info, "numClips=%d\n", int(clips.size()));
		for (auto&[name,seq] : clips) {
			sys_print(Info, "%s (duration=%f) (additive=%d)\n", name.c_str(),seq.ptr->duration, int(seq.ptr->is_additive_clip));
		}
		});
	group.add("model_info", [](const Cmd_Args& args) {
		if (args.size() != 2) {
			sys_print(Error, "usage: model_info <model>\n");
			return;
		}
		auto mod = Model::load(args.at(1));
		if (!mod) {
			sys_print(Error, "model doesn't exist\n");
			return;
		}
		sys_print(Info, "hasSkel = %d\n", int(mod->get_skel() != nullptr));
		if (mod->get_skel()) {
			sys_print(Info, "numBones = %d\n", int(mod->get_skel()->get_all_bones().size()));
			sys_print(Info, "numAnims = %d\n", int(mod->get_skel()->get_num_animations()));
			sys_print(Info, "hasMirroring = %d\n", int(mod->get_skel()->has_mirroring_table()));
		}
		sys_print(Info, "materials (%d) = [\n", mod->get_num_materials());
		for (int i = 0; i < mod->get_num_materials(); i++) {
			auto mat = mod->get_material(i);
			if (mat) {
				sys_print(Info, "\t%s\n",mat->get_name().c_str());
			}
			else {
				sys_print(Info, "\t<null>\n");
			}
		}
		sys_print(Info, "]\n");
		sys_print(Info, "isLightmapped = %d\n", int(mod->get_lightmap_type()));
		sys_print(Info, "lightmapSize = %d %d\n", mod->get_lightmap_size().x, mod->get_lightmap_size().y);
		sys_print(Info, "hasCollision = %d\n", int(mod->get_physics_body()!=nullptr));
		sys_print(Info, "numLods(%d) = [\n", mod->get_num_lods());
		for (int i = 0; i < mod->get_num_lods(); i++) {
			auto& lod = mod->get_lod(i);
			int totalV = 0;
			int totalI = 0;
			for (int p = 0; p < lod.part_count; p++) {
				totalV += mod->get_part(p + lod.part_ofs).vertex_count;
				totalI += mod->get_part(p + lod.part_ofs).element_count / MODEL_BUFFER_INDEX_TYPE_SIZE;
			}
			sys_print(Info, "\t[%d] verts=%d indicies=%d parts=%d fade=%f\n", i, totalV, totalI, lod.part_count, lod.end_percentage);
		}
		sys_print(Info, "]\n");
		});

#endif
}

void MainVbIbAllocator::print_usage() const
{
	auto print_facts = [](const char* name, const buffer& b, int element_size) {
		float used_percentage = 1.0;
		if (b.allocated > 0)
			used_percentage = (double)b.used_total / (double)b.allocated;
		used_percentage *= 100.0;

		int used_elements = b.used_total / element_size;
		int allocated_elements = b.allocated / element_size;
		sys_print(Info, "	%s: %d/%d (%.1f%%) (bytes:%d) (%d:%d)\n", name, used_elements, allocated_elements, used_percentage, b.used_total, b.tail, b.head);

	};
	sys_print(Info, "MainVbIbAllocator::print_usage\n");

	print_facts("IndexBuffer", ibuffer, MODEL_BUFFER_INDEX_TYPE_SIZE);
	print_facts("VertexBuffer", vbuffer, sizeof(ModelVertex));
}
void ModelMan::print_usage() const
{
	allocator.print_usage();
}


int MainVbIbAllocator::append_to_v_buffer(const uint8_t* data, size_t size) {
	return append_buf_shared(data, size, "Vertex", vbuffer, GL_ARRAY_BUFFER);
}
int MainVbIbAllocator::append_to_i_buffer(const uint8_t* data, size_t size) {
	return append_buf_shared(data, size, "Index", ibuffer, GL_ELEMENT_ARRAY_BUFFER);
}
int MainVbIbAllocator::move_append_v_buffer(int ofs, int size) {
	return move_append_buf_shared(ofs, size, "Vertex", vbuffer, GL_ARRAY_BUFFER);
}
int MainVbIbAllocator::move_append_i_buffer(int ofs, int size) {
	return move_append_buf_shared(ofs, size, "Index", ibuffer, GL_ELEMENT_ARRAY_BUFFER);
}

int MainVbIbAllocator::append_buf_shared(const uint8_t* data, size_t size, const char* name, buffer& buf, uint32_t target)
{
	auto out_of_memory = [&]() {
		sys_print(Error, "%s buffer overflow %d/%d !!!\n", name, int(size + buf.used_total), int(buf.allocated));
		std::fflush(stdout);
		std::abort();
	};

	int where_to_append = buf.head;
	if (buf.head >= buf.tail) {
		if (buf.head + size > buf.allocated) {
			if (size > buf.tail) {
				out_of_memory();
			}
			where_to_append = 0;
		}
	}
	else {
		if (buf.head + size > buf.tail) {
			out_of_memory();
		}
	}

	glBindBuffer(target, buf.handle);
	glBufferSubData(target, where_to_append, size, data);
	buf.used_total += size;
	buf.head = where_to_append + size;
	return where_to_append;
}
int MainVbIbAllocator::move_append_buf_shared(int ofs, int size, const char* name, buffer& buf, uint32_t target)
{
	auto out_of_memory = [&]() {
		sys_print(Error, "%s buffer overflow %d/%d !!!\n", name, int(size + buf.used_total), int(buf.allocated));
		std::fflush(stdout);
		std::abort();
	};
	int where_to_append = buf.head;
	if (buf.head >= buf.tail) {
		if (buf.head + size > buf.allocated) {
			if (size > buf.tail) {
				out_of_memory();
			}
			where_to_append = 0;
		}
	}
	else {
		if (buf.head + size > buf.tail) {
			out_of_memory();
		}
	}
	glBindBuffer(target, buf.handle);
	glCopyBufferSubData(target, target, ofs, where_to_append, size);
	buf.used_total += size;
	buf.head = where_to_append + size;
	return where_to_append;
}


static glm::vec4 bounds_to_sphere(Bounds b)
{
	glm::vec3 center = b.get_center();
	glm::vec3 mindiff = center - b.bmin;
	glm::vec3 maxdiff = b.bmax - center;
	float radius = glm::max(glm::length(mindiff), glm::length(maxdiff));
	return glm::vec4(center, radius);
}


extern ConfigVar developer_mode;

void Model::uninstall()
{
	lods.resize(0);
	parts.clear();
	merged_index_pointer = merged_vert_offset = 0;
	data = RawMeshData();	// so destructor gets called and memory is freed
	//skel.reset(nullptr);	// dont uninstall because of pointers...
	if (skel) {
		skel->uninstall();
	}
	collision.reset();
	tags.clear();
	//for (auto mat : materials) {
	//	mat->dec_ref_count_and_uninstall_if_zero();
	//}
	materials.clear();
	uid = 0;	// reset the UID

	g_modelMgr.remove_model_from_list(this);
}


void Model::post_load() {
	if (did_load_fail()) {
		return;
	}
	ASSERT(uid == 0);
	g_modelMgr.upload_model(this);
	Model::on_model_loaded.invoke(this);
}

MulticastDelegate<Model*> Model::on_model_loaded;


#include "AssetCompile/ModelCompilierLocal.h"




// Format definied in ModelCompilier.cpp
bool Model::load_internal(IAssetLoadingInterface* loading)
{
	auto file = FileSys::open_read_game(get_name().c_str());
	if (!file) {
		sys_print(Error, "model %s does not exist\n", get_name().c_str());
		return false;
	}

	BinaryReader read(file.get());

	uint32_t magic = read.read_int32();
	if (magic != 'CMDL') {
		sys_print(Error, "bad model format\n");
		return false;
	}
	uint32_t version = read.read_int32();
	if (version != MODEL_FORMAT_VERSION) {
		sys_print(Error, "out of date format\n");
		return false;
	}
	uint8_t isLightmappedByte = read.read_byte();
	assert(isLightmappedByte >= 0 && isLightmappedByte <= 2);
	isLightmapped = (Model::LightmapType)isLightmappedByte;
	lightmapX = (int16_t)read.read_int32();
	lightmapY = (int16_t)read.read_int32();


	read.read_struct(&skeleton_root_transform);

	read.read_struct(&aabb);
	bounding_sphere = bounds_to_sphere(aabb);

	int num_lods = read.read_int32();
	lods.reserve(num_lods);
	for (int i = 0; i < num_lods; i++) {
		MeshLod mlod;
		read.read_struct(&mlod);
		lods.push_back(mlod);
	}
	int num_parts = read.read_int32();
	parts.reserve(num_parts);
	for (int i = 0; i < num_parts; i++) {
		Submesh submesh;
		read.read_struct(&submesh);
		parts.push_back(submesh);
	}

	uint32_t DEBUG_MARKER = read.read_int32();
	assert(DEBUG_MARKER == 'HELP');

	int num_materials = read.read_int32();
	materials.resize(num_materials);
	std::string buffer;
	for (int i = 0; i < num_materials; i++) {
		read.read_string(buffer);

		//materials.push_back(imaterials->find_material_instance(buffer.c_str()));
		materials[i] = g_assets.find_sync_sptr<MaterialInstance>(buffer);// loading->load_asset(&MaterialInstance::StaticType, buffer);

		if (!materials[i]->is_valid_to_use()) {
			sys_print(Error, "model doesn't have material %s\n", buffer.c_str());
			materials.back() = imaterials->get_fallback_sptr();
		}
	}


	int num_locators = read.read_int32();
	tags.reserve(num_locators);
	for (int i = 0; i < num_locators; i++) {
		ModelTag tag;
		read.read_string(tag.name);
		read.read_struct(&tag.transform);
		tag.bone_index = read.read_int32();
		tags.push_back(tag);
	}


	int num_indicies = read.read_int32();
	data.indicies.resize(num_indicies);
	read.read_bytes_ptr(
		data.indicies.data(),
		num_indicies * MODEL_BUFFER_INDEX_TYPE_SIZE
	);

	int num_verticies = read.read_int32();
	data.verts.resize(num_verticies);
	read.read_bytes_ptr(
		data.verts.data(),
		num_verticies * sizeof(ModelVertex)
	);

	DEBUG_MARKER = read.read_int32();
	assert(DEBUG_MARKER == 'HELP');


	bool has_physics = read.read_byte();
	if (has_physics) {
		collision = std::make_unique<PhysicsBodyDefinition>();
		auto& body = *collision.get();
		body.shapes.resize(read.read_int32());
		for (int i = 0; i < body.shapes.size(); i++) {
			read.read_bytes_ptr(&body.shapes[i], sizeof(physics_shape_def));
			g_physics.load_physics_into_shape(read, body.shapes[i]);
			DEBUG_MARKER = read.read_int32();
			assert(DEBUG_MARKER == 'HELP');
		}
	}


	DEBUG_MARKER = read.read_int32();
	assert(DEBUG_MARKER == 'HELP');

	int num_bones = read.read_int32();
	if (num_bones > 0) {

		skel = std::make_unique<MSkeleton>();
		skel->bone_dat.reserve(num_bones);
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
			skel->bone_dat.push_back(bd);
		}

		int num_anims = read.read_int32();
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
			aseq->has_rootmotion = read.read_byte();

			aseq->channel_offsets.resize(num_bones);
			read.read_bytes_ptr(aseq->channel_offsets.data(), num_bones * sizeof(ChannelOffset));
			uint32_t packed_size = read.read_int32();
			aseq->pose_data.resize(packed_size);
			read.read_bytes_ptr(aseq->pose_data.data(), packed_size * sizeof(float));
	
			MSkeleton::refed_clip rc;
			rc.ptr = aseq;
			skel->clips.insert({ std::move(name),rc });
		}

		int num_includes = read.read_int32();
		for (int i = 0; i < num_includes; i++) {
			std::string str;
			read.read_string(str);
		}

		bool has_mirror_map = read.read_byte();
		if (has_mirror_map) {
			skel->mirroring_table.resize(num_bones);
			read.read_bytes_ptr(skel->mirroring_table.data(), num_bones * sizeof(int16_t));
		}

		DEBUG_MARKER = read.read_int32();
		assert(DEBUG_MARKER == 'E');
	}

	// collision data goes here
	return true;
}

bool Model::load_asset(IAssetLoadingInterface* loading) {
	const auto& path = get_name();

#ifdef EDITOR_BUILD
	if (developer_mode.get_bool()) {
		std::string model_def = strip_extension(path.c_str());
		model_def += ".mis";

		bool good = ModelCompilier::compile(model_def.c_str(),loading);
		if (!good) {
			sys_print(Error, "compilier failed on model %s\n", model_def.c_str());
		}
	}
#endif

	bool good = load_internal(loading);
	if (good)
		return true;
	return false;
}

void Model::move_construct(IAsset* _src)
{
	const bool had_skel = skel != nullptr;
	uninstall();
	assert(had_skel == (skel != nullptr));
	Model* src = (Model*)_src;
	ASSERT(this->uid == 0);
	assert(src);

	for (int i = 0; i < src->lods.size(); i++)
		this->lods.push_back(src->lods[i]);
	parts = std::move(src->parts);
	merged_index_pointer = src->merged_index_pointer;
	merged_vert_offset = src->merged_vert_offset;
	aabb = src->aabb;
	bounding_sphere = src->bounding_sphere;
	data = std::move(src->data);
	
	if (bool(skel) != bool(src->skel)) {
		throw std::runtime_error("Model::move_construct: cant reaload a model to a skeletal model or vice versa. restart the game.\n");
	}
	if (skel) {
		assert(src->skel);
		skel->move_construct(*src->skel);
	}
	if (collision) {
		collision->uninstall_shapes();
	}
	collision = std::move(src->collision);

	tags = std::move(src->tags);
	materials = std::move(src->materials);
	skeleton_root_transform = src->skeleton_root_transform;
	isLightmapped = src->isLightmapped;
	lightmapX = src->lightmapX;
	lightmapY = src->lightmapY;
	src->uninstall();
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

	{
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
	}
	{

		glGenVertexArrays(1, &lightmapped_vao);
		glBindVertexArray(lightmapped_vao);
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
		// LIGHTMAPCOORD_LOC
		glVertexAttribPointer(LIGHTMAPCOORD_LOC, 4, GL_UNSIGNED_SHORT, GL_TRUE, sizeof(ModelVertex), (void*)offsetof(ModelVertex, color[0]));
		glEnableVertexAttribArray(LIGHTMAPCOORD_LOC);
	
		glBindVertexArray(0);
	}


	create_default_models();
	auto& a = g_assets;
	LIGHT_CONE = a.find_global_sync<Model>("eng/LIGHT_CONE.cmdl").get();
	LIGHT_SPHERE = a.find_global_sync<Model>("eng/LIGHT_SPHERE.cmdl").get();
	LIGHT_DOME = a.find_global_sync<Model>("eng/LIGHT_DOME.cmdl").get();

	if (!LIGHT_CONE || !LIGHT_SPHERE || !LIGHT_DOME)
		Fatalf("!!! ModelMan::init: couldn't load default LIGHT_x volumes (used for gbuffer lighting)\n");
}

void ModelMan::create_default_models()
{
	error_model = g_assets.find_global_sync<Model>("eng/question.cmdl").get();
	if (!error_model)
		Fatalf("couldnt load error model (question.cmdl)\n");
	defaultPlane = g_assets.find_global_sync<Model>("eng/plane.cmdl").get();
	if (!defaultPlane)
		Fatalf("couldnt load defaultPlane model\n");

	_sprite = new Model;
	_sprite->aabb = Bounds(glm::vec3(-0.5), glm::vec3(0.5));
	_sprite->bounding_sphere = bounds_to_sphere(_sprite->aabb);
	_sprite->materials.push_back(imaterials->get_fallback_sptr());
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
		upload_model(_sprite);

		g_assets.install_system_asset(_sprite, "_SPRITE");
	}
}

// Uploads the models vertex and index data to the gpu
// and sets the models ptrs/offsets into the global vertex buffer
bool ModelMan::upload_model(Model* mesh)
{
	ASSERT(mesh);
	ASSERT(all_models.find(mesh) == nullptr);
	all_models.insert(mesh);


	mesh->uid = cur_mesh_id++;
	//sys_print(Debug, "uploading mode: %s\n", mesh->get_name().c_str());

	if (mesh->parts.size() == 0) {
		sys_print(Warning, "ModelMan::upload_model: model has not parts (%d)\n", mesh->get_name().c_str());
		return false;
	}


	size_t indiciesbufsize{};
	const uint8_t* const ibufferdata = mesh->data.get_index_data(&indiciesbufsize);
	mesh->merged_index_pointer = allocator.append_to_i_buffer(ibufferdata, indiciesbufsize);	// dont divide by sizeof(uint16_2), this is an pointer

	size_t vertbufsize{};
	const uint8_t* const v_bufferdata = mesh->data.get_vertex_data(&vertbufsize);
	mesh->merged_vert_offset = allocator.append_to_v_buffer(v_bufferdata, vertbufsize);
	mesh->merged_vert_offset /= sizeof(ModelVertex);

	return true;
}


void ModelMan::remove_model_from_list(Model* m)
{
	ASSERT(m);
	all_models.remove(m);
	ASSERT(!all_models.find(m));
}
void ModelMan::add_model_to_list(Model* m)
{
	ASSERT(m->uid != 0);
	ASSERT(all_models.find(m) == nullptr);
	all_models.insert(m);
}
ModelMan::ModelMan() : all_models(6)
{

}

