
#include "LightComponents.h"
#include "Render/Render_Light.h"
#include "Render/Render_Sun.h"
#include "Render/DrawPublic.h"
#include "Render/Texture.h"

#include "BillboardComponent.h"
#include "ArrowComponent.h"

#include "Game/Entity.h"
#include "Assets/AssetDatabase.h"

#include "Framework/ReflectionMacros.h"
#include "Framework/ReflectionProp.h"

#include "GameEnginePublic.h"

#include <filesystem>
////

glm::vec4 get_color_light_value(Color32 c, float intensity) {
	auto vec = color32_to_vec4(c);
	auto linear = colorvec_srgb_to_linear(vec);
	return linear * PI * intensity;
}

void SpotLightComponent::start()
{
	if (eng->is_editor_level())
	{
		auto billboard = get_owner()->create_component<BillboardComponent>();
		billboard->set_texture(default_asset_load<Texture>("eng/icon/_nearest/flashlight.png"));
		billboard->dont_serialize_or_edit = true;	// editor only item, dont serialize
		auto arrow_obj = get_owner()->create_child_entity();
		arrow_obj->dont_serialize_or_edit = true;
		auto arrow_comp = arrow_obj->create_component<ArrowComponent>();
		arrow_obj->set_ls_transform(glm::vec3(0,0,0.4), {}, glm::vec3(0.25f));
		editor_arrow = arrow_comp;
		editor_billboard = billboard;
	}

	sync_render_data();
}
void SpotLightComponent::on_sync_render_data()
{
	if (!light_handle.is_valid())
		light_handle = idraw->get_scene()->register_light();

	Render_Light light;
	light.color = get_color_light_value(color, intensity);// glm::vec3(color.r, color.g, color.b)* (intensity / 255.f)* PI;
	light.projected_texture = const_cast<Texture*>(cookie_asset);
	light.conemax = cone_angle;
	light.conemin = inner_cone;
	light.radius = radius;
	light.is_spotlight = true;
	light.casts_shadow_mode = (int8_t)shadow;
	light.casts_shadow_size = (int8_t)shadow_size;

	auto& transform = get_owner()->get_ws_transform();

	light.position = transform[3];
	light.normal = glm::normalize(-transform[2]);

	idraw->get_scene()->update_light(light_handle, light);
}



void SpotLightComponent::stop()
{
	idraw->get_scene()->remove_light(light_handle);
	if (auto b = editor_billboard.get()) {
		b->destroy();
	}
	if (auto arrow = editor_arrow.get()) {
		arrow->get_owner()->destroy_deferred();
	}
}



void PointLightComponent::on_sync_render_data()
{
	if (!light_handle.is_valid())
		light_handle = idraw->get_scene()->register_light();
	Render_Light light;
	light.color = get_color_light_value(color, intensity);// glm::vec3(color.r, color.g, color.b)* (intensity / 255.f)* PI;
	light.radius = radius;
	light.is_spotlight = false;
	auto& transform = get_owner()->get_ws_transform();
	light.position = transform[3];
	idraw->get_scene()->update_light(light_handle, light);
};

void PointLightComponent::start()
{
	if (eng->is_editor_level())
	{
		auto b = get_owner()->create_component<BillboardComponent>();
		b->set_texture(default_asset_load<Texture>("eng/icon/pointBig.png"));
		b->dont_serialize_or_edit = true;	// editor only item, dont serialize
		editor_billboard = b->get_instance_id();
	}
	sync_render_data();
}

void PointLightComponent::stop()
{
	idraw->get_scene()->remove_light(light_handle);
	auto e = eng->get_object(editor_billboard);
	if (e)
		((Component*)e)->destroy();
}

void SunLightComponent::on_sync_render_data()
{
	if (!light_handle.is_valid())
		light_handle = idraw->get_scene()->register_sun();
	Render_Sun light;
	light.color = get_color_light_value(color, intensity);// glm::vec3(color.r, color.g, color.b)* (intensity / 255.f)* PI;
	light.fit_to_scene = fit_to_scene;
	light.log_lin_lerp_factor = log_lin_lerp_factor;
	light.z_dist_scaling = z_dist_scaling;
	light.max_shadow_dist = max_shadow_dist;
	light.epsilon = epsilon;
	light.cast_shadows = true;
	auto& transform = get_owner()->get_ws_transform();
	light.direction = glm::normalize(-transform[2]);
	idraw->get_scene()->update_sun(light_handle, light);
}



void SunLightComponent::start()
{
	if (eng->is_editor_level())
	{
		auto b = get_owner()->create_component<BillboardComponent>();
		b->set_texture(default_asset_load<Texture>("eng/icon/_nearest/sun.png"));
		b->dont_serialize_or_edit = true;	// editor only item, dont serialize

		auto s = get_owner()->create_component<ArrowComponent>();
		s->dont_serialize_or_edit = true;
		editor_arrow = s->get_instance_id();
		editor_billboard = b->get_instance_id();
	}

	sync_render_data();
}

void SunLightComponent::stop()
{
	idraw->get_scene()->remove_sun(light_handle);

	auto e = eng->get_object(editor_billboard);
	if (e)
		((Component*)e)->destroy();
	e = eng->get_object(editor_arrow);
	if (e)
		((Component*)e)->destroy();
}


SunLightComponent::~SunLightComponent() {}
PointLightComponent::~PointLightComponent() {}
SpotLightComponent::~SpotLightComponent() {}
#include "Render/Render_Volumes.h"

void SkylightComponent::start() {

	mytexture = new Texture; // g_imgs.install_system_texture("_skylight");
	sync_render_data();

	if (eng->is_editor_level()) {
		auto b = get_owner()->create_component<BillboardComponent>();
		b->set_texture(default_asset_load<Texture>("eng/icon/_nearest/skylight.png"));
		b->dont_serialize_or_edit = true;	// editor only item, dont serialize
	}
}
void SkylightComponent::stop() {
	idraw->get_scene()->remove_skylight(handle);
	//delete mytexture;
	mytexture = nullptr;
}
void SkylightComponent::on_sync_render_data()
{
	bool wants_recapture = !handle.is_valid();
	wants_recapture |= recapture_skylight.check_and_swap();

	if (!handle.is_valid())
		handle = idraw->get_scene()->register_skylight();
	Render_Skylight sl;
	sl.generated_cube = mytexture;
	sl.wants_update = wants_recapture;
	sl.fog_enabled = enable_fog;
	sl.height_fog_density = fog_density;
	sl.height_fog_exp = fog_height_falloff;
	sl.height_fog_start = fog_height_start;
	sl.fog_color = fog_color;
	sl.fog_use_skylight_cubemap = use_sky_cubemap_for_fog;

	idraw->get_scene()->update_skylight(handle, sl);
}


#include "Game/Entity.h"


SpotLightComponent::SpotLightComponent() {
	set_call_init_in_editor(true);
}
PointLightComponent::PointLightComponent() {
	set_call_init_in_editor(true);
}

SunLightComponent::SunLightComponent() {
	set_call_init_in_editor(true);
}
#include "Game/Components/MeshbuilderComponent.h"
#include "Render/MaterialPublic.h"
#include "MeshComponent.h"
#include "Level.h"
void CubemapComponent::start() {
	mytexture = new Texture;

	if (eng->is_editor_level()) {
		editor_meshbuilder = get_owner()->create_component<MeshBuilderComponent>();
		editor_meshbuilder->dont_serialize_or_edit = true;
		editor_meshbuilder->use_background_color = true;
		editor_meshbuilder->use_transform = false;

		auto meshEntity = eng->get_level()->spawn_entity();
		auto mesh = meshEntity->create_component<MeshComponent>();
		mesh->set_model(Model::load("eng/LIGHT_SPHERE.cmdl"));
		mesh->set_material_override(MaterialInstance::load("top_down/gray_metal.mi"));
		meshEntity->dont_serialize_or_edit = true;
		this->editor_mesh = meshEntity;

		update_editormeshbuilder();
	}
	on_changed_transform();

	sync_render_data();
}
void CubemapComponent::on_sync_render_data()
{
	if (!handle.is_valid())
		handle = idraw->get_scene()->register_reflection_volume();
	Render_Reflection_Volume h;
	h.wants_update = true;
	h.generated_cube = mytexture;
	h.probe_position = get_probe_pos();
	glm::vec3 scale = get_owner()->get_ls_scale();
	h.boxmin = get_ws_position() - scale * 0.5f;
	h.boxmax = get_ws_position() + scale * 0.5f;
	h.probe_ofs = probe_ofs;

	idraw->get_scene()->update_reflection_volume(handle, h);
}

void CubemapComponent::update_editormeshbuilder()
{
	if (!editor_meshbuilder)
		return;
	glm::vec3 scale = get_owner()->get_ls_scale();
	auto boxmin = get_ws_position() - scale * 0.5f;
	auto boxmax = get_ws_position() + scale * 0.5f;
	editor_meshbuilder->mb.Begin();
	editor_meshbuilder->mb.PushLineBox(boxmin, boxmax, COLOR_GREEN);
	editor_meshbuilder->mb.End();
}
void CubemapComponent::stop() {
	idraw->get_scene()->remove_reflection_volume(handle);
	//delete mytexture;
	mytexture = nullptr;
	if (editor_meshbuilder) {
		editor_meshbuilder->destroy();
		editor_meshbuilder = nullptr;
	}
	if (editor_mesh.get()) {
		editor_mesh.get()->destroy();
	}
}

void CubemapComponent::on_changed_transform() {
	update_editormeshbuilder();
	if (editor_mesh.get()) {
		editor_mesh->set_ws_position(get_ws_position());
	}
}





#include "Framework/Files.h"
void LightmapComponent::start()
{
	sync_render_data();
}

void LightmapComponent::stop()
{
	if (handle.is_valid())
		idraw->get_scene()->remove_lightmap(handle);
}
extern void export_godot_scene(const std::string& base_export_path);
#include <fstream>
#include "Framework/StringUtils.h"
#include "Level.h"
#include "MeshComponent.h"
#include "Framework/MapUtil.h"
// hack fixme stuff etc

void LightmapComponent::on_sync_render_data()
{
	if (!handle.is_valid())
		handle = idraw->get_scene()->register_lightmap();
	Lightmap_Object obj;
	obj.lightmap_texture = lightmapTexture;
	obj.staticAmbientCubeProbes = bakedProbes;	//fixme copy
	idraw->get_scene()->update_lightmap(handle, obj);
}

// this is the path to the data directory used by godot when you export/import lightmap bakes
// like: "C:/Users/charl/Documents/lightmapexporter/"
ConfigVar godot_lightmap_engine_path("godot_lightmap_engine_path", "", 0, "");

#ifdef EDITOR_BUILD

void CubemapComponent::editor_on_change_property() {
	if (recapture.check_and_swap()) {
		sync_render_data();
	}
	update_editormeshbuilder();
}
void LightmapComponent::editor_on_change_property()
{
	if (bakeLightmaps.check_and_swap()) {
		do_export();
	}
	if (importBaked.check_and_swap()) {
		do_import();
	}
}
void SkylightComponent::editor_on_change_property() {
	sync_render_data();
}


void LightmapComponent::do_export()
{
	string expPath = godot_lightmap_engine_path.get_string();// "C:/Users/charl/Documents/lightmapexporter/";
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
			glm::vec3 pos = owner->get_ws_transform()* glm::vec4(modelCenter, 1.0);

			outfile << "probe " << pos.x << " " << pos.y << " " << pos.z << '\n';
			lmProbeToObj.insert({ probe_index,as_mesh->unique_file_id });
			probe_index += 1;
		}
		else if (auto as_cube = o->cast_to<CubemapComponent>()) {
			if (as_cube->dont_serialize_or_edit)
				continue;
			if (as_cube->get_owner()->dont_serialize_or_edit)
				continue;
			auto pos = as_cube->get_probe_pos();
			outfile << "probe " << pos.x << " " << pos.y << " " << pos.z << '\n';
			lmProbeToObj.insert({ probe_index,as_cube->unique_file_id });
			probe_index += 1;
		}
	}
}

glm::vec3 evaluate_sh9(const glm::vec3 dir, const glm::vec3 sh[9]) {
	float x = dir.x, y = dir.y, z = dir.z;

	float sh_basis[9];
	sh_basis[0] = 0.282095f;                       // L00
	sh_basis[1] = 0.488603f * y;                   // L1-1
	sh_basis[2] = 0.488603f * z;                   // L10
	sh_basis[3] = 0.488603f * x;                   // L11
	sh_basis[4] = 1.092548f * x * y;               // L2-2
	sh_basis[5] = 1.092548f * y * z;               // L2-1
	sh_basis[6] = 0.315392f * (3.0f * z * z - 1.0f); // L20
	sh_basis[7] = 1.092548f * x * z;               // L21
	sh_basis[8] = 0.546274f * (x * x - y * y);     // L22

	glm::vec3 result(0.0f);
	for (int i = 0; i < 9; ++i)
		result += sh[i] * sh_basis[i];

	return result;  // RGB value in that direction
}
ConfigVar lightmapShDebug("lightmapShDebug", "0", CVAR_BOOL, "");
ConfigVar lightmapShTweakSh("lightmapShTweakSh", "0.5", CVAR_FLOAT, "");
void LightmapComponent::do_import()
{
	namespace fs = std::filesystem;
	string sourcePath = string(godot_lightmap_engine_path.get_string()) + "lightmap_root_scene.exr";
	string gamePath = FileSys::get_game_path();
	fs::path source = sourcePath;
	fs::path target_dir = gamePath;

	// get the filename to use:
	string filename = eng->get_level()->get_source_asset_name();
	StringUtils::remove_extension(filename);
	filename += "_lightmap_baked.exr";
	fs::path target = gamePath+"/"+filename;

	//fs::create_directories(target_dir);
	//fs::path target = target_dir / source.filename();

	fs::copy_file(source, target, fs::copy_options::overwrite_existing);

	lightmapTexture = Texture::load(filename);
	if (lightmapTexture) {
		g_assets.reload_sync<Texture>(lightmapTexture);
	}
	else {
		sys_print(Warning, "LightmapComponent::do_import: failed to texture load?\n");
	}

	//OUT_LIGHTMAP_BAKED.txt
	string bakedPath = string(godot_lightmap_engine_path.get_string()) +  "OUT_LIGHTMAP_BAKED.txt";


	std::unordered_map<int, Component*> found;
	auto& all_objs = eng->get_level()->get_all_objects();
	for (auto o : all_objs) {
		if (auto as_mesh = o->cast_to<MeshComponent>()) {
			as_mesh->set_not_lightmapped();
			if (as_mesh->unique_file_id != 0)
				MapUtil::insert_test_exists(found, as_mesh->unique_file_id, (Component*)as_mesh);
		}
		else if (auto as_box = o->cast_to<CubemapComponent>()) {
			as_box->set_baked_probe_ofs(-1);
			if (as_box->unique_file_id != 0)
				MapUtil::insert_test_exists(found, as_box->unique_file_id, (Component*)as_box);
		}
	}
	bakedProbes.clear();
	std::ifstream infile(bakedPath);
	std::string str;
	int cur_probe_index = 0;
	while (std::getline(infile, str)) {
		if (str.empty()) continue;
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
				LightmapCoords coords{ x,y,xofs,yofs };
				as_mesh->set_lightmapped(coords);
			}
		}
		else if (split.at(0) == "probe") {
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
				{ -1,  0,  0},  // +X
				{1,  0,  0},  // -X
				{ 0,  -1,  0},  // +Y
				{ 0, +1,  0},  // -Y
				{ 0,  0,  -1},  // +Z
				{ 0,  0, +1},  // -Z
			};
			glm::vec3 ambient_cube[6];
			for (int i = 0; i < 6; ++i) {
				if (!lightmapShDebug.get_bool()) {
					// looks too dark whatev
					ambient_cube[i] = evaluate_sh9(face_dirs[i], colors) + colors[0] * lightmapShTweakSh.get_float(); // bruh
				}
				else {
					ambient_cube[i] = colors[0];
				}
			}

			int uid = MapUtil::get_or(lmProbeToObj, cur_probe_index, -1);
			if (uid != -1) {
				auto find = MapUtil::get_or(found, uid, (Component*)nullptr);
				if (!find) {
					sys_print(Warning, "LightmapComponent: couldnt find probe lit obj with id %d\n", uid);
				}
				else {
					if (auto as_mesh = find->cast_to<MeshComponent>()) {
						as_mesh->set_static_probe_lit(cur_probe_index);
					}
					else if(auto as_cube = find->cast_to<CubemapComponent>()) {
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
#include "Framework/Serializer.h"


LightmapComponent::~LightmapComponent() {

}

void LightmapComponent::serialize(Serializer& s)
{
	Component::serialize(s);

	const char* const baked_tag = "lmProbeBin";

	if (s.is_loading()) {
		std::string outData;
		if (s.serialize(baked_tag, outData)) {
			auto data = StringUtils::base64_decode(outData);
			if ((data.size() % sizeof(glm::vec3)) != 0) {
				sys_print(Error, "LightmapComponent::serialize: unserialized bin data bad size?\n");
			}
			else {
				const int vec3Count = data.size() / sizeof(glm::vec3);
				bakedProbes.clear();
				bakedProbes.resize(vec3Count);
				for (int i = 0; i < vec3Count; i++) {
					bakedProbes.at(i) = *(glm::vec3*)(&data.at(i * sizeof(glm::vec3)));
				}
			}
		}
	}
	else {
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
