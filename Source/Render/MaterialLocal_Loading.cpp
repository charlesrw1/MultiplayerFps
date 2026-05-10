#include "MaterialLocal.h"
#include "Framework/Files.h"
#include "Framework/DictParser.h"
#include "AssetCompile/Someutils.h"
#include "DrawLocal.h"
#include "Render/Texture.h"
#include "Render/Model.h"
#include "glad/glad.h"
#include "IGraphsDevice.h"
#include "Assets/AssetRegistry.h"
#include "Assets/AssetDatabase.h"
#include "Framework/StringUtils.h"
#include "imgui.h"
#include "EditorPopups.h"

#include <stdexcept>
#include <fstream>

// ---------------------------------------------------------------------------
// Editor asset metadata
// ---------------------------------------------------------------------------

#ifdef EDITOR_BUILD
class MaterialAssetMetadata : public AssetMetadata
{
public:
	MaterialAssetMetadata() {
		extensions.push_back("mi");
		extensions.push_back("mm");
	}

	virtual Color32 get_browser_color() const override { return Color32(219, 189, 68); }
	virtual std::string get_type_name() const override { return "Material"; }
	virtual const ClassTypeInfo* get_asset_class_type() const override { return &MaterialInstance::StaticType; }
	void draw_browser_menu(const string& path) const final {}
};

REGISTER_ASSETMETADATA_MACRO(MaterialAssetMetadata);
#endif

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

class MasterMaterialExcept : public std::runtime_error
{
public:
	MasterMaterialExcept(const std::string& error) : std::runtime_error(error) {}
};

inline std::string remove_filename_from_path(std::string& path) {
	auto find = path.rfind("/");
	if (find == std::string::npos) {
		return "";
	} else {
		std::string wo_filename = path.substr(0, find);
		path = path.substr(find);
		return wo_filename;
	}
}

extern ConfigVar material_print_debug;

// ---------------------------------------------------------------------------
// MasterMaterialImpl – loading
// ---------------------------------------------------------------------------

const MaterialParameterDefinition* MasterMaterialImpl::find_definition(const std::string& str, int& index) const {
	ASSERT(!str.empty());
	for (int i = 0; i < (int)param_defs.size(); i++)
		if (param_defs[i].name == str) {
			index = i;
			return &param_defs[i];
		}
	return nullptr;
}

const char* get_master_shader_path(MaterialUsage usage) {
	ASSERT(true); // usage is always valid
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

void MasterMaterialImpl::load_from_file(const std::string& fullpath, IFile* file, IAssetLoadingInterface* loading) {
	ASSERT(file);

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
		auto parse_options = [&](const std::vector<std::string>& opts) -> int {
			in.read_string(tok);
			for (int i = 0; i < (int)opts.size(); i++) {
				if (tok.cmp(opts[i].c_str()))
					return i;
			}
			throw MasterMaterialExcept("Unknown option " + to_std_string_sv(tok));
		};
		while (in.read_string(tok) && !in.is_eof()) {
			if (tok.cmp("VAR")) {
				MaterialParameterDefinition def;
				def.default_value.type = (MatParamType)parse_options(
					{"_null_", "float_vec4", "float", "vec4", "bool", "texture2D", "constTexture2D"});
				in.read_string(tok);
				def.name = to_std_string_sv(tok);
				def.hashed_name = StringName(def.name.c_str());

				switch (def.default_value.type) {
				case MatParamType::Bool: {
					int b = 0;
					in.read_int(b);
					def.default_value.boolean = b;
				} break;
				case MatParamType::Float: {
					float f = 0.0;
					in.read_float(f);
					def.default_value.scalar = f;
				} break;
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
				} break;
				case MatParamType::FloatVec: {
					glm::vec4 v;
					in.read_float(v.x);
					in.read_float(v.y);
					in.read_float(v.z);
					in.read_float(v.w);
					def.default_value.vector = v;
				} break;
				case MatParamType::Texture2D: {
					in.read_string(tok);
					def.default_value.tex = g_assets.find_sync_sptr<Texture>(to_std_string_sv(tok));
					assert(def.default_value.tex);
				} break;
				default:
					throw MasterMaterialExcept("bad VAR type");
					break;
				}

				param_defs.push_back(def);
			} else if (tok.cmp("OPT")) {

				in.read_string(tok);

				if (tok.cmp("AlphaTested")) {
					alpha_tested = parse_options({"false", "true"});
				} else if (tok.cmp("BlendMode")) {
					blend = (BlendState)parse_options({"Opaque", "Blend", "Add", "Mult", "Screen", "PreMult"});
				} else if (tok.cmp("LightingMode")) {
					light_mode = (LightingMode)parse_options({"Lit", "Unlit"});
				} else if (tok.cmp("ShowBackfaces")) {
					backface = parse_options({"false", "true"});
				} else if (tok.cmp("WriteAlbedo")) {
					decal_affect_albedo = true;
				} else if (tok.cmp("WriteNormal")) {
					decal_affect_normal = true;
				} else if (tok.cmp("WriteEmissive")) {
					decal_affect_emissive = true;
				} else if (tok.cmp("WriteRoughMetal")) {
					decal_affect_roughmetal = true;
				} else
					throw MasterMaterialExcept("Unknown OPT " + to_std_string_sv(tok));
			} else if (tok.cmp("UBO")) {
				in.read_string(tok);
				ASSERT(0);
			} else if (tok.cmp("INST")) {
				in.read_string(tok);
				InstanceData id;
				id.is_vector_type = parse_options({"float", "vec4"});

				in.read_string(tok);
				id.name = to_std_string_sv(tok);
				int index = 0;
				in.read_int(index);
				if (index < 0 || index >= MAX_INSTANCE_PARAMETERS) {
					throw MasterMaterialExcept("INST index not valid " + std::to_string(index));
				}
				inst_dats.push_back(id);
			} else if (tok.cmp("_VS_BEGIN")) {
				while (in.read_line(tok)) {
					std::string line = to_std_string_sv(tok);
					if (line.find("_VS_END") != std::string::npos) {
						break;
					}
					vs_code += line + "\n";
				}
			} else if (tok.cmp("_FS_BEGIN")) {
				while (in.read_line(tok)) {
					std::string line = to_std_string_sv(tok);
					if (line.find("_FS_END") != std::string::npos) {
						break;
					}
					fs_code += line + "\n";
				}
			} else if (tok.cmp("DOMAIN")) {
				usage = (MaterialUsage)parse_options({"Default", "PostProcess", "Terrain", "Decal", "UI", "Particle"});
			} else {
				throw MasterMaterialExcept("Unknown cmd : " + to_std_string_sv(tok));
			}
		}
	}
	catch (MasterMaterialExcept m) {
		throw MasterMaterialExcept(string_format("line:%d %s", in.get_last_line(), m.what()));
	}

	std::sort(param_defs.begin(), param_defs.end(), [](const auto& a, const auto& b) -> bool {
		return (int)a.default_value.type < (int)b.default_value.type;
	});

	uint32_t param_ofs = 0;
	uint32_t tex_ofs = 0;

	for (int i = 0; i < (int)param_defs.size(); i++) {
		auto& pd = param_defs[i];
		switch (pd.default_value.type) {
		case MatParamType::FloatVec:
			pd.offset = param_ofs;
			param_ofs += 16;
			break;
		case MatParamType::Float:
		case MatParamType::Vector:
			pd.offset = param_ofs;
			param_ofs += 4;
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
	{
		auto out_glsl_path = strip_extension(fullpath) + "_shader.glsl";
		auto outGlslFile = FileSys::open_read_game(out_glsl_path);
		auto masterFile = FileSys::open_read_engine(("Shaders\\" + std::string(get_master_shader_path(usage))).c_str());
		ASSERT(masterFile);

		if (!outGlslFile || outGlslFile->get_timestamp() < file->get_timestamp() ||
			outGlslFile->get_timestamp() < masterFile->get_timestamp()) {
			masterFile.reset();
			sys_print(Debug, "MasterMaterialImpl::load_from_file: updating .glsl because its out of date (%s)\n",
					  out_glsl_path.c_str());
			string outStr = create_glsl_shader(vs_code, fs_code, inst_dats);
			outGlslFile.reset();
			outGlslFile = FileSys::open_write_game(out_glsl_path);
			if (!outGlslFile) {
				sys_print(Error,
						  "MasterMaterialImpl::load_from_file: couldn't open file to write .glsl file (%s)\n",
						  out_glsl_path.c_str());
			} else {
				outGlslFile->write(outStr.data(), outStr.size());
			}
		}
	}
#endif
}

// ---------------------------------------------------------------------------
// MaterialImpl – loading helpers
// ---------------------------------------------------------------------------

void MaterialImpl::init_from(const std::shared_ptr<MaterialInstance>& parent) {
	ASSERT(parent && parent->impl);
	auto parent_master = parent->get_master_material();
	masterMaterial = parent;
	params.resize(parent_master->param_defs.size());
	for (int i = 0; i < (int)parent_master->param_defs.size(); i++)
		params[i] = parent->impl->params[i];
	texture_bindings.resize(parent_master->num_texture_bindings, nullptr);
}

void MaterialImpl::load_master(MaterialInstance* self, IFile* file, IAssetLoadingInterface* loading) {
	ASSERT(self && file);
	masterImpl = std::make_unique<MasterMaterialImpl>();
	masterImpl->self = self;
	masterImpl->load_from_file(self->get_name(), file, loading);

	params.resize(masterImpl->param_defs.size());
	for (int i = 0; i < (int)masterImpl->param_defs.size(); i++)
		params[i] = masterImpl->param_defs[i].default_value;
	texture_bindings.resize(masterImpl->num_texture_bindings, nullptr);
}

void MaterialImpl::load_instance(MaterialInstance* self, IFile* file, IAssetLoadingInterface* loading) {
	ASSERT(self && file);
	const auto& fullpath = self->get_name();

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
		auto parent = g_assets.find_sync_sptr<MaterialInstance>(parent_mat);
		if (!parent || !parent->impl)
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
				assert(index < (int)params.size() && index >= 0);
				auto& myparam = params[index];

				switch (ptr->default_value.type) {
				case MatParamType::Bool: {
					int b = 0;
					in.read_int(b);
					myparam.boolean = b;
				} break;
				case MatParamType::Float: {
					float f = 0.0;
					in.read_float(f);
					myparam.scalar = f;
				} break;
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
				} break;
				case MatParamType::FloatVec: {
					glm::vec4 v;
					in.read_float(v.x);
					in.read_float(v.y);
					in.read_float(v.z);
					in.read_float(v.w);
					myparam.vector = v;
				} break;
				case MatParamType::Texture2D: {
					in.read_string(tok);
					string s = to_std_string_sv(tok);
					myparam.tex = g_assets.find_sync_sptr<Texture>(s);
					if (!myparam.tex) {
						sys_print(Error, "MaterialImpl::load_instance: texture not found: %s\n", s.c_str());
						throw MasterMaterialExcept("Texture not found: " + s);
					}
					assert(myparam.tex);
				} break;
				default:
					throw MasterMaterialExcept("bad VAR type");
					break;
				}
			} else
				throw MasterMaterialExcept("can only have VAR option for materialinstances");
		}
	}
	catch (MasterMaterialExcept m) {
		throw MasterMaterialExcept(string_format("line:%d %s", in.get_last_line(), m.what()));
	}
}

bool MaterialImpl::load_from_file(MaterialInstance* self, IAssetLoadingInterface* loading) {
	ASSERT(self);
	this->self = self;
	const auto& name = self->get_name();
	try {
		auto file = FileSys::open_read_game(name.c_str());
		if (!file)
			throw MasterMaterialExcept("couldn't mm/mi open file");
		if (has_extension(name, "mm")) {
			load_master(self, file.get(), loading);
		} else {
			load_instance(self, file.get(), loading);
		}
	}
	catch (MasterMaterialExcept exppt) {
		sys_print(Error, "error loading material %s: %s\n", name.c_str(), exppt.what());
		return false;
	}
	return true;
}

void MaterialImpl::post_load(MaterialInstance* self) {
	ASSERT(!has_called_post_load_already);
	ASSERT(this->self == self);
	if (masterImpl) {
		masterImpl->material_id = matman.get_next_master_id();
		ASSERT(masterImpl->self == self);
	}
	matman.add_to_dirty_list(self);
}
