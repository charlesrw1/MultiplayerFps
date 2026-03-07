#include "MaterialLocal.h"
#include "Framework/Files.h"

#include "Framework/DictParser.h"
#include "AssetCompile/Someutils.h"

#include "DrawLocal.h"

#include "Render/Texture.h"
#include "Render/Model.h"

#include <algorithm>
#include <stdexcept>
#include <fstream>
#include <array>
#include "glad/glad.h"

#include "IGraphsDevice.h"
#include "Assets/AssetRegistry.h"
#include "Assets/AssetDatabase.h"
#include "Framework/StringUtils.h"
#include "imgui.h"
#include "EditorPopups.h"
MaterialManagerLocal matman;
MaterialManagerPublic* imaterials = &matman;

//extern IEditorTool* g_mateditor;

ConfigVar material_print_debug("material_print_debug", "1", CVAR_DEV | CVAR_BOOL, "");

MulticastDelegate<MaterialInstance*> MaterialInstance::on_material_loaded;


#ifdef EDITOR_BUILD
class MaterialAssetMetadata : public AssetMetadata
{
public:
	MaterialAssetMetadata() {
		extensions.push_back("mi");
		extensions.push_back("mm");
	}

	// Inherited via AssetMetadata
	virtual Color32 get_browser_color()  const override { return Color32( 219, 189, 68 ); }
	virtual std::string get_type_name()  const override { return "Material"; }


	virtual const ClassTypeInfo* get_asset_class_type()  const override { return &MaterialInstance::StaticType; }

	void draw_browser_menu(const string& path)const final {

	}

	//IEditorTool* tool_to_edit_me() const override { return g_mateditor;  }
};

REGISTER_ASSETMETADATA_MACRO(MaterialAssetMetadata);
#endif

inline std::string remove_filename_from_path(std::string& path)
{
	auto find = path.rfind("/");
	if (find == std::string::npos) {
		return "";
	}
	else {
		std::string wo_filename = path.substr(0, find);
		path = path.substr(find);
		return wo_filename;
	}
}


class MasterMaterialExcept : public std::runtime_error
{
public:
	MasterMaterialExcept(const std::string& error) 
		: std::runtime_error(error) {}
};


void MaterialShaderTable::recompile_for_material(MasterMaterialImpl* mat)
{
	//if (material_print_debug.get_bool())
	//	sys_print(Debug, "recompiling material %s\n", mat->self->get_name().c_str());

	uint32_t id = mat->material_id;
	for (auto& pair : shader_key_to_program_handle) {
		uint32_t this_id = pair.first & ((1ul << 27ul) - 1ul);
		if (this_id == id) {
			draw.get_prog_man().recompile(pair.second);
		}
	}
	// do one default shader compile so we can aproximately tell if the shader is actually invalid and shouldnt be used
	program_handle default_h = matman.get_mat_shader(nullptr, mat->self,0);
	mat->is_compilied_shader_valid = !draw.get_prog_man().did_shader_fail(default_h);
	if (!mat->is_compilied_shader_valid)
		sys_print(Error, "recompile_for_material: material is invalid %s\n", mat->self->get_name().c_str());
}

MaterialShaderTable::MaterialShaderTable()
{

}

program_handle MaterialShaderTable::lookup(shader_key key)
{
	uint32_t key32 = key.as_uint32();
	auto find = shader_key_to_program_handle.find(key32);
	return find == shader_key_to_program_handle.end() ? -1 : find->second;
}
void MaterialShaderTable::insert(shader_key key, program_handle handle)
{
	shader_key_to_program_handle.insert({ key.as_uint32(), handle });
}



program_handle MaterialManagerLocal::compile_mat_shader(const MaterialInstance* mat, shader_key key)
{
	// FIXME: make this faster

	std::string name = FileSys::get_game_path() + ("/"  + mat->get_name());
	name = strip_extension(name);
	name += "_shader.glsl";

	std::string params;
	if (key.has_flag(MSF_ANIMATED)) params += "ANIMATED,";
	if (key.has_flag(MSF_DITHER)) params += "DITHER,";
	if (key.has_flag(MSF_EDITOR_ID)) params += "EDITOR_ID,";
	if (key.has_flag(MSF_DEPTH_ONLY)) params += "DEPTH_ONLY,";
	if (key.has_flag(MSF_DEBUG)) params += "DEBUG_SHADER,";
	if (key.has_flag(MSF_LIGHTMAPPED)) params += "LIGHTMAPPED,";
	if (key.has_flag(MSF_IS_FORCED_FORWARD))params += "THUMBNAIL_FORWARD,";
	if (key.has_flag(MSF_NO_TAA))params += "NO_TAA,";
	if (!params.empty())params.pop_back();

	if(material_print_debug.get_bool())
		sys_print(Debug,"compiling shader: %s %s\n", mat->get_name().c_str(), params.c_str());

	const bool is_tesselation = mat->get_master_material()->usage == MaterialUsage::Terrain;
	program_handle handle = draw.get_prog_man().create_single_file(name, is_tesselation, params);
	ASSERT(handle != -1);

	mat_shader_table.insert(key, handle);
	return handle;
}
#include "tracy/public/tracy/Tracy.hpp"
program_handle MaterialManagerLocal::get_mat_shader(
	const Model* mod, 
	const MaterialInstance* mat,
	int flags)
{
	//ZoneScopedN("get_mat_shader");
	const MasterMaterialImpl* mm = mat->get_master_material();

	//bool is_animated = mod && mod->has_bones() && has_animated_matricies;

	shader_key key;
	key.material_id = mm->material_id;
	key.msf_flags = flags;

	program_handle handle = mat_shader_table.lookup(key);
	if (handle != -1) 
		return handle;
	return compile_mat_shader(mm->self, key);	// dynamic compilation ...
}

