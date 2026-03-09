
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
	light.projected_texture = (Texture*)cookie_asset;
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
	sl.fog_cubemap_min_dist = fog_cubemap_min_dist;
	sl.fog_cubemap_max_dist = fog_cubemap_max_dist;
	sl.fog_cubemap_max_mip = fog_cubemap_max_mip;


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

	if (eng->is_editor_level()) {
		editor_meshbuilder = get_owner()->create_component<MeshBuilderComponent>();
		editor_meshbuilder->dont_serialize_or_edit = true;
		editor_meshbuilder->use_background_color = true;
		editor_meshbuilder->use_transform = true;
		{
			auto mesh = get_owner()->create_component<MeshComponent>();
			mesh->set_ignore_baking(true);
			mesh->set_ignore_cubemap_view(true);
			mesh->dont_serialize_or_edit = true;
			mesh->set_model(Model::load("cube1m.cmdl"));
			mesh->set_material_override(MaterialInstance::load("cubemap_zone.mi"));
		}
		auto meshEntity = eng->get_level()->spawn_entity();
		auto mesh = meshEntity->create_component<MeshComponent>();
		mesh->set_ignore_baking(true);
		mesh->set_ignore_cubemap_view(true);
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

}

void CubemapComponent::update_editormeshbuilder()
{
	if (!editor_meshbuilder)
		return;
	glm::vec3 scale = get_owner()->get_ls_scale();
	auto boxmin = vec3(-0.5f);
	auto boxmax = vec3(0.5f);
	editor_meshbuilder->mb.Begin();
	editor_meshbuilder->mb.PushLineBox(boxmin, boxmax, COLOR_GREEN);
	editor_meshbuilder->mb.End();
}
void CubemapComponent::stop() {
	
	//delete mytexture;
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
			lmProbeToObj.insert({ probe_index,(int)as_mesh->get_instance_id() });
			probe_index += 1;
		}
		else if (auto as_cube = o->cast_to<CubemapComponent>()) {
			if (as_cube->dont_serialize_or_edit)
				continue;
			if (as_cube->get_owner()->dont_serialize_or_edit)
				continue;
			auto pos = as_cube->get_probe_pos();
			outfile << "probe " << pos.x << " " << pos.y << " " << pos.z << '\n';
			lmProbeToObj.insert({ probe_index,as_cube->get_instance_id() });
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
			if (as_mesh->get_instance_id() != 0)
				MapUtil::insert_test_exists(found, (int)as_mesh->get_instance_id(), (Component*)as_mesh);
		}
		else if (auto as_box = o->cast_to<CubemapComponent>()) {
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

void GiVolumeComponent::start()
{
	if (eng->is_editor_level()) {
		auto mesh = get_owner()->create_component<MeshComponent>();
		mesh->set_ignore_baking(true);
		mesh->set_ignore_cubemap_view(true);
		mesh->set_model(Model::load("cube1m.cmdl"));
		mesh->set_material_override(MaterialInstance::load("giprobe_zone.mi"));
		mesh->dont_serialize_or_edit = true;
	}
}

void GiVolumeComponent::stop()
{
}
Model* AreaishLightComponent::get_model_to_use()
{
	if (override_model) return override_model;
	return Model::load("eng/plane.cmdl");
}
void AreaishLightComponent::start()
{
	mat = imaterials->create_dynmaic_material(MaterialInstance::load("emissive_basic.mm"));
	auto mesh = get_owner()->create_component<MeshComponent>();
	mesh->set_model(get_model_to_use());
	mesh->set_material_override(mat.get());
	mat->set_floatvec_parameter("EmissiveColor", color32_to_vec4(color) * intensity);
}

void AreaishLightComponent::stop()
{
}

void AreaishLightComponent::editor_on_change_property()
{
	glm::vec4 linear = colorvec_srgb_to_linear(color32_to_vec4(color));
	mat->set_floatvec_parameter("EmissiveColor", linear * intensity);
	get_owner()->get_component<MeshComponent>()->set_model(get_model_to_use());
}
bool load_dds_file_specialized_format(IGraphicsTexture*& out_ptr, uint8_t* buffer, int len);


#include "Render/RenderGiManager.h"

static const string cubemap_suffix = "_cubemaps.dds";
static const string irrad_suffix = "_ddgi_irrad.dds";
static const string depth_suffix = "_ddgi_depth.dds";
static const string baked_gi_suffix = "_baked.gi";
static const int gi_saved_version = 1;

// version
// num cm volumes
// cm volumes[]
// num ddgi vols
// ddgi vols[]
// num probes/relocate
// relocate[]
#include "Framework/BinaryReadWrite.h"
void GameSceneGiUtil::on_scene_load_gi(const string& mapname)
{
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
		auto file = FileSys::open_read_game((name+suffix).c_str());
		if (!file) return nullptr;
		std::vector<std::byte> buf;
		buf.resize(file->size());
		file->read(buf.data(), buf.size());
		IGraphicsTexture* tex{};
		load_dds_file_specialized_format(tex, (uint8_t*)buf.data(), buf.size());
		return tex;
	};
	IGraphicsTexture* irrad = read_tex(irrad_suffix);
	IGraphicsTexture* depth = read_tex(depth_suffix);
	IGraphicsTexture* cubemaps = read_tex(cubemap_suffix);

	auto release = [](IGraphicsTexture* t) {
		if (t) t->release();
	};
	if (!irrad || !depth || !cubemaps) {
		sys_print(Warning, "couldnt load baked texture(s)\n");
		release(irrad);
		release(depth);
		release(cubemaps);
		return;
	}

	BakedDdgiInputData input_ddgi;
	input_ddgi.depths = depth;
	input_ddgi.irrad = irrad;
	input_ddgi.offsets = std::move(relocate);
	input_ddgi.volumes = std::move(ddgi_vols);
	RenderGiManager::inst->set_loaded_ddgi_data(std::move(input_ddgi));
	RenderGiManager::inst->set_cubemaps_from_loading(std::move(cm_vols), cubemaps);
}

void GameSceneGiUtil::on_scene_exit()
{
	RenderGiManager::inst->update_cubemap_volumes({});	// clear all
}

void GameSceneGiUtil::bake_all_cubemaps()
{
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
		vol.extents = glm::vec4(size,0);
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
	RenderGiManager::inst->update_cubemap_volumes(volumes);
	RenderGiManager::inst->bake_all_cubemaps();
}

void GameSceneGiUtil::bake_one_cubemap(CubemapComponent* volume)
{
	if (volume->probe_ofs != -1)
		RenderGiManager::inst->bake_one_cubemap(volume->probe_ofs);

}

bool GameSceneGiUtil::had_changes = false;
void GameSceneGiUtil::on_cubemap_changes()
{
	had_changes = true;
}
void GameSceneGiUtil::check_changes()
{
	if (!had_changes)
		return;
	had_changes = false;
}
#include "Framework/BinaryReadWrite.h"
void ExportCubemapArrayHDR(GLuint texture, int faceSize, int cubemapCount);
void export_float_texture(GLuint texture, int width, int height, string name);
bool SaveCubeArrayToDDS(GLuint texture,
	uint32_t width,
	uint32_t height,
	uint32_t mipLevels,
	uint32_t cubeCount,
	const char* filename);

bool save_float_texture_as_dds(GLuint texture,
	uint32_t width,
	uint32_t height,
	int mode,
	const char* filename);


