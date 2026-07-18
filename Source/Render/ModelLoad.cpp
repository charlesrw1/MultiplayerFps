// ModelLoad.cpp — Model asset loading, binary deserialisation, uninstall
// Split from Model.cpp to keep file sizes under 600 LOC.

#include "Model.h"
#include "ModelManager.h"
#include "Animation/AnimSidecarFile.h"

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
#include "Level.h"
#include "Game/Entity.h"
#include "Game/EntityComponent.h"
#include "Game/BaseUpdater.h"
#include "GameEnginePublic.h"
#include "AssetCompile/ModelCompilierLocal.h"

static const int MODEL_FORMAT_VERSION = 19;

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
	ASSERT(!name.is_null());
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
	// Keep the unique_ptr<MSkeleton> alive across reload so anyone caching
	// model->get_skel() keeps a valid address. Clear its contents.
	if (skel) {
		skel->uninstall();
	}
	collision.reset();
	tags.clear();
	materials.clear();

	// DONT reset the UID
	// dont do this 'uid = 0'

	g_modelMgr.remove_model_from_list(this);
}

void Model::post_load() {
	if (did_load_fail()) {
		return;
	}
	//	ASSERT(uid == 0);
	g_modelMgr.upload_model(this);

	const bool is_reload = first_post_load_done;
	first_post_load_done = true;

	if (is_reload) {
		// Hot-reload of an already-loaded model.  Walk the live scene and tell every
		// component that may cache anything derived from this model's skeleton/data
		// (animator clip ptrs, bone-index caches, retarget maps) to invalidate.
		// Done here — not via a delegate — so components do not need lifecycle hooks.
		Level* lvl = eng ? eng->get_level() : nullptr;
		if (lvl) {
			for (BaseUpdater* bu : lvl->get_all_objects()) {
				if (bu && bu->get_type().is_a(Component::StaticType)) {
					static_cast<Component*>(bu)->refresh_after_model_reload(this);
				}
			}
		}
	}

	AnimSidecarFile::apply_to_model(this);
	Model::on_model_loaded.invoke(this);
}

MulticastDelegate<Model*> Model::on_model_loaded;

// Format defined in ModelCompilier.cpp
bool Model::load_internal() {
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
	cull_distance = read.read_float();
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
		// Reuse the existing MSkeleton instance (its contents were cleared by
		// Model::uninstall on reload) so model->get_skel() keeps its address.
		if (!skel)
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

			aseq->fps = (aseq->num_frames > 0 && aseq->duration > 0.f)
				? (float)aseq->num_frames / aseq->duration
				: 30.f;

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

bool Model::load_asset() {
	const auto& path = get_name();

#ifdef EDITOR_BUILD
	if (developer_mode.get_bool()) {
		std::string model_def = strip_extension(path.c_str());
		model_def += ".mis";

		ModelCompilier::Ret ret = ModelCompilier::compile(model_def.c_str());
		if (ret == ModelCompilier::CompileErr) {
			sys_print(Error, "compilier failed on model %s\n", model_def.c_str());
		} else if (ret == ModelCompilier::CompileGood) {
			Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, string_format("model_info %s", path.c_str()));
		}
	}
#endif

	bool good = load_internal();
	if (good)
		return true;
	return false;
}