void MaterialManagerLocal::add_to_dirty_list(MaterialInstance* mat) {
	dirty_list.insert(mat);
}

void MaterialManagerLocal::remove_from_dirty_list_if_it_is(MaterialInstance* mat) {
	dirty_list.remove(mat);
}

void MaterialManagerLocal::free_material_instance(MaterialInstance* m) {
	if (!m->impl)
		return;
	remove_from_dirty_list_if_it_is(m);
	if (m->impl->gpu_buffer_offset != MaterialImpl::INVALID_MAPPING) {
		mat_offset_table->unregister_material(m);
	}

}

#include "Assets/AssetDatabase.h"

const MasterMaterialImpl* MaterialInstance::get_master_material() const
{
	return impl->get_master_impl();
}

bool MaterialInstance::is_this_a_master_material() const
{
	return impl&&impl->masterMaterial;
}
bool MaterialInstance::is_this_a_dynamic_material() const
{
	return impl && impl->is_dynamic_material;
}

bool MaterialInstance::load_asset(IAssetLoadingInterface* loading)
{
	if (!impl) {
		impl = std::make_unique<MaterialImpl>();
		bool good = impl->load_from_file(this,loading);
		assert(!good || impl && impl->is_valid());
		
		if (!good)
			impl.reset();
		return good;
	}
	else {
		// impl already exists, we have to sweep references
		// since we cant be uninstalled, this fakes "loading" the resource again
		sweep_references(loading);
	}
	//assert(impl && impl->masterMaterial);
	return impl.get();
}
void MaterialInstance::sweep_references(IAssetLoadingInterface* loading)const {
	if (!impl)
		return;
}
void MaterialInstance::uninstall()
{
	matman.remove_from_dirty_list_if_it_is(this);
	matman.free_material_instance(this);
	impl.reset();
}
void MaterialInstance::post_load()
{
	if (did_load_fail())
		return;
	assert(impl);
	if (!impl->has_called_post_load_already) {
		assert(impl->is_valid());
		impl->post_load(this);
		impl->has_called_post_load_already = true;
	}

	MaterialInstance::on_material_loaded.invoke(this);
}

// BLECH!!
void MaterialInstance::move_construct(IAsset* _other)
{
	auto other = (MaterialInstance*)_other;
	//uninstall();	// fixme: unsafe for materials already referencing us

	std::shared_ptr<MaterialInstance> myParentMat = impl->masterMaterial;
	*this = std::move(*other);
	impl->self = this;
	if (impl->masterImpl) {
		impl->masterImpl->self = this;
	}
	else {
	
		std::shared_ptr<MaterialInstance> newParent = g_assets.find_sync_sptr<MaterialInstance>(impl->masterMaterial->get_name());
		assert(newParent);
		impl->masterMaterial = newParent;
		assert(impl->is_valid());
	}

	std::vector<IAsset*> ar;
	g_assets.get_assets_of_type(ar, &MaterialInstance::StaticType);
	for (auto a : ar) {
		if (a == this) 
			continue;
		MaterialInstance* mi = (MaterialInstance*)a;
		if (mi->impl && mi->impl->masterMaterial.get() == this) {
			g_assets.reload_sync(mi);
		}
	}

//	other->uninstall();

}

MaterialInstance* MaterialManagerLocal::create_dynmaic_material_unsafier(const MaterialInstance* parent) {
	assert(parent);
	// more bs crap
	std::shared_ptr<MaterialInstance> as_sptr = g_assets.find_sync_sptr<MaterialInstance>(parent->get_name());

	MaterialInstance* dynamic_mat = dynamic_mat_allocator.allocate_dynamic();
	ASSERT(dynamic_mat);

	dynamic_mat->impl = std::make_unique<MaterialImpl>();
	dynamic_mat->impl->self = dynamic_mat;
	dynamic_mat->impl->init_from(as_sptr);
	dynamic_mat->impl->is_dynamic_material = true;
	dynamic_mat->impl->post_load(dynamic_mat);	// add to dirty list, set material id

	return dynamic_mat;
}

void MaterialManagerLocal::free_dynamic_material(MaterialInstance* mat) {
	if (!mat)
		return;
	dynamic_mat_allocator.free_dynamic(mat);
}



void MaterialImpl::post_load(MaterialInstance* self)
{
	ASSERT(!has_called_post_load_already);
	ASSERT(this->self == self);
	if (masterImpl) {
		masterImpl->material_id = matman.get_next_master_id();
		ASSERT(masterImpl->self == self);
	}
	matman.add_to_dirty_list(self);
}

void MaterialImpl::init_from(const std::shared_ptr<MaterialInstance>& parent)
{
	auto parent_master = parent->get_master_material();
	masterMaterial = parent;
	params.resize(parent_master->param_defs.size());
	for (int i = 0; i < parent_master->param_defs.size(); i++)
		params[i] = parent->impl->params[i];
	texture_bindings.resize(parent_master->num_texture_bindings, nullptr);
}

void MaterialImpl::load_master(MaterialInstance* self, IFile* file,IAssetLoadingInterface* loading)
{
	masterImpl = std::make_unique<MasterMaterialImpl>();
	masterImpl->self = self;
	masterImpl->load_from_file(self->get_name(), file,loading);
	
	// init default instance, textures get filled in the dirty list
	params.resize(masterImpl->param_defs.size());
	for (int i = 0; i < masterImpl->param_defs.size(); i++)
		params[i] = masterImpl->param_defs[i].default_value;
	texture_bindings.resize(masterImpl->num_texture_bindings, nullptr);
}

MaterialInstance::MaterialInstance()
{

}
MaterialInstance::~MaterialInstance()
{

	if (impl&&impl->is_dynamic_material) {
		matman.free_dynamic_material(this);
	}
#ifdef _DEBUG
	std::vector<IAsset*> mats;
	g_assets.get_assets_of_type(mats, &MaterialInstance::StaticType);
	for (auto mat : mats) {
		if (mat == this) continue;
		assert(mat);
		auto matPtr = (MaterialInstance*)mat;
		if (!matPtr->impl) continue;
		assert(!impl||matPtr->impl->masterMaterial.get() !=this);
	}
	std::vector<IAsset*> mods;
	g_assets.get_assets_of_type(mods, &Model::StaticType);
	for (auto mod : mods) {
		auto modPtr = (Model*)mod;
		assert(mod);

		for (int i = 0; i < modPtr->get_num_materials(); i++) {
			assert(modPtr->get_material(i) != this);
		}
	}
#endif
}

MaterialInstance* MaterialInstance::load(const std::string& path)
{
	return g_assets.find_sync<MaterialInstance>(path).get();
}

MaterialInstance* MaterialInstance::alloc_dynamic_mat(MaterialInstance* from)
{
	return matman.create_dynmaic_material_unsafier(from);
}

void MaterialInstance::free_dynamic_mat(MaterialInstance* mat)
{
	return matman.free_dynamic_material(mat);
}

void MaterialImpl::load_instance(MaterialInstance* self, IFile* file, IAssetLoadingInterface* loading)
{
	const auto& fullpath = self->get_name();

	// for reloading
	params.clear();

	DictParser in;
	in.load_from_file(file);
	StringView tok;
	try {
		if (!in.read_string(tok) || !tok.cmp("TYPE") || !in.read_string(tok) || !tok.cmp("MaterialInstance")) {
			throw MasterMaterialExcept("Expceted TYPE MaterialInstance");
		}
		if (!in.read_string(tok) || !tok.cmp("PARENT") || !in.read_string(tok)) {
			throw MasterMaterialExcept("Expceted PARENT ...");
		}
		std::string parent_mat = to_std_string_sv(tok);
		auto parent = g_assets.find_sync_sptr<MaterialInstance>(parent_mat);// loading->load_asset(&MaterialInstance::StaticType, parent_mat);
	//	AssetPtr<MaterialInstance> parent = mat->cast_to<MaterialInstance>();
		if (!parent||!parent->impl)
			throw MasterMaterialExcept("Couldnt find parent material" + fullpath);

		init_from(parent);
		assert(masterMaterial);
		auto masterMat = get_master_impl();
		assert(params.size() == masterMat->param_defs.size());
		while (in.read_string(tok) && !in.is_eof()) {
			if (tok.cmp("VAR")) {
				in.read_string(tok);
				std::string paramname = to_std_string_sv(tok);
				int index = 0;
				auto ptr = masterMat->find_definition(paramname, index);
				if (!ptr)
					throw MasterMaterialExcept("Couldnt find parent parameter: " + paramname);
				assert(index < params.size()&&index>=0);
				auto& myparam = params[index];

				switch (ptr->default_value.type)
				{
				case MatParamType::Bool: {
					int b = 0; in.read_int(b); myparam.boolean = b;
				}break;
				case MatParamType::Float: {
					float f = 0.0;
					in.read_float(f);
					myparam.scalar = f;
				}break;
				case MatParamType::Vector: {
					int r, g, b, a;
					in.read_int(r);
					in.read_int(g);
					in.read_int(b);
					in.read_int(a);
					Color32 c;
					c.r = r;
					c.g = g;
					c.b = b;
					c.a = a;
					myparam.color32 = c.to_uint();
				}break;
				case MatParamType::FloatVec: {
					glm::vec4 v;
					in.read_float(v.x);
					in.read_float(v.y);
					in.read_float(v.z);
					in.read_float(v.w);
					myparam.vector = v;
				}break;
				case MatParamType::Texture2D:
				{
					in.read_string(tok);
					string s = to_std_string_sv(tok);
					//auto tex = loading->load_asset(&Texture::StaticType, s);
					myparam.tex = g_assets.find_sync_sptr<Texture>(s);// tex->cast_to<Texture>();
					if (!myparam.tex) {
						sys_print(Error, "MaterialImpl::load_instance: texture not found: %s\n", s.c_str());
						throw MasterMaterialExcept("Texture not found: " + s);
					}
					assert(myparam.tex);
				}break;

				default:
					throw MasterMaterialExcept("bad VAR type");
					break;
				}
			}
			else
				throw MasterMaterialExcept("can only have VAR option for materialinstances");
		}
	}
	catch (MasterMaterialExcept m) {
		throw MasterMaterialExcept(string_format("line:%d %s", in.get_last_line(), m.what()));
	}
}

bool MaterialImpl::load_from_file(MaterialInstance* self, IAssetLoadingInterface* loading)
{
	this->self = self;
	const auto& name = self->get_name();
	// try to find master file
	try {
		auto file = FileSys::open_read_game(name.c_str());
		if (!file)
			throw MasterMaterialExcept("couldn't mm/mi open file");
		if (has_extension(name, "mm")) {
			load_master(self, file.get(), loading);
		}
		else {
			load_instance(self, file.get(), loading);
		}
	}
	catch (MasterMaterialExcept exppt) {
		sys_print(Error, "error loading material %s: %s\n", name.c_str(), exppt.what());
		return false;
	}
	return true;
}
extern ConfigVar developer_mode;

