// GameSceneGiUtil: scene-level GI baking, cubemap management, and DDGI disk I/O.
// Also contains low-level texture export helpers (HDR, DDS).

#include "LightComponents.h"
#include "Render/RenderGiManager.h"
#include "Render/DrawPublic.h"

#include "Game/Entity.h"
#include "Assets/AssetDatabase.h"

#include "Framework/ReflectionMacros.h"
#include "Framework/ReflectionProp.h"
#include "Framework/Files.h"
#include "Framework/StringUtils.h"
#include "Framework/BinaryReadWrite.h"
#include <algorithm>

#include "GameEnginePublic.h"
#include "Level.h"

#include "Render/IGraphicsDevice.h"

////
// Forward declarations for render-side helpers defined elsewhere
////

bool load_dds_file_specialized_format(IGraphicsTexture*& out_ptr, uint8_t* buffer, int len);
bool SaveCubeArrayToDDS(IGraphicsTexture* texture, uint32_t width, uint32_t height, uint32_t mipLevels, uint32_t cubeCount,
						const char* filename);
bool save_float_texture_as_dds(IGraphicsTexture* texture, uint32_t width, uint32_t height, int mode, const char* filename);
int write_hdr_wrapper(const char* filename, int w, int h, int comp, const float* data);

////
// File-suffix constants shared by save/load
////

static const string cubemap_suffix = "_cubemaps.dds";
static const string irrad_suffix   = "_ddgi_irrad.dds";
static const string depth_suffix   = "_ddgi_depth.dds";
static const string baked_gi_suffix = "_baked.gi";
static const int    gi_saved_version = 1;

////
// GameSceneGiUtil
////

bool GameSceneGiUtil::had_changes = false;

void GameSceneGiUtil::on_scene_load_gi(const string& mapname) {
	string name = mapname;
	StringUtils::remove_extension(name);
	auto baked_file = FileSys::open_read_game((name + baked_gi_suffix).c_str());
	if (!baked_file) {
		sys_print(Warning, "scene has no baked gi\n");
		return;
	}
	BinaryReader reader(baked_file.get());
	int version = reader.read_int32();
	if (version != gi_saved_version) {
		sys_print(Warning, "baked gi version out of date\n");
		return;
	}
	int num_cm_vols = reader.read_int32();
	std::vector<R_CubemapVolume> cm_vols;
	for (int i = 0; i < num_cm_vols; i++) {
		R_CubemapVolume dest;
		reader.read_struct(&dest);
		cm_vols.push_back(dest);
	}
	int num_ddgi_vols = reader.read_int32();
	std::vector<DdgiVolumeGpu> ddgi_vols;
	for (int i = 0; i < num_ddgi_vols; i++) {
		DdgiVolumeGpu dest;
		reader.read_struct(&dest);
		ddgi_vols.push_back(dest);
	}
	int probe_count = reader.read_int32();
	std::vector<glm::vec4> relocate;
	relocate.resize(probe_count);
	reader.read_bytes_ptr(relocate.data(), probe_count * sizeof(glm::vec4));

	auto read_tex = [&](string suffix) -> IGraphicsTexture* {
		auto file = FileSys::open_read_game((name + suffix).c_str());
		if (!file)
			return nullptr;
		std::vector<std::byte> buf;
		buf.resize(file->size());
		file->read(buf.data(), buf.size());
		IGraphicsTexture* tex{};
		load_dds_file_specialized_format(tex, (uint8_t*)buf.data(), buf.size());
		return tex;
	};
	IGraphicsTexture* irrad    = read_tex(irrad_suffix);
	IGraphicsTexture* depth    = read_tex(depth_suffix);
	IGraphicsTexture* cubemaps = read_tex(cubemap_suffix);

	auto release = [](IGraphicsTexture* t) {
		if (t)
			t->release();
	};
	if (!irrad || !depth || !cubemaps) {
		sys_print(Warning, "couldnt load baked texture(s)\n");
		release(irrad);
		release(depth);
		release(cubemaps);
		return;
	}

	BakedDdgiInputData input_ddgi;
	input_ddgi.depths  = depth;
	input_ddgi.irrad   = irrad;
	input_ddgi.offsets = std::move(relocate);
	input_ddgi.volumes = std::move(ddgi_vols);
	RenderGiManager::inst->set_loaded_ddgi_data(std::move(input_ddgi));
	RenderGiManager::inst->set_cubemaps_from_loading(std::move(cm_vols), cubemaps);
}

void GameSceneGiUtil::on_scene_exit() {
	ASSERT(RenderGiManager::inst != nullptr);
	RenderGiManager::inst->update_cubemap_volumes({}); // clear all
}

