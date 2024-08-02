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

#include "glad/glad.h"

static const char* const MATERIAL_DIR = "./Data/Materials/";
MaterialManagerLocal matman;
MaterialManagerPublic* imaterials = &matman;

CLASS_IMPL(MasterMaterial);
CLASS_IMPL(MaterialInstance);
CLASS_IMPL(MaterialParameterBuffer);

#include "Assets/AssetRegistry.h"
#include "Assets/AssetLoaderRegistry.h"
class MaterialAssetMetadata : public AssetMetadata
{
public:
	// Inherited via AssetMetadata
	virtual Color32 get_browser_color()  const override { return { 219, 189, 68 }; }
	virtual std::string get_type_name()  const override { return "Material"; }
	virtual void index_assets(std::vector<std::string>& filepaths)  const override
	{
		auto tree = FileSys::find_files("./Data/Materials");
		for (auto file : tree) {
			if(has_extension(file,"mi")||has_extension(file,"mm"))
				filepaths.push_back(strip_extension(file.substr(17)));
		}
	}
	virtual bool assets_are_filepaths() const override { return false; }
	virtual std::string root_filepath()  const override { return "./Data/Materials/"; }
	virtual const ClassTypeInfo* get_asset_class_type()  const override { return &MaterialInstance::StaticType; }
};

REGISTERASSETLOADER_MACRO(MaterialInstance, &matman);
REGISTER_ASSETMETADATA_MACRO(MaterialAssetMetadata);



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
	MasterMaterialExcept(const std::string& path, const std::string& error) 
		: std::runtime_error("!!! load MasterMaterial " + path + "error: " + error + "\n") {}
};


program_handle MaterialManagerLocal::compile_mat_shader(const MasterMaterial* mat, shader_key key)
{
	// FIXME: make this faster

	std::string name = MATERIAL_DIR + mat->get_name();
//	name = strip_extension(name);
	name += "_shader.glsl";

	std::string params;
	if (key.animated) params += "ANIMATED,";
	if (key.dither) params += "DITHER,";
	if (key.editor_id) params += "EDITOR_ID,";
	if (key.depth_only) params += "DEPTH_ONLY,";
	if (key.debug) params += "DEBUG_SHADER,";
	if (!params.empty())params.pop_back();

	sys_print("*** INFO: compiling shader: %s\n", mat->get_name().c_str(), params.c_str());

	const bool is_tesselation = mat->usage == MaterialUsage::Terrain;
	program_handle handle = draw.prog_man.create_single_file(name.c_str(), is_tesselation, params);
	ASSERT(handle != -1);

	mat_table.insert(key, handle);
	return handle;
}

program_handle MaterialManagerLocal::get_mat_shader(
	bool has_animated_matricies,
	const Model* mod, 
	const MaterialInstanceLocal* mat,
	bool depth_pass,
	bool dither,
	bool is_editor_mode,
	bool debug_mode)
{
	const MasterMaterial* mm = mat->get_master_material();

	bool is_animated = mod && mod->has_bones() && has_animated_matricies;

	shader_key key;
	key.material_id = mm->material_id;
	key.animated = is_animated;
	key.depth_only = depth_pass;
	key.dither = dither;
	key.editor_id = is_editor_mode;

#ifdef _DEBUG
#else 
	//key.debug = false;
#endif
	key.debug = debug_mode;

	program_handle handle = mat_table.lookup(key);
	if (handle != -1) return handle;
	return compile_mat_shader(mm, key);	// dynamic compilation ...
}