const MaterialParameterDefinition* MasterMaterialImpl::find_definition(const std::string& str, int& index) const {
	for (int i = 0; i < param_defs.size(); i++)
		if (param_defs[i].name == str) {
			index = i;
			return &param_defs[i];
		}
	return nullptr;
}

static const char* get_master_shader_path(MaterialUsage usage)
{
	const char* master_shader_path = "MASTER/MasterDeferredShader.txt";
	if (usage == MaterialUsage::Terrain)
		master_shader_path = "MASTER/MasterTerrainShader.txt";
	else if (usage == MaterialUsage::Decal)
		master_shader_path = "MASTER/MasterDecalShader.txt";
	else if (usage == MaterialUsage::UI)
		master_shader_path = "MASTER/MasterUIShader.txt";
	else if (usage == MaterialUsage::Postprocess)
		master_shader_path = "MASTER/MasterPostProcessShader.txt";
	else if (usage == MaterialUsage::Particle)
		master_shader_path = "MASTER/MasterParticleShader.txt";
	return master_shader_path;
}

void MasterMaterialImpl::load_from_file(const std::string& fullpath, IFile* file, IAssetLoadingInterface* loading)
{

	DictParser in;
	in.load_from_file(file);
	std::string vs_code;
	std::string fs_code;
	std::vector<InstanceData> inst_dats;
	StringView tok;

	try {
		if (!in.read_string(tok) || !tok.cmp("TYPE") || !in.read_string(tok) || !tok.cmp("MaterialMaster")) {
			throw MasterMaterialExcept("Expceted TYPE MaterialMaster");
		}
		auto parse_options = [&](const std::vector<std::string>& opts)->int {
			in.read_string(tok);
			for (int i = 0; i < opts.size(); i++) {
				if (tok.cmp(opts[i].c_str()))
					return i;
			}
			throw MasterMaterialExcept("Unknown option " + to_std_string_sv(tok));
		};
		while (in.read_string(tok) && !in.is_eof()) {
			if (tok.cmp("VAR")) {
				MaterialParameterDefinition def;
				def.default_value.type = (MatParamType)parse_options({ "_null_","float_vec4","float","vec4","bool","texture2D","constTexture2D" });
				in.read_string(tok);
				def.name = to_std_string_sv(tok);
				def.hashed_name = StringName(def.name.c_str());

				switch (def.default_value.type)
				{
				case MatParamType::Bool: {
					int b = 0; in.read_int(b); def.default_value.boolean = b;
				}break;
				case MatParamType::Float: {
					float f = 0.0;
					in.read_float(f);
					def.default_value.scalar = f;
				}break;
				case MatParamType::Vector: {
					int r, g, b, a;
					in.read_int(r);
					in.read_int(g);
					in.read_int(b);
					in.read_int(a);
					Color32 c;
					c.r = r;
					c.g = g;
					c.b = b;
					c.a = a;
					def.default_value.color32 = c.to_uint();
				}break;
				case MatParamType::FloatVec: {
					glm::vec4 v;
					in.read_float(v.x);
					in.read_float(v.y);
					in.read_float(v.z);
					in.read_float(v.w);
					def.default_value.vector = v;
				}break;
				case MatParamType::Texture2D:
				{
					in.read_string(tok);
					def.default_value.tex = g_assets.find_sync_sptr<Texture>(to_std_string_sv(tok));
					assert(def.default_value.tex);
				}break;

				default:
					throw MasterMaterialExcept("bad VAR type");
					break;
				}

				param_defs.push_back(def);
			}
			else if (tok.cmp("OPT")) {

				in.read_string(tok);

				if (tok.cmp("AlphaTested")) {
					alpha_tested = parse_options({ "false","true" });
				}
				else if (tok.cmp("BlendMode")) {
					blend = (BlendState)parse_options({ "Opaque","Blend","Add","Mult","Screen","PreMult"});
				}
				else if (tok.cmp("LightingMode")) {
					light_mode = (LightingMode)parse_options({ "Lit","Unlit" });
				}
				else if (tok.cmp("ShowBackfaces")) {
					backface = parse_options({ "false","true" });
				}
				else if (tok.cmp("WriteAlbedo")) {
					decal_affect_albedo = true;
				}
				else if (tok.cmp("WriteNormal")) {
					decal_affect_normal = true;
				}
				else if (tok.cmp("WriteEmissive")) {
					decal_affect_emissive = true;
				}
				else if (tok.cmp("WriteRoughMetal")) {
					decal_affect_roughmetal = true;
				}
				else
					throw MasterMaterialExcept("Unknown OPT " + to_std_string_sv(tok));
			}
			else if (tok.cmp("UBO")) {
				in.read_string(tok);

				ASSERT(0);
			}
			else if (tok.cmp("INST")) {
				in.read_string(tok);
				InstanceData id;
				id.is_vector_type = parse_options({ "float","vec4" });

				in.read_string(tok);
				id.name = to_std_string_sv(tok);
				int index = 0;
				in.read_int(index);
				if (index < 0 || index >= MAX_INSTANCE_PARAMETERS) {
					throw MasterMaterialExcept("INST index not valid " + std::to_string(index));
				}
				inst_dats.push_back(id);
			}
			else if (tok.cmp("_VS_BEGIN")) {
				while (in.read_line(tok)) {
					std::string line = to_std_string_sv(tok);
					if (line.find("_VS_END") != std::string::npos) {
						break;
					}
					vs_code += line + "\n";
				}
			}
			else if (tok.cmp("_FS_BEGIN")) {
				while (in.read_line(tok)) {
					std::string line = to_std_string_sv(tok);
					if (line.find("_FS_END") != std::string::npos) {
						break;
					}
					fs_code += line + "\n";
				}
			}
			else if (tok.cmp("DOMAIN")) {
				usage = (MaterialUsage)parse_options({ "Default","PostProcess","Terrain","Decal","UI","Particle" });
			}
			else {
				throw MasterMaterialExcept("Unknown cmd : " + to_std_string_sv(tok));
			}
		}
	}
	catch (MasterMaterialExcept m) {
		throw MasterMaterialExcept(string_format("line:%d %s", in.get_last_line(), m.what()));
	}


	std::sort(param_defs.begin(), param_defs.end(), 
		[](const auto& a, const auto& b) -> bool {
			return (int)a.default_value.type < (int)b.default_value.type;
		}
	);

	uint32_t param_ofs = 0;
	uint32_t tex_ofs = 0;

	for (int i = 0; i < param_defs.size(); i++) {
		auto& pd = param_defs[i];
		switch (pd.default_value.type)
		{
		case MatParamType::FloatVec:
			pd.offset = param_ofs;
			param_ofs += 16;	// sizeof(float)*4
			break;
		case MatParamType::Float:
		case MatParamType::Vector:
			pd.offset = param_ofs;
			param_ofs += 4; // sizeof(float) or sizeof(color32)
			break;
		case MatParamType::Bool:
			pd.offset = param_ofs;
			pd.offset += 1;
			break;
		case MatParamType::Texture2D:
			pd.offset = tex_ofs++;
			break;
		default:
			break;
		}
	}

	if (param_ofs > MATERIAL_SIZE) {
		throw MasterMaterialExcept("Too many material parameters exceeds max material size of 64 bytes");
	}

	num_texture_bindings = tex_ofs;

#ifdef EDITOR_BUILD
	// material compilation, outputs a glsl file
	// i have another layer beneath this  (in Prog_Man) that compiles the glsl to a platform binary (which is cached), but that 
	// layer depends on dynamic usage state like if its an animator object, editor mode, etc.
	// so it has to be done there.
	//if (developer_mode.get_bool()) 
	{
		auto out_glsl_path = strip_extension(fullpath) + "_shader.glsl";
		auto outGlslFile = FileSys::open_read_game(out_glsl_path);
		auto masterFile = FileSys::open_read_engine(("Shaders\\" + std::string(get_master_shader_path(usage))).c_str());
		ASSERT(masterFile);

		if (!outGlslFile || outGlslFile->get_timestamp() < file->get_timestamp() || outGlslFile->get_timestamp() < masterFile->get_timestamp()) {
			masterFile.reset(); // close master
			sys_print(Debug, "MasterMaterialImpl::load_from_file: updating .glsl because its out of date (%s)\n", out_glsl_path.c_str());
			string outStr = create_glsl_shader(vs_code, fs_code, inst_dats);
			outGlslFile.reset(); // close it
			outGlslFile = FileSys::open_write_game(out_glsl_path);
			if (!outGlslFile) {
				sys_print(Error, "MasterMaterialImpl::load_from_file: couldn't open file to write .glsl file (%s)\n", out_glsl_path.c_str());
			}
			else {
				outGlslFile->write(outStr.data(), outStr.size());
			}
		}
	}
#endif
}

#ifdef EDITOR_BUILD

static void read_and_add_recursive(std::string filepath, std::string& text);
static void read_instream(std::istream& stream, std::string& text)
{

	static const char* const INCLUDE_SPECIFIER = "#include";


	std::string line;
	while (std::getline(stream, line)) {

		size_t pos = line.find(INCLUDE_SPECIFIER);
		if (pos == std::string::npos)
			text.append(line + '\n');
		else if(pos==0) {
			size_t start_file = line.find('\"');
			if (start_file == std::string::npos) {
				throw MasterMaterialExcept("include not followed with filepath\n");
			}
			size_t end_file = line.rfind('\"');
			if (end_file == start_file) {
				throw MasterMaterialExcept("include missing ending quote\n");
			}
			read_and_add_recursive(line.substr(start_file + 1, end_file - start_file - 1), text);
		}
	}
}
#include <sstream>

static void read_and_add_recursive(std::string filepath, std::string& text)
{
	static const char* const SHADER_PATH = "Shaders\\";
	std::string path(SHADER_PATH);
	path += filepath;
	std::ifstream infile(path);
	if (!infile) {
		throw MasterMaterialExcept("couldn't open shader path" + path);
	}
	read_instream(infile, text);
}
static void expand_includes_of_text(std::string& inouttext) {
	std::stringstream ss;
	ss << inouttext;
	inouttext.clear();
	read_instream(ss, inouttext);
}

static void replace(std::string& str, const std::string& from, const std::string& to) {
	if (from.empty())
		return;
	size_t start_pos = 0;
	while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
		str.replace(start_pos, from.length(), to);
		start_pos += to.length();
	}
}


static void replace_variable(std::string& str, const std::string& from, const std::string& to) {
	if (from.empty())
		return;
	size_t start_pos = 0;
	while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
		// check the characters before and after
		if (start_pos > 0 && isalnum(str.at(start_pos - 1))) {
			start_pos += from.size();
			continue;
		}
		if (start_pos + from.size() < str.size() - 1 && isalnum(str.at(start_pos + from.size()))) {
			start_pos += from.size();
			continue;
		}
		str.replace(start_pos, from.length(), to);
		start_pos += to.length();
	}
}



std::string MasterMaterialImpl::create_glsl_shader(
	std::string& vs_code,
	std::string& fs_code,
	const std::vector<InstanceData>& instdat
)
{
	std::string masterShader;

	const char* master_shader_path = get_master_shader_path(usage);

	// handle defines here?
	if (usage == MaterialUsage::Decal) {
		if (decal_affect_albedo)
			masterShader += "#define DECAL_ALBEDO_WRITE\n";
		if(decal_affect_emissive)
			masterShader += "#define DECAL_EMISSIVE_WRITE\n";
		if (decal_affect_normal)
			masterShader += "#define DECAL_NORMAL_WRITE\n";
		if (decal_affect_roughmetal)
			masterShader += "#define DECAL_ROUGHMETAL_WRITE\n";
	}
	if (is_alphatested()) {
		masterShader += "#define ALPHATEST\n";
	}
	if (blend != BlendState::OPAQUE) {
		masterShader+= "#define FORWARD_SHADER\n";
		if (light_mode == LightingMode::Lit) {
			masterShader += "#define FORWARD_LIT\n";
		}

	}




	try {
		read_and_add_recursive(master_shader_path, masterShader);
	}
	catch (MasterMaterialExcept m) {
		throw MasterMaterialExcept(string_format("in master shader %s: %s", master_shader_path, m.what()));
	}

	std::string actual_vs_code;

	auto autogen_code = [&](const char* for_what, std::string& actual_code, std::string& inp_code) {

		actual_code += "// Texture defs\n";
		for (int i = 0; i < param_defs.size(); i++) {
			if (param_defs[i].default_value.type == MatParamType::Texture2D) {
				int index = param_defs[i].offset;
				ASSERT(index >= 0 && index < 32);

				// layout(binding=0) uniform sampler2D exampleTexture;
				actual_code += "layout(binding = ";
				actual_code += std::to_string(index);
				actual_code += " ) uniform sampler2D ";
				actual_code += param_defs[i].name;
				actual_code += ";\n";
			}
		}
		actual_code += "\n";

		// replace any parameter data references
		for (int i = 0; i < param_defs.size(); i++) {
			auto& def = param_defs[i];
			auto type = def.default_value.type;
			if (type == MatParamType::Texture2D)
				continue;

			const uint32_t UINT_OFS = def.offset / 4;
			const uint32_t BYTE_OFS = def.offset % 4;

			std::string replacement_code;
			switch (type) {
			case MatParamType::Float:
				replacement_code = "uintBitsToFloat( _material_param_buffer[FS_IN_Matid + ";
				replacement_code += std::to_string(UINT_OFS) + "] )";
				break;
			case MatParamType::Vector:
				replacement_code = "unpackUnorm4x8( _material_param_buffer[FS_IN_Matid + ";
				replacement_code += std::to_string(UINT_OFS) + "] )";
				break;
			case MatParamType::FloatVec:
				replacement_code = "vec4( ";
				replacement_code += "uintBitsToFloat(_material_param_buffer[FS_IN_Matid + " + std::to_string(UINT_OFS) + "] ), ";
				replacement_code += "uintBitsToFloat(_material_param_buffer[FS_IN_Matid + " + std::to_string(UINT_OFS + 1) + "] ), ";
				replacement_code += "uintBitsToFloat(_material_param_buffer[FS_IN_Matid + " + std::to_string(UINT_OFS + 2) + "] ), ";
				replacement_code += "uintBitsToFloat(_material_param_buffer[FS_IN_Matid + " + std::to_string(UINT_OFS + 3) + "] )";
				replacement_code += ")";
				break;
			}
			replacement_code += " /* ";
			replacement_code += def.name;
			replacement_code += " */ ";

			replace_variable(inp_code, def.name, replacement_code);
		}
		actual_code += "\n";
		actual_code += inp_code;
		try {
			expand_includes_of_text(actual_code);
		}
		catch (MasterMaterialExcept m) {
			throw MasterMaterialExcept(string_format("during codegen %s: %s", for_what, m.what()));
		}
	};

	if (!vs_code.empty()) { 
		autogen_code("VS",actual_vs_code, vs_code);
	}
	else
		actual_vs_code = "void VSmain() { }\n";

	std::string actual_fs_code;
	actual_fs_code += "const uint _MATERIAL_TYPE = ";
	// names defined in SharedGpuTypes.txt
	if (light_mode == LightingMode::Lit)
		actual_fs_code += "MATERIAL_TYPE_LIT;\n";
	else
		actual_fs_code += "MATERIAL_TYPE_UNLIT;\n";


	if (!fs_code.empty()) {
		autogen_code("FS",actual_fs_code, fs_code);
	}
	else
		actual_fs_code += "void FSmain() { }\n";


	replace(masterShader, "___USER_VS_CODE___", actual_vs_code);
	replace(masterShader, "___USER_FS_CODE___", actual_fs_code);


	masterShader.insert(0, 
		"// ***********************************\n"
		"// **** GENERATED MATERIAL SHADER ****\n"
		"// ***********************************\n"
	);

	return masterShader;
}
#endif

void MaterialManagerLocal::on_reload_shader_invoke()
{


}

void MaterialManagerLocal::init() {
	draw.on_reload_shaders.add(this, &MaterialManagerLocal::on_reload_shader_invoke);

	//glCreateBuffers(1, &gpuMaterialBuffer);
	//glNamedBufferStorage(gpuMaterialBuffer, MATERIAL_SIZE * MAX_MATERIALS, nullptr, GL_DYNAMIC_STORAGE_BIT);

	auto create_buffer = [&]() {
		CreateBufferArgs args;
		args.size = MATERIAL_SIZE * MAX_MATERIALS;
		args.flags = GraphicsBufferUseFlags(BUFFER_USE_DYNAMIC | BUFFER_USE_AS_STORAGE_READ);
		return IGraphicsDevice::inst->create_buffer(args);
	};
	gpuMatBufferPtr = create_buffer();

	materialBufferSize = MATERIAL_SIZE * MAX_MATERIALS;
	mat_offset_table = std::make_unique<AllMaterialTable>(MAX_MATERIALS);
	//materialBitmapAllocator.resize(MAX_MATERIALS/64	/* 64 bit bitmask */, 0);

	fallback = g_assets.find_sync_sptr<MaterialInstance>("eng/fallback.mm",true);
	if (!fallback)
		Fatalf("couldnt load the fallback master material\n");

	defaultBillboard = g_assets.find_sync_sptr<MaterialInstance>("eng/billboardDefault.mm",true);
	if (!defaultBillboard)
		Fatalf("couldnt load the default billboard material\n");

	pp_editor_select_mat = g_assets.find_sync_sptr<MaterialInstance>("eng/defaultEditorSelect.mm",true);
	if (!pp_editor_select_mat)
		Fatalf("couldnt load the default editor select material\n");
}

void MaterialManagerLocal::pre_render_update()
{
	for (auto mat : dirty_list) {
		// dynamic or static material got removed after it got added to the dirty list, skip
		if (!mat)
			continue;

		if (mat->impl->masterImpl.get())
			mat_shader_table.recompile_for_material(mat->impl->masterImpl.get());

		auto check_buffer_offset = [&]() {
			const int gpu_buffer_offset = mat->impl->gpu_buffer_offset;
			// allocate it if it doesnt exist
			if (gpu_buffer_offset == MaterialImpl::INVALID_MAPPING) {
				mat_offset_table->register_material(mat);
			}
		};
		check_buffer_offset();

		std::array<std::byte, MATERIAL_SIZE> data_to_upload = {};

		auto mm = mat->get_master_material();
		ASSERT(mm);
		auto& params = mat->impl->params;
		ASSERT(mm->param_defs.size() == mat->impl->params.size());
		for (int i = 0; i < params.size(); i++) {

			auto& param = params[i];
			auto& def = mm->param_defs[i];
			if (param.type == MatParamType::Texture2D)
				mat->impl->texture_bindings.at(def.offset) = param.tex.get();
			else {
				if (param.type == MatParamType::Float) {
					ASSERT(def.offset >= 0 && def.offset < MATERIAL_SIZE - 4);
					memcpy(data_to_upload.data()+def.offset, &param.scalar, sizeof(float));
				}
				else if (param.type == MatParamType::FloatVec) {
					ASSERT(def.offset >= 0 && def.offset < MATERIAL_SIZE - sizeof(glm::vec4));
					memcpy(data_to_upload.data()+def.offset, &param.vector, sizeof(glm::vec4));
				}
				else if (param.type == MatParamType::Vector) {
					ASSERT(def.offset >= 0 && def.offset < MATERIAL_SIZE - sizeof(Color32));
					memcpy(data_to_upload.data()+def.offset, &param.color32, sizeof(Color32));
				}
				else
					ASSERT(0);
			}
		}
		// compute hasher after texture update stuff. fixme dont recompute if didint change texture
		mat->impl->texture_id_hash = binding_hasher.get_texture_hash_id_for_material(mat->impl.get());


		const int offset = mat->impl->gpu_buffer_offset * sizeof(uint);
		// update the buffer
	//	glNamedBufferSubData(gpuMaterialBuffer, offset, data_to_upload.size(), data_to_upload.data());

		gpuMatBufferPtr->sub_upload(data_to_upload.data(), data_to_upload.size(), offset);

	}

	dirty_list.clear_all();
}

void MaterialInstance::set_float_parameter(StringName name, float f)
{
	if (auto p = impl->find_param_type(name, MatParamType::Float)) {
		p->scalar = f;
		matman.add_to_dirty_list(this);
	}
	else {
		sys_print(Error, "couldnt find parameter for set_float_parameter\n");

	}
}
void MaterialInstance::set_floatvec_parameter(StringName name, glm::vec4 f)
{
	if (auto p = impl->find_param_type(name, MatParamType::FloatVec)) {
		p->vector = f;
		matman.add_to_dirty_list(this);
	}
	else {
		sys_print(Error, "couldnt find parameter for set_floatvec_parameter\n");

	}
}

void MaterialInstance::set_u8vec_parameter(StringName name, Color32 f)
{
	if (auto p = impl->find_param_type(name, MatParamType::Vector)) {
		p->color32 = f.to_uint();
		matman.add_to_dirty_list(this);
	}
	else {
		sys_print(Error, "couldnt find parameter for set_u8vec_parameter\n");
	}
}


void MaterialInstance::set_tex_parameter(StringName name, const Texture* t)
{
	if (!t) 
		return;

	if (auto p = impl->find_param_type(name, MatParamType::Texture2D)) {
		p->tex = g_assets.find_sync_sptr<Texture>(t->get_name());
		matman.add_to_dirty_list(this);
	}
	else {
		sys_print(Error, "couldnt find parameter for set_tex_parameter\n");

	}
}

