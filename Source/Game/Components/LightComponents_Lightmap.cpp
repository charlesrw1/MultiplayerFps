// LightmapComponent: bake export/import, serialize, and runtime scene registration.

#include "LightComponents.h"
#include "Render/DrawPublic.h"
#include "Render/Texture.h"
#include "Render/Model.h"
#include "Render/RenderObj.h"

#include "MeshComponent.h"

#include "Game/Entity.h"
#include "Assets/AssetDatabase.h"

#include "Framework/ReflectionMacros.h"
#include "Framework/ReflectionProp.h"
#include "Framework/Files.h"
#include "Framework/Serializer.h"
#include "Framework/StringUtils.h"
#include "Framework/MapUtil.h"

#include "GameEnginePublic.h"
#include "Level.h"

#include <fstream>
#include <filesystem>

////

extern void export_godot_scene(const std::string& base_export_path);

////
// LightmapComponent – lifecycle
////

void LightmapComponent::start() {
	ASSERT(get_owner() != nullptr);
	sync_render_data();
}

void LightmapComponent::stop() {
	ASSERT(idraw != nullptr || !handle.is_valid());
	if (handle.is_valid())
		idraw->get_scene()->remove_lightmap(handle);
}

void LightmapComponent::on_sync_render_data() {
	ASSERT(idraw != nullptr);
	if (!handle.is_valid())
		handle = idraw->get_scene()->register_lightmap();
	Lightmap_Object obj;
	obj.lightmap_texture = lightmapTexture;
	obj.staticAmbientCubeProbes = bakedProbes; // fixme copy
	idraw->get_scene()->update_lightmap(handle, obj);
}

LightmapComponent::~LightmapComponent() {}

////
// LightmapComponent – serialization
////

void LightmapComponent::serialize(Serializer& s) {
	ASSERT(true); // handles both loading and saving paths
	Component::serialize(s);

	const char* const baked_tag = "lmProbeBin";

	if (s.is_loading()) {
		std::string outData;
		if (s.serialize(baked_tag, outData)) {
			auto data = StringUtils::base64_decode(outData);
			if ((data.size() % sizeof(glm::vec3)) != 0) {
				sys_print(Error, "LightmapComponent::serialize: unserialized bin data bad size?\n");
			} else {
				const int vec3Count = data.size() / sizeof(glm::vec3);
				bakedProbes.clear();
				bakedProbes.resize(vec3Count);
				for (int i = 0; i < vec3Count; i++) {
					bakedProbes.at(i) = *(glm::vec3*)(&data.at(i * sizeof(glm::vec3)));
				}
			}
		}
	} else {
		std::vector<uint8_t> outData;
		outData.resize(bakedProbes.size() * sizeof(glm::vec3));
		for (int i = 0; i < bakedProbes.size(); i++) {
			glm::vec3* outPtr = (glm::vec3*)&outData.at(i * sizeof(glm::vec3));
			*outPtr = bakedProbes.at(i);
		}
		std::string encoded = StringUtils::base64_encode(outData);
		s.serialize(baked_tag, encoded);
	}
}

////
// LightmapComponent – editor bake helpers
////

// this is the path to the data directory used by godot when you export/import lightmap bakes
// like: "C:/Users/charl/Documents/lightmapexporter/"
ConfigVar godot_lightmap_engine_path("godot_lightmap_engine_path", "", 0, "");

static glm::vec3 evaluate_sh9(const glm::vec3 dir, const glm::vec3 sh[9]) {
	ASSERT(glm::length(dir) > 0.001f);
	float x = dir.x, y = dir.y, z = dir.z;

	float sh_basis[9];
	sh_basis[0] = 0.282095f;						 // L00
	sh_basis[1] = 0.488603f * y;					 // L1-1
	sh_basis[2] = 0.488603f * z;					 // L10
	sh_basis[3] = 0.488603f * x;					 // L11
	sh_basis[4] = 1.092548f * x * y;				 // L2-2
	sh_basis[5] = 1.092548f * y * z;				 // L2-1
	sh_basis[6] = 0.315392f * (3.0f * z * z - 1.0f); // L20
	sh_basis[7] = 1.092548f * x * z;				 // L21
	sh_basis[8] = 0.546274f * (x * x - y * y);		 // L22

	glm::vec3 result(0.0f);
	for (int i = 0; i < 9; ++i)
		result += sh[i] * sh_basis[i];

	return result; // RGB value in that direction
}

ConfigVar lightmapShDebug("lightmapShDebug", "0", CVAR_BOOL, "");
ConfigVar lightmapShTweakSh("lightmapShTweakSh", "0.5", CVAR_FLOAT, "");

#ifdef EDITOR_BUILD

void LightmapComponent::editor_on_change_property() {
	ASSERT(true); // BoolButton state drives which action to take
	if (bakeLightmaps.check_and_swap()) {
		do_export();
	}
	if (importBaked.check_and_swap()) {
		do_import();
	}
}

void LightmapComponent::do_export() {
	ASSERT(eng->get_level() != nullptr);
	string expPath = godot_lightmap_engine_path.get_string(); // "C:/Users/charl/Documents/lightmapexporter/";
	export_godot_scene(expPath);

	string probeInputPath = expPath + "LM_PROBE_POSITIONS_INPUT.txt";
	std::ofstream outfile(probeInputPath);
	if (!outfile) {
		sys_print(Error, "LightmapComponent::do_export: couldnt create probe input file\n");
		return;
	}
	lmProbeToObj.clear();
	auto all_objs = eng->get_level()->get_all_objects();
	int probe_index = 0;
	for (auto o : all_objs) {
		if (auto as_mesh = o->cast_to<MeshComponent>()) {
			if (as_mesh->dont_serialize_or_edit)
				continue;
			if (as_mesh->get_owner()->dont_serialize_or_edit)
				continue;
			if (!as_mesh->get_model())
				continue;
			if (as_mesh->get_model()->has_lightmap_coords())
				continue;
			auto owner = as_mesh->get_owner();

			glm::vec3 modelCenter = as_mesh->get_model()->get_bounds().get_center();
			glm::vec3 pos = owner->get_ws_transform() * glm::vec4(modelCenter, 1.0);

			outfile << "probe " << pos.x << " " << pos.y << " " << pos.z << '\n';
			lmProbeToObj.insert({probe_index, (int)as_mesh->get_instance_id()});
			probe_index += 1;
		} else if (auto as_cube = o->cast_to<CubemapComponent>()) {
			if (as_cube->dont_serialize_or_edit)
				continue;
			if (as_cube->get_owner()->dont_serialize_or_edit)
				continue;
			auto pos = as_cube->get_probe_pos();
			outfile << "probe " << pos.x << " " << pos.y << " " << pos.z << '\n';
			lmProbeToObj.insert({probe_index, as_cube->get_instance_id()});
			probe_index += 1;
		}
	}
}

void LightmapComponent::do_import() {
	ASSERT(eng->get_level() != nullptr);
	namespace fs = std::filesystem;
	string sourcePath = string(godot_lightmap_engine_path.get_string()) + "lightmap_root_scene.exr";
	string gamePath = FileSys::get_game_path();
	fs::path source = sourcePath;
	fs::path target_dir = gamePath;

	// get the filename to use:
	string filename = eng->get_level()->get_source_asset_name();
	StringUtils::remove_extension(filename);
	filename += "_lightmap_baked.exr";
	fs::path target = gamePath + "/" + filename;

	// fs::create_directories(target_dir);
	// fs::path target = target_dir / source.filename();

	fs::copy_file(source, target, fs::copy_options::overwrite_existing);

	lightmapTexture = Texture::load(filename);
	if (lightmapTexture) {
		g_assets.reload<Texture>(lightmapTexture);
	} else {
		sys_print(Warning, "LightmapComponent::do_import: failed to texture load?\n");
	}

	// OUT_LIGHTMAP_BAKED.txt
	string bakedPath = string(godot_lightmap_engine_path.get_string()) + "OUT_LIGHTMAP_BAKED.txt";

	std::unordered_map<int, Component*> found;
	auto& all_objs = eng->get_level()->get_all_objects();
	for (auto o : all_objs) {
		if (auto as_mesh = o->cast_to<MeshComponent>()) {
			as_mesh->set_not_lightmapped();
			if (as_mesh->get_instance_id() != 0)
				MapUtil::insert_test_exists(found, (int)as_mesh->get_instance_id(), (Component*)as_mesh);
		} else if (auto as_box = o->cast_to<CubemapComponent>()) {
			as_box->set_baked_probe_ofs(-1);
			if (as_box->get_instance_id() != 0)
				MapUtil::insert_test_exists(found, (int)as_box->get_instance_id(), (Component*)as_box);
		}
	}
	bakedProbes.clear();
	std::ifstream infile(bakedPath);
	std::string str;
	int cur_probe_index = 0;
	while (std::getline(infile, str)) {
		if (str.empty())
			continue;
		auto split = StringUtils::split(str);
		if (split.empty()) {
			sys_print(Warning, "LightmapComponent: didnt get 5 args in line\n");
			continue;
		}
		if (split.at(0) == "mesh") {
			int uid = std::stoi(split.at(1));
			float x = std::stof(split.at(2));
			float y = std::stof(split.at(3));
			float xofs = std::stof(split.at(4));
			float yofs = std::stof(split.at(5));

			auto find = MapUtil::get_or(found, uid, (Component*)nullptr);
			if (!find) {
				sys_print(Warning, "LightmapComponent: couldnt find obj with id %d\n", uid);
				continue;
			}
			if (auto as_mesh = find->cast_to<MeshComponent>()) {
				LightmapCoords coords{x, y, xofs, yofs};
				as_mesh->set_lightmapped(coords);
			}
		} else if (split.at(0) == "probe") {
			glm::vec3 colors[9];
			for (int i = 0; i < 9; i++) {
				for (int j = 0; j < 3; j++) {
					int index = i * 3 + j;
					float val = std::stof(split.at(index + 1));
					colors[i][j] = val;
				}
			}

			float r = colors[0].r;
			float g = colors[0].g;
			float b = colors[0].b;
			sys_print(Debug, "LightmapComponent: probe %f %f %f\n", r, g, b);

			const glm::vec3 face_dirs[6] = {
				{-1, 0, 0}, // +X
				{1, 0, 0},	// -X
				{0, -1, 0}, // +Y
				{0, +1, 0}, // -Y
				{0, 0, -1}, // +Z
				{0, 0, +1}, // -Z
			};
			glm::vec3 ambient_cube[6];
			for (int i = 0; i < 6; ++i) {
				if (!lightmapShDebug.get_bool()) {
					// looks too dark whatev
					ambient_cube[i] =
						evaluate_sh9(face_dirs[i], colors) + colors[0] * lightmapShTweakSh.get_float(); // bruh
				} else {
					ambient_cube[i] = colors[0];
				}
			}

			int uid = MapUtil::get_or(lmProbeToObj, cur_probe_index, -1);
			if (uid != -1) {
				auto find = MapUtil::get_or(found, uid, (Component*)nullptr);
				if (!find) {
					sys_print(Warning, "LightmapComponent: couldnt find probe lit obj with id %d\n", uid);
				} else {
					if (auto as_mesh = find->cast_to<MeshComponent>()) {
						as_mesh->set_static_probe_lit(cur_probe_index);
					} else if (auto as_cube = find->cast_to<CubemapComponent>()) {
						as_cube->set_baked_probe_ofs(cur_probe_index);
					}
				}
			}
			for (int i = 0; i < 6; i++) {
				bakedProbes.push_back(ambient_cube[i]);
			}
			cur_probe_index += 1;
		}
	}
	sync_render_data();
}
#endif