void GameSceneGiUtil::bake_all_cubemaps() {
	ASSERT(RenderGiManager::inst != nullptr);
	std::vector<R_CubemapVolume> volumes;

	std::vector<CubemapComponent*> components;
	auto& objs = eng->get_level()->get_all_objects();
	for (auto o : objs) {
		if (auto cc = o->cast_to<CubemapComponent>()) {
			components.push_back(cc);
		}
	}
	for (int i = 0; i < components.size(); i++) {
		components[i]->probe_ofs = i;

		glm::quat rot = components[i]->get_owner()->get_ws_rotation();
		glm::vec3 pos = components[i]->get_owner()->get_ws_position();

		glm::mat4 transform = glm::translate(glm::mat4(1), pos) * glm::mat4_cast(rot);
		glm::vec3 size = components[i]->get_owner()->get_ls_scale();
		R_CubemapVolume vol{};
		vol.extents   = glm::vec4(size, components[i]->priority);
		vol.transform = transform;

		const auto& full_transform = components[i]->get_ws_transform();
		glm::vec3 min1 = full_transform * glm::vec4(-0.5, -0.5, -0.5, 1);
		glm::vec3 max1 = full_transform * glm::vec4(0.5, 0.5, 0.5, 1);
		glm::vec3 min = glm::min(min1, max1);
		glm::vec3 max = glm::max(min1, max1);
		min -= vec3(1);
		max += vec3(1);
		vol.bounds_min = glm::vec4(min, 0);
		vol.bounds_max = glm::vec4(max, 0);

		volumes.push_back(vol);
	}
	std::sort(volumes.begin(), volumes.end(),
			  [](const R_CubemapVolume& a, const R_CubemapVolume& b) { return a.extents.w > b.extents.w; });

	RenderGiManager::inst->update_cubemap_volumes(volumes);
	RenderGiManager::inst->bake_all_cubemaps();
}

void GameSceneGiUtil::bake_one_cubemap(CubemapComponent* volume) {
	ASSERT(volume != nullptr);
	if (volume->probe_ofs != -1)
		RenderGiManager::inst->bake_one_cubemap(volume->probe_ofs);
}

void GameSceneGiUtil::on_cubemap_changes() {
	had_changes = true;
}
void GameSceneGiUtil::check_changes() {
	ASSERT(true); // had_changes is a static flag
	if (!had_changes)
		return;
	had_changes = false;
}

void GameSceneGiUtil::save_to_disk() {
	ASSERT(RenderGiManager::inst != nullptr);
	// save specular cubemaps
	// save spec volume info
	// save diffuse volumes
	// save probe offsets
	// save 2 ddgi textures

	string name = eng->get_level()->get_source_asset_name();
	StringUtils::remove_extension(name);

	FileWriter writer;
	const auto& volumes = RenderGiManager::inst->get_cubemap_volumes_vector();
	writer.write_int32(gi_saved_version);
	writer.write_int32(volumes.size());
	writer.write_bytes_ptr((uint8_t*)volumes.data(), volumes.size() * sizeof(R_CubemapVolume));
	const auto& ddgivols = draw.ddgi->myvolumes;
	writer.write_int32(ddgivols.size());
	writer.write_bytes_ptr((uint8_t*)ddgivols.data(), ddgivols.size() * sizeof(DdgiVolumeGpu));
	auto& relocate = draw.ddgi->temp_probe_relocate_thing;
	writer.write_int32(relocate.size());
	writer.write_bytes_ptr((uint8_t*)relocate.data(), relocate.size() * sizeof(glm::vec4));
	IFilePtr out = FileSys::open_write_game(name + baked_gi_suffix);
	out->write(writer.get_buffer(), writer.get_size());

	auto cubemap_tex = RenderGiManager::inst->get_cubemap_array_texture();
	const string dir = FileSys::get_game_path() + string("/") + name;
	SaveCubeArrayToDDS(cubemap_tex, CUBEMAP_WIDTH, CUBEMAP_WIDTH,
					   Texture::get_mip_map_count(CUBEMAP_WIDTH, CUBEMAP_WIDTH), volumes.size(),
					   (dir + cubemap_suffix).c_str()

	);

	if (draw.ddgi->probe_irradiance) {
		auto t = draw.ddgi->probe_irradiance;
		save_float_texture_as_dds(t, t->get_size().x, t->get_size().y, 1,
								  (dir + irrad_suffix).c_str());
		t = draw.ddgi->probe_depth;
		save_float_texture_as_dds(t, t->get_size().x, t->get_size().y, 0,
								  (dir + depth_suffix).c_str());
	} else {
		sys_print(Warning, "no probe irrad/depth to save\n");
	}
}

void GameSceneGiUtil::bake_ddgi() {}