void DynamicMaterialDeleter::operator()(MaterialInstance* mat) const
{
	ASSERT(mat->impl->is_dynamic_material);
	matman.free_dynamic_material(mat);
}

AllMaterialTable::AllMaterialTable(int max_materials) : allocator(max_materials) {
	all_mats.resize(max_materials, nullptr);
}

void AllMaterialTable::register_material(MaterialInstance* mat) {
	assert(mat);
	assert(mat->impl->gpu_buffer_offset == MaterialImpl::INVALID_MAPPING);
	const int index = allocator.allocate();
	assert(!all_mats.at(index));
	all_mats.at(index) = mat;
	const int ofs = index * MATERIAL_SIZE / 4;
	mat->impl->gpu_buffer_offset = ofs;

	//assert(ofs >= 0 && ofs < materialBufferSize / 4);

}

void AllMaterialTable::unregister_material(MaterialInstance* mat) {
	const int gpu_buf_ofs = mat->impl->gpu_buffer_offset;
	assert(gpu_buf_ofs != MaterialImpl::INVALID_MAPPING);
	const int byteIndex = mat->impl->get_material_index_from_buffer_ofs();
	allocator.free(byteIndex);
	assert(all_mats.at(byteIndex) == mat);
	all_mats.at(byteIndex) = nullptr;
	mat->impl->gpu_buffer_offset = MaterialImpl::INVALID_MAPPING;
}

const std::vector<MaterialInstance*>& AllMaterialTable::get_all_mat_array() const {
	return all_mats;
}

BitmapAllocator::BitmapAllocator(int size) {
	int bitmap_sz = size / 64;
	if (size % 64 != 0)
		bitmap_sz += 1;
	materialBitmapAllocator.resize(bitmap_sz);
	this->max_ids = size;
}

int BitmapAllocator::allocate() {
	// returns INDEX, not POINTER
	for (int i = 0; i < materialBitmapAllocator.size(); i++) {
		if (materialBitmapAllocator[i] == UINT64_MAX)
			continue;
		// find bit
		for (int bit = 0; bit < 64; bit++) {
			if ((materialBitmapAllocator[i] & (1ull << bit)) == 0) {
				materialBitmapAllocator[i] |= (1ull << bit);
				return i * 64 + bit;
			}
		}
		ASSERT(0);	// should have found a bit
	}
	Fatalf("allocate_material_instance: out of memory\n");

	return  0;
}

void BitmapAllocator::free(int id) {
	assert(id >= 0 && id < max_ids);

	//int byteIndex = (m->impl->gpu_buffer_offset * 4) / MATERIAL_SIZE;
	int bitmapIndex = id / 64;
	int bitIndex = id % 64;
	materialBitmapAllocator.at(bitmapIndex) &= ~(1ull << bitIndex);
}

int TextureBindingHasher::get_texture_hash_id_for_material(MaterialImpl* mat) {
	if (mat->texture_bindings.empty())
		return NO_TEXTURE_ID;
	opt<int> existing = find_existing(mat->texture_bindings);
	if (existing.has_value())
		return *existing;
	return insert_new(mat->texture_bindings);
}

bool TextureBindingHasher::are_arrays_equal(const InlineVec<Texture*, 6>& v1, const std::vector<Texture*>& v2) {
	if (v1.size() != v2.size())
		return false;
	for (int i = 0; i < v1.size(); i++) {
		if (v1[i] != v2[i])
			return false;
	}
	return true;
}

int TextureBindingHasher::insert_new(const std::vector<Texture*> bindings) {
	assert(!bindings.empty());
	Texture* first = bindings.at(0);
	assert(first);
	const int this_id = current_texture_hash_id++;
	HashItem item;
	item.id = this_id;
	for (auto t : bindings)
		item.textures.push_back(t);
	table[first].push_back(item);	// inserts into table if doesnt exist, then push_back()
	return this_id;
}

opt<int> TextureBindingHasher::find_existing(const std::vector<Texture*> bindings) {
	if (bindings.empty())
		return NO_TEXTURE_ID;
	Texture* first = bindings.at(0);
	assert(first);
	const HashItemVec* items = MapUtil::get_opt(table, first);
	if (items) {
		for (int i = 0; i < items->size(); i++) {
			const HashItem* item = &(*items)[i];
			const bool are_equal = are_arrays_equal(item->textures, bindings);
			if (are_equal)
				return item->id;
		}
	}
	return std::nullopt;
}

MaterialInstance* DynamicMaterialAllocator::allocate_dynamic()
{
	auto it = free_dynamic_ptrs.begin();
	MaterialInstance* first = nullptr;
	if (it != free_dynamic_ptrs.end()) {
		first = *it;
		free_dynamic_ptrs.remove(first);
	}
	else {
		first = new MaterialInstance;
	}

	outstanding_dynamic_mats += 1;


	ASSERT(first);
	first->set_loaded_manually_unsafe("*dm");
	assert(first->get_is_loaded());
	return first;
}

void DynamicMaterialAllocator::free_dynamic(MaterialInstance* mat)
{
	ASSERT(mat && mat->is_this_a_dynamic_material());
	ASSERT(free_dynamic_ptrs.find(mat) == nullptr);

	outstanding_dynamic_mats -= 1;
	ASSERT(outstanding_dynamic_mats >= 0);
	mat->uninstall();
	free_dynamic_ptrs.insert(mat);
}