void GameSceneGiUtil::save_to_disk()
{
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
	writer.write_bytes_ptr((uint8_t*)ddgivols.data(), ddgivols.size()*sizeof(DdgiVolumeGpu));
	auto& relocate = draw.ddgi->temp_probe_relocate_thing;
	writer.write_int32(relocate.size());
	writer.write_bytes_ptr((uint8_t*)relocate.data(), relocate.size()*sizeof(glm::vec4));
	IFilePtr out = FileSys::open_write_game(name + baked_gi_suffix);
	out->write(writer.get_buffer(), writer.get_size());

	auto cubemap_tex = RenderGiManager::inst->get_cubemap_array_texture();
		const string dir = FileSys::get_game_path()+string("/")+name;
	SaveCubeArrayToDDS(cubemap_tex->get_internal_handle(), CUBEMAP_WIDTH, CUBEMAP_WIDTH,
		Texture::get_mip_map_count(CUBEMAP_WIDTH, CUBEMAP_WIDTH), volumes.size(), 
		(dir+ cubemap_suffix).c_str()
	
	);
	
	if (draw.ddgi->probe_irradiance) {
		auto t = draw.ddgi->probe_irradiance;
		save_float_texture_as_dds(t->get_internal_handle(),
			t->get_size().x,
			t->get_size().y,
			1,
			(dir + irrad_suffix).c_str()
		);
		t = draw.ddgi->probe_depth;
		save_float_texture_as_dds(t->get_internal_handle(),
			t->get_size().x,
			t->get_size().y,
			0,
			(dir + depth_suffix).c_str()
		);
	}
	else {
		sys_print(Warning, "no probe irrad/depth to save\n");
	}

	//auto file = FileSys::open_read_engine("cubemaparray.dds");
	//std::vector<std::byte> buf;
	//buf.resize(file->size());
	//file->read(buf.data(), buf.size());
	//IGraphicsTexture* tex{};
	//load_dds_file_specialized_format(tex, (uint8_t*)buf.data(), buf.size());
	//
	//
	//file = FileSys::open_read_engine("probe_depth.dds");
	//buf.resize(file->size());
	//file->read(buf.data(), buf.size());
	//tex = {};
	//load_dds_file_specialized_format(tex, (uint8_t*)buf.data(), buf.size());

}
int write_hdr_wrapper(const char* filename, int w, int h, int comp, const float* data);
void export_float_texture(GLuint texture,int width, int height, string name)
{
	glBindTexture(GL_TEXTURE_2D, texture);

	const int channels = 3; // assuming RGB
	const int facePixelCount = width * height * channels;

	std::vector<float> faceBuffer(facePixelCount);


		int outputWidth = width;
		int outputHeight = height;



			// Read one face layer
			glGetTextureImage(
				texture, 0,
				GL_RGB,
				GL_FLOAT,
				faceBuffer.size() * sizeof(float),
				faceBuffer.data()
			);


		// Save HDR
		std::string filename = name + ".hdr";

		write_hdr_wrapper(
			filename.c_str(),
			outputWidth,
			outputHeight,
			channels,
			faceBuffer.data()
		);


	glBindTexture(GL_TEXTURE_2D, 0);
}
void ExportCubemapArrayHDR(GLuint texture, int faceSize, int cubemapCount)
{
	glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, texture);

	const int channels = 3; // assuming RGBA
	const int facePixelCount = faceSize * faceSize * channels;

	std::vector<float> faceBuffer(facePixelCount);

	for (int cube = 0; cube < cubemapCount; cube++)
	{
		int outputWidth = faceSize * 6;
		int outputHeight = faceSize;

		std::vector<float> outputImage(outputWidth * outputHeight * channels);

		for (int face = 0; face < 6; face++)
		{
			int layer = cube * 6 + face;

			// Read one face layer
			glGetTextureSubImage(
				texture,
				0,                  // mip level
				0, 0, layer,        // x,y,z offset
				faceSize,
				faceSize,
				1,                  // depth = 1 layer
				GL_RGB,
				GL_FLOAT,
				facePixelCount * sizeof(float),
				faceBuffer.data()
			);

			// Copy into horizontal strip
			for (int y = 0; y < faceSize; y++)
			{
				float* dst = &outputImage[
					(y * outputWidth + face * faceSize) * channels
				];

				float* src = &faceBuffer[
					(y * faceSize) * channels
				];

				memcpy(dst, src, faceSize * channels * sizeof(float));
			}
		}

		// Save HDR
		std::string filename = "cubemap_" + std::to_string(cube) + ".hdr";

		write_hdr_wrapper(
			filename.c_str(),
			outputWidth,
			outputHeight,
			channels,
			outputImage.data()
		);
	}

	glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, 0);
}

void GameSceneGiUtil::bake_ddgi()
{
}