const MaterialInstance* MaterialManagerLocal::find_material_instance(const char* mat_inst_name)
{
	std::string name = mat_inst_name;
	if (all_materials.find(name) != all_materials.end()) {
		auto& item = all_materials.find(name)->second;
		MaterialInstanceLocal* l = nullptr;
		if (item.is_master_material)
			l = (MaterialInstanceLocal*)item.mm->get_default_material_inst();
		else
			l = item.mi;
		ASSERT(l);

		if (!l->is_this_currently_uploaded()) {
			add_to_dirty_list(l);
		}
		return l;
	}
	
	// try to find it
	std::string findname = MATERIAL_DIR + name + ".mm";
	auto file = FileSys::open_read(findname.c_str());
	if (file) {
		// load the master material
		MasterMaterial* mm = new MasterMaterial;
		try {
			mm->load_from_file(findname, file.get());
		}
		catch (std::runtime_error er) {
			sys_print(er.what());
			delete mm;
			return nullptr;
		}

		mm->create_material_instance();
		mm->path = name;
		mm->is_loaded = true;
		mm->material_id = current_master_id++;
		mm->default_inst.unique_id = current_instance_id++;
		mm->default_inst.path = name;
		add_to_dirty_list(&mm->default_inst);

		MaterialItem something;
		something.is_master_material = true;
		something.mm = mm;
		all_materials.insert({ name,something });

		return mm->get_default_material_inst();
	}
	else {
		findname = MATERIAL_DIR + name + ".mi";
		file = FileSys::open_read(findname.c_str());
		if (!file) {
			// no material
			sys_print("!!! no material found\n");
			return nullptr;
		}
		// load the material instance

		// load the master material
		MaterialInstanceLocal* mi = new MaterialInstanceLocal(false/* not dynamic*/);
		try {
			mi->load_from_file(findname, file.get());
		}
		catch (std::runtime_error er) {
			sys_print(er.what());
			delete mi;
			return nullptr;
		}

		mi->path = name;
		mi->is_loaded = true;
		mi->unique_id = current_instance_id++;
		add_to_dirty_list(mi);

		MaterialItem something;
		something.is_master_material = false;
		something.mi = mi;
		all_materials.insert({ name,something });

		return mi;
	}
	ASSERT(0);
	return nullptr;
}
void MaterialInstanceLocal::init_from(MasterMaterial* parent)
{
	params.resize(parent->param_defs.size());
	for (int i = 0; i < parent->param_defs.size(); i++)
		params[i] = parent->param_defs[i].default_value;
	texture_bindings.resize(parent->num_texture_bindings, nullptr);
	master = parent;
}
void MaterialInstanceLocal::init_from(MaterialInstanceLocal* parent)
{
	auto parent_master = parent->master;

	params.resize(parent_master->param_defs.size());
	for (int i = 0; i < parent_master->param_defs.size(); i++)
		params[i] = parent->params[i];
	texture_bindings.resize(parent_master->num_texture_bindings, nullptr);
	master = parent_master;
}
bool MaterialInstanceLocal::load_from_file(const std::string& fullpath, IFile* file)
{
	DictParser in;
	in.load_from_file(file);
	StringView tok;
	if (!in.read_string(tok) || !tok.cmp("TYPE") || !in.read_string(tok) || !tok.cmp("MaterialInstance")) {
		throw MasterMaterialExcept(fullpath, "Expceted TYPE MaterialInstance");
	}
	if (!in.read_string(tok) || !tok.cmp("PARENT") || !in.read_string(tok)) {
		throw MasterMaterialExcept(fullpath, "Expceted PARENT ...");
	}
	std::string parent_mat = to_std_string_sv(tok);
	MaterialInstanceLocal* parent = (MaterialInstanceLocal*)matman.find_material_instance(parent_mat.c_str());
	if(!parent)
		throw MasterMaterialExcept(fullpath, "Couldnt find parent material" + fullpath);
	
	init_from(parent);

	while (in.read_string(tok) && !in.is_eof()) {
		if (tok.cmp("VAR")) {
			
			in.read_string(tok);
			std::string paramname = to_std_string_sv(tok);
			int index = 0;
			auto ptr = master->find_definition(paramname,index);
			if(!ptr)
				throw MasterMaterialExcept(fullpath, "Couldnt find parent parameter: " + paramname);
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
			case MatParamType::ConstTexture2D:
			{
				in.read_string(tok);
				myparam.tex_ptr = g_imgs.find_texture(to_std_string_sv(tok).c_str());
			}break;

			default:
				throw MasterMaterialExcept(fullpath, "bad VAR type");
				break;
			}
		}
		else
			throw MasterMaterialExcept(fullpath, "can only have VAR option for materialinstances");
	}
	return true;
}
bool MasterMaterial::load_from_file(const std::string& fullpath, IFile* file)
{
	DictParser in;
	
	in.load_from_file(file);
	
	std::string vs_code;
	std::string fs_code;
	std::vector<InstanceData> inst_dats;

	StringView tok;
	if (!in.read_string(tok) || !tok.cmp("TYPE") || !in.read_string(tok) || !tok.cmp("MaterialMaster")) {
		throw MasterMaterialExcept(fullpath, "Expceted TYPE MaterialMaster");
	}
	auto parse_options = [&](const std::vector<std::string>& opts)->int {
		in.read_string(tok);
		for (int i = 0; i < opts.size(); i++) {
			if (tok.cmp(opts[i].c_str()))
				return i;
		}
		throw MasterMaterialExcept(fullpath, "Unknown option " + to_std_string_sv(tok));
	};
	while (in.read_string(tok) && !in.is_eof()) {
		if (tok.cmp("VAR")) {
			MaterialParameterDefinition def;
			def.default_value.type = (MatParamType)parse_options({ "_null_","float_vec4","float","vec4","bool","texture2D","constTexture2D" });
			in.read_string(tok);
			def.name = to_std_string_sv(tok);
			
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
			case MatParamType::ConstTexture2D:
			{
				in.read_string(tok);
				def.default_value.tex_ptr = g_imgs.find_texture(to_std_string_sv(tok).c_str());
			}break;

			default:
				throw MasterMaterialExcept(fullpath, "bad VAR type");
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
				blend = (blend_state)parse_options({ "Opaque","Blend","Add"});
			}
			else if (tok.cmp("LightingMode")) {
				light_mode = (LightingMode)parse_options({ "Lit","Unlit" });
			}
			else if (tok.cmp("ShowBackfaces")) {
				backface = parse_options({ "false","true" });
			}
			else
				throw MasterMaterialExcept(fullpath, "Unknown OPT " + to_std_string_sv(tok));
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
				throw MasterMaterialExcept(fullpath, "INST index not valid " + std::to_string(index));
			}
			inst_dats.push_back(id);
		}
		else if (tok.cmp("_VS_BEGIN")) {
			while (in.read_line(tok)) {
				std::string line = to_std_string_sv(tok);
				if (line.find("_VS_END")!=std::string::npos) {
					break;
				}
				vs_code += line;
			}
		}
		else if (tok.cmp("_FS_BEGIN")) {
			while (in.read_line(tok)) {
				std::string line = to_std_string_sv(tok);
				if (line.find("_FS_END") != std::string::npos) {
					break;
				}
				fs_code += to_std_string_sv(tok);
			}
		}
		else if(tok.cmp("DOMAIN")) {
			usage = (MaterialUsage)parse_options({ "Default","PostProcess","Terrain","Decal"});
		}
		else {
			throw MasterMaterialExcept(fullpath, "Unknown cmd : " + to_std_string_sv(tok));
		}
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
		case MatParamType::ConstTexture2D:
			pd.offset = tex_ofs++;
			break;
		default:
			break;
		}
	}

	if (param_ofs > MATERIAL_SIZE) {
		throw MasterMaterialExcept(fullpath, "Too many material parameters exceeds max material size of 64 bytes");
	}

	num_texture_bindings = tex_ofs;

	auto str = create_glsl_shader(vs_code, fs_code, inst_dats);
	auto out_glsl_path = strip_extension(fullpath) + "_shader.glsl";
	std::ofstream outfile(out_glsl_path);
	outfile.write(str.data(), str.size());

}


static const char* const SHADER_PATH = "Shaders\\";
static const char* const INCLUDE_SPECIFIER = "#include";


static bool read_and_add_recursive(std::string filepath, std::string& text)
{
	std::string path(SHADER_PATH);
	path += filepath;
	std::ifstream infile(path);
	if (!infile) {
		printf("ERROR: Couldn't open path %s\n", filepath.c_str());
		return false;
	}


	std::string line;
	while (std::getline(infile, line)) {

		size_t pos = line.find(INCLUDE_SPECIFIER);
		if (pos == std::string::npos)
			text.append(line + '\n');
		else {
			size_t start_file = line.find('\"');
			if (start_file == std::string::npos) {
				printf("ERROR: include not followed with filepath\n");
				return false;
			}
			size_t end_file = line.rfind('\"');
			if (end_file == start_file) {
				printf("ERROR: include missing ending quote\n");
				return false;
			}
			read_and_add_recursive(line.substr(start_file + 1, end_file - start_file - 1), text);

		}
	}

	return true;
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

bool is_alpha_numeric(char c)
{
	return isalnum(c);
}

static void replace_variable(std::string& str, const std::string& from, const std::string& to) {
	if (from.empty())
		return;
	size_t start_pos = 0;
	while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
		// check the characters before and after
		if (start_pos > 0 && is_alpha_numeric(str.at(start_pos - 1))) {
			start_pos += from.size();
			continue;
		}
		if (start_pos + from.size() < str.size() - 1 && is_alpha_numeric(str.at(start_pos + from.size()))) {
			start_pos += from.size();
			continue;
		}
		str.replace(start_pos, from.length(), to);
		start_pos += to.length();
	}
}

std::string MasterMaterial::create_glsl_shader(
	std::string& vs_code,
	std::string& fs_code,
	const std::vector<InstanceData>& instdat
)
{
	std::string masterShader;

	const char* master_shader_path = "MasterDeferredShader.txt";
	if (usage == MaterialUsage::Terrain)
		master_shader_path = "MasterTerrainShader.txt";
	else if (usage == MaterialUsage::Decal)
		master_shader_path = "MasterDecalShader.txt";

	bool good = read_and_add_recursive(master_shader_path, masterShader);
	if (!good)
		throw MasterMaterialExcept("...", "couldnt open master shader: " + std::string( master_shader_path ));

	std::string actual_vs_code;

	auto autogen_code = [&](std::string& actual_code, std::string& inp_code) {

		actual_code += "// Texture defs\n";
		for (int i = 0; i < param_defs.size(); i++) {
			if (param_defs[i].default_value.type == MatParamType::Texture2D ||
				param_defs[i].default_value.type == MatParamType::ConstTexture2D) {
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
			if (type == MatParamType::Texture2D || type == MatParamType::ConstTexture2D)
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
		actual_code += inp_code;

	};

	if (!vs_code.empty()) {
		autogen_code(actual_vs_code, vs_code);
	}
	else
		actual_vs_code = "void VSmain() { }\n";

	std::string actual_fs_code;
	if (!fs_code.empty()) {
		autogen_code(actual_fs_code, fs_code);
	}
	else
		actual_fs_code = "void FSmain() { }\n";


	replace(masterShader, "___USER_VS_CODE___", actual_vs_code);
	replace(masterShader, "___USER_FS_CODE___", actual_fs_code);

	if (is_alphatested())
		masterShader.insert(0,
			"#define ALPHATEST\n");

	masterShader.insert(0, 
		"// ***********************************\n"
		"// **** GENERATED MATERIAL SHADER ****\n"
		"// ***********************************\n"
	);

	return masterShader;
}

void MaterialManagerLocal::on_reload_shader_invoke()
{


}


void MaterialManagerLocal::init() {
	draw.on_reload_shaders.add(this, &MaterialManagerLocal::on_reload_shader_invoke);

	glCreateBuffers(1, &gpuMaterialBuffer);
	glNamedBufferStorage(gpuMaterialBuffer, MATERIAL_SIZE * MAX_MATERIALS, nullptr, GL_DYNAMIC_STORAGE_BIT);
	materialBufferSize = MATERIAL_SIZE * MAX_MATERIALS;
	materialBitmapAllocator.resize(MAX_MATERIALS/64	/* 64 bit bitmask */, 0);

	fallback = find_material_instance("fallback");
	if (!fallback)
		Fatalf("couldnt load the fallback master material\n");
}

void MaterialManagerLocal::pre_render_update()
{
	for (auto mat : dirty_list) {
		// dynamic or static material got removed after it got added to the dirty list, skip
		if (!mat)
			continue;
		mat->dirty_buffer_index = -1;

		// allocate it if it doesnt exist
		if (mat->gpu_buffer_offset == MaterialInstanceLocal::INVALID_MAPPING) {
			mat->gpu_buffer_offset = allocate_material_instance() * MATERIAL_SIZE/4;
			ASSERT(mat->gpu_buffer_offset >= 0 && mat->gpu_buffer_offset < materialBufferSize/4);
		}

		uint8_t data_to_upload[MATERIAL_SIZE];
		memset(data_to_upload, 0, MATERIAL_SIZE);

		const MasterMaterial* mm = mat->master;
		ASSERT(mm->param_defs.size() == mat->params.size());
		for (int i = 0; i < mat->params.size(); i++) {

			auto& param = mat->params[i];
			auto& def = mm->param_defs[i];
			if (param.type == MatParamType::Texture2D || param.type == MatParamType::ConstTexture2D)
				mat->texture_bindings.at(def.offset) = param.tex_ptr;
			else {
				if (param.type == MatParamType::Float) {
					ASSERT(def.offset >= 0 && def.offset < MATERIAL_SIZE - 4);
					memcpy(data_to_upload+def.offset, &param.scalar, sizeof(float));
				}
				else if (param.type == MatParamType::FloatVec) {
					ASSERT(def.offset >= 0 && def.offset < MATERIAL_SIZE - sizeof(glm::vec4));
					memcpy(data_to_upload+def.offset, &param.vector, sizeof(glm::vec4));
				}
				else if (param.type == MatParamType::Vector) {
					ASSERT(def.offset >= 0 && def.offset < MATERIAL_SIZE - sizeof(Color32));
					memcpy(data_to_upload+def.offset, &param.color32, sizeof(Color32));
				}
				else
					ASSERT(0);
			}
		}

		// update the buffer
		glNamedBufferSubData(gpuMaterialBuffer, mat->gpu_buffer_offset * sizeof(uint), MATERIAL_SIZE, data_to_upload);

	}
	dirty_list.clear();
}