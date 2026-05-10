// ModelLoad.cpp — Model asset loading, binary deserialisation, move_construct, uninstall
// Split from Model.cpp to keep file sizes under 600 LOC.

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

#include "Framework/Config.h"
#include <algorithm>

#include "Animation/SkeletonData.h"

#include "Physics/Physics2.h"

#include "Render/MaterialPublic.h"

#include <unordered_set>
#include "AssetCompile/Someutils.h"
#include "Assets/AssetRegistry.h"
#include "Assets/AssetDatabase.h"
#include "AssetCompile/ModelCompilierLocal.h"

static const int MODEL_FORMAT_VERSION = 18;

extern ConfigVar developer_mode;

bool Model::has_lightmap_coords() const {
	ASSERT(true); // always callable
	return isLightmapped != Model::LightmapType::None;
}

bool Model::has_bones() const {
	ASSERT(true); // always callable
	return skel != nullptr;
}

int Model::bone_for_name(StringName name) const {
	ASSERT(name.is_valid());
	if (!get_skel())
		return -1;
	return get_skel()->get_bone_index(name);
}

glm::vec4 bounds_to_sphere(Bounds b) {
	ASSERT(true); // pure math, no preconditions
	glm::vec3 center = b.get_center();
	glm::vec3 mindiff = center - b.bmin;
	glm::vec3 maxdiff = b.bmax - center;
	float radius = glm::max(glm::length(mindiff), glm::length(maxdiff));
	return glm::vec4(center, radius);
}

PhysicsMaterialWrapper* Model::get_physics_material_to_use() const {
	ASSERT(true); // always callable on a valid Model
	if (physics_material)
		return physics_material;
	if (get_num_materials() > 0) {
		// can return null here
		return get_material(0)->get_physics_material();
	}
	return nullptr;
}

void Model::uninstall() {
	ASSERT(true); // can be called on any Model state
	lods.resize(0);
	parts.clear();

	data = RawMeshData(); // so destructor gets called and memory is freed
	// skel.reset(nullptr);	// dont uninstall because of pointers...
	if (skel) {
		skel->uninstall();
	}
	collision.reset();
	tags.clear();
	// for (auto mat : materials) {
	//	mat->dec_ref_count_and_uninstall_if_zero();
	//}
	materials.clear();

	// DONT reset the UID
	// dont do this 'uid = 0'

	g_modelMgr.remove_model_from_list(this);

	set_is_loaded(false);
}

void Model::post_load() {
	ASSERT(get_is_loaded());
	if (did_load_fail()) {
		return;
	}
	//	ASSERT(uid == 0);
	g_modelMgr.upload_model(this);
	Model::on_model_loaded.invoke(this);
}

MulticastDelegate<Model*> Model::on_model_loaded;

// Format defined in ModelCompilier.cpp
bool Model::load_internal(IAssetLoadingInterface* loading) {
	ASSERT(loading != nullptr || true); // loading may be null in some code paths
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

		// materials.push_back(imaterials->find_material_instance(buffer.c_str()));
		materials[i] = g_assets.find_sync_sptr<MaterialInstance>(
			buffer); // loading->load_asset(&MaterialInstance::StaticType, buffer);

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
	read.read_bytes_ptr(data.indicies.data(), num_indicies * MODEL_BUFFER_INDEX_TYPE_SIZE);

	int num_verticies = read.read_int32();
	data.verts.resize(num_verticies);
	read.read_bytes_ptr(data.verts.data(), num_verticies * sizeof(ModelVertex));

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
			skel->clips.insert({std::move(name), rc});
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
	ASSERT(loading != nullptr || true); // loading may be null in some code paths
	const auto& path = get_name();

#ifdef EDITOR_BUILD
	if (developer_mode.get_bool()) {
		std::string model_def = strip_extension(path.c_str());
		model_def += ".mis";

		ModelCompilier::Ret ret = ModelCompilier::compile(model_def.c_str(), loading);
		if (ret == ModelCompilier::CompileErr) {
			sys_print(Error, "compilier failed on model %s\n", model_def.c_str());
		} else if (ret == ModelCompilier::CompileGood) {
			Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, string_format("model_info %s", path.c_str()));
		}
	}
#endif

	bool good = load_internal(loading);
	if (good)
		return true;
	return false;
}

void Model::move_construct(IAsset* _src) {
	ASSERT(_src != nullptr);
	const bool had_skel = skel != nullptr;
	uninstall();

	assert(!get_is_loaded());

	assert(had_skel == (skel != nullptr));
	Model* src = (Model*)_src;
	// ASSERT(this->uid == 0);
	assert(src);

	for (int i = 0; i < src->lods.size(); i++)
		this->lods.push_back(src->lods[i]);
	parts = std::move(src->parts);
	index_alloc_ptr = src->index_alloc_ptr;
	vertex_alloc_ptr = src->vertex_alloc_ptr;
	aabb = src->aabb;
	bounding_sphere = src->bounding_sphere;
	data = std::move(src->data);

	if (bool(skel) != bool(src->skel)) {
		throw std::runtime_error(
			"Model::move_construct: cant reaload a model to a skeletal model or vice versa. restart the game.\n");
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

	set_is_loaded(true);
}
