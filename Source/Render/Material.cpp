#include "Render/Material.h"
#include "Framework/Files.h"
#include "Framework/DictParser.h"
#include "Texture.h"

#include "../Shaders/SharedGpuTypes.txt"
#include "AssetCompile/Someutils.h"
#include "AssetRegistry.h"
#include "Assets/AssetLoaderRegistry.h"
class MaterialAssetMetadata : public AssetMetadata
{
public:
	// Inherited via AssetMetadata
	virtual Color32 get_browser_color() override{ return { 219, 189, 68 }; }
	virtual std::string get_type_name() override { return "Material"; }
	virtual void index_assets(std::vector<std::string>& filepaths) override
	{
		auto tree = FileSys::find_files("./Data/Materials");
		for (auto file : tree) {
			filepaths.push_back(strip_extension(file.substr(17)));
		}
	}
	virtual bool assets_are_filepaths() { return false; }
	virtual std::string root_filepath() override { return "./Data/Materials/"; }
	virtual const ClassTypeInfo* get_asset_class_type() { return &Material::StaticType; }
};
CLASS_IMPL(Material);
REGISTERASSETLOADER_MACRO(Material, &mats);
REGISTER_ASSETMETADATA_MACRO(MaterialAssetMetadata);

using std::string;
MaterialMan mats;
void MaterialMan::reload_all()
{
	assert(0);
}
bool MaterialMan::update_gpu_material_buffer(gpu::Material_Data* buffer, size_t buffer_size, size_t* num_materials)
{
	if (!referenced_materials_dirty)
		return false;
	referenced_materials_dirty = false;

	int i = 0;
	for (auto& mat : mats.materials) {
		if (i >= buffer_size)
			Fatalf("too many materials for gpu buffer!!\n");

		Material& m = *mat.second;
		if (!m.is_loaded_in_memory())
			continue;

		gpu::Material_Data gpumat;
		gpumat.diffuse_tint = m.diffuse_tint;
		gpumat.rough_mult = m.roughness_mult;
		gpumat.metal_mult = m.metalness_mult;
		gpumat.bitmask_flags = 0;
		if (m.billboard == billboard_setting::ROTATE_AXIS)
			gpumat.bitmask_flags |= gpu::MATFLAG_BILLBOARD_ROTATE_AXIS;


		//	gpumat.rough_remap_x = m.roughness_remap_range.x;
			//gpumat.rough_remap_y = m.roughness_remap_range.y;
		m.gpu_material_mapping = i;
		//m.gpu_material_mapping = scene_mats_vec.size();
		buffer[i] = gpumat;
		//scene_mats_vec.push_back(gpumat);
	//}
	//else {
	//	mat.second.gpu_material_mapping = -1;
	//}

		i++;
	}
	*num_materials = i;

	return true;
}


Material* MaterialMan::find_and_make_if_dne(const char* name)
{
	std::string str = name;
	Material* m = find_existing(str);
	if (m) 
		return m;
	m = new Material;
	materials[str] = m;
	m->path = std::move(str);
	return m;
}


#define ENSURE(num) if(line.size() < num) { sys_print("!!! bad material definition %s @ %d\n", matname.c_str(), mat.second.linenum); continue;}
Material* MaterialMan::load_material_file(const char* path, bool overwrite)
{
	auto fileos = FileSys::open_read_os(path);
	if (!fileos) {
		sys_print("!!! Couldn't open material file %s\n", path);
		return nullptr;
	}

	DictParser file;
	file.load_from_file(fileos.get());

	string key;
	string temp2;

	StringView name;
	if (!file.read_string(name) || !file.expect_item_start()) {
		sys_print("!!! bad material def %s\n", path);
		return nullptr;
	}

	std::string matname = std::string(name.str_start, name.str_len);

	Material* gs = nullptr;

	{
		gs = new Material;
		materials[matname] = gs;
		gs->path = matname;
		gs->material_id = cur_mat_id++;
	}

	StringView tok;
	while (file.read_string(tok) && !file.is_eof() && !file.check_item_end(tok)) {


		if (tok.cmp("pbr_full")) {
			auto ss = tok.to_stack_string();
			const char* str_get = ss.c_str();
			const char* path = string_format("%s_albedo.png", str_get);
			gs->get_image(material_texture::DIFFUSE) = g_imgs.create_unloaded_ptr(path);
			path = string_format("%s_normal-ogl.png", str_get);
			gs->get_image(material_texture::NORMAL) = g_imgs.create_unloaded_ptr(path);
			path = string_format("%s_ao.png", str_get);
			gs->get_image(material_texture::AO) = g_imgs.create_unloaded_ptr(path);
			path = string_format("%s_roughness.png", str_get);
			gs->get_image(material_texture::ROUGHNESS) = g_imgs.create_unloaded_ptr(path);
			path = string_format("%s_metallic.png", str_get);
			gs->get_image(material_texture::METAL) = g_imgs.create_unloaded_ptr(path);
		}

		// assume that ref_shaderX will be defined in the future, so create them if they haven't been
		else if (tok.cmp("ref_shader1")) {
			file.read_string(tok);
			gs->references[0] = find_and_make_if_dne(tok.to_stack_string().c_str());
		}
		else if (tok.cmp("ref_shader2")) {
			file.read_string(tok);
			gs->references[1] = find_and_make_if_dne(tok.to_stack_string().c_str());
		}

		else if (tok.cmp("albedo")) {
			file.read_string(tok);
			auto ss = tok.to_stack_string();
			const char* str_get = ss.c_str();
			gs->get_image(material_texture::DIFFUSE) = g_imgs.create_unloaded_ptr(str_get);
		}
		else if (tok.cmp("normal")) {
			file.read_string(tok);
			auto ss = tok.to_stack_string();
			const char* str_get = ss.c_str();
			gs->get_image(material_texture::NORMAL) = g_imgs.create_unloaded_ptr(str_get);
		}
		else if (tok.cmp("ao")) {
			file.read_string(tok);
			auto ss = tok.to_stack_string();
			const char* str_get = ss.c_str();
			gs->get_image(material_texture::AO) = g_imgs.create_unloaded_ptr(str_get);
		}
		else if (tok.cmp("rough")) {
			file.read_string(tok);
			auto ss = tok.to_stack_string();
			const char* str_get = ss.c_str();
			gs->get_image(material_texture::ROUGHNESS) = g_imgs.create_unloaded_ptr(str_get);
		}
		else if (tok.cmp("metal")) {
			file.read_string(tok);
			auto ss = tok.to_stack_string();
			const char* str_get = ss.c_str();
			gs->get_image(material_texture::METAL) = g_imgs.create_unloaded_ptr(str_get);
			gs->metalness_mult = 1.0;
		}
		else if (tok.cmp("special")) {
			file.read_string(tok);
			auto ss = tok.to_stack_string();
			const char* str_get = ss.c_str();
			gs->get_image(material_texture::SPECIAL) = g_imgs.create_unloaded_ptr(str_get);
		}
		else if (tok.cmp("tint")) {
			glm::vec3 t;
			file.read_float3(t.x, t.y, t.z);
			gs->diffuse_tint = glm::vec4(t, gs->diffuse_tint.w);
		}
		else if (tok.cmp("metal_val")) {
			file.read_float(gs->metalness_mult);
		}
		else if (tok.cmp("rough_val")) {
			file.read_float(gs->roughness_mult);
		}
		else if (tok.cmp("rough_remap")) {
			glm::vec2 v;
			file.read_float2(v.x, v.y);
			gs->roughness_remap_range = v;
		}
		else if (tok.cmp("shader")) {
			file.read_string(tok);
			if (tok.cmp("blend2")) gs->type = material_type::TWOWAYBLEND;
			else if (tok.cmp("wind")) gs->type = material_type::WINDSWAY;
			else if (tok.cmp("water")) {
				gs->blend = blend_state::BLEND;
				gs->type = material_type::WATER;
			}

			else sys_print("unknown shader type %s\n", tok.to_stack_string().c_str());
		}
		else if (tok.cmp("alpha")) {
			file.read_string(tok);
			if (tok.cmp("add"))
				gs->blend = blend_state::ADD;
			else if (tok.cmp("blend"))
				gs->blend = blend_state::BLEND;
			else if (tok.cmp("test"))
				gs->alpha_tested = true;

		}
		else if (tok.cmp("billboard"))
			gs->billboard = billboard_setting::FACE_CAMERA;
		else if (tok.cmp("billboard_axis"))
			gs->billboard = billboard_setting::ROTATE_AXIS;
	}

	return gs;
}
#undef ENSURE;
Material* MaterialMan::find_existing(const std::string& path)
{
	if (materials.find(path) != materials.end()) {
		auto mat = materials.find(path)->second;
		ensure_data_is_loaded(mat);
		return mat;
	}
	return nullptr;
}
Material* MaterialMan::create_temp_shader(const char* name)
{
	std::string str = name;
	Material* gs = find_existing(str);
	if (gs) 
		return gs;
	gs = new Material;
	materials[str]=gs;
	gs->path = std::move(str);
	gs->material_id = cur_mat_id++;
	gs->is_loaded = true;
	gs->is_referenced = true;
	referenced_materials_dirty = true;

	return gs;
}


void MaterialMan::ensure_data_is_loaded(Material* mat)
{
	if (!mat->is_loaded) {
		for (int i = 0; i < (int)material_texture::COUNT; i++) {
			if (mat->images[i] && !mat->images[i]->is_loaded_in_memory()) {
				bool good = g_imgs.load_texture(mat->images[i]->get_name(), mat->images[i]);
				if (!good) mat->images[i] = nullptr;
			}
		}

		for (int i = 0; i < Material::MAX_REFERENCES; i++) {
			if (mat->references[i] && !mat->references[i]->is_loaded_in_memory())
				ensure_data_is_loaded(mat);
		}

		mat->is_loaded = true;

		referenced_materials_dirty = true;
	}
	mat->is_referenced = true;
}

Material* MaterialMan::find_for_name(const char* name)
{
	auto mat = find_existing(name);
	if (mat)
		return mat;
	else {
		std::string path = "./Data/Materials/";
		path += name;
		path += ".txt";
		auto mat = load_material_file(path.c_str(), true);
		if (!mat)
			return nullptr;
		ensure_data_is_loaded(mat);
		return mat;
	}
}

void MaterialMan::init()
{
	sys_print("--------- Materials Init ---------\n");

	fallback = create_temp_shader("_fallback");
	fallback->is_loaded = true;

	unlit = create_temp_shader("_unlit");
	unlit->blend = blend_state::BLEND;
	unlit->type = material_type::UNLIT;
	unlit->is_loaded = true;


	outline_hull = create_temp_shader("_outlinehull");
	outline_hull->blend = blend_state::OPAQUE;
	outline_hull->type = material_type::OUTLINE_HULL;
	outline_hull->backface = true;
	outline_hull->is_loaded = true;
}