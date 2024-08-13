#include "MaterialEditorLocal.h"

static MaterialEditorLocal g_mateditor_local;
IEditorTool* g_mateditor = &g_mateditor_local;



class MaterialParamPropEditor : public IPropertyEditor
{
public:
	// Inherited via IPropertyEditor
	virtual bool internal_update() override
	{
		if (!has_init) {
			const int index = prop->offset;
			MaterialEditorLocal* mLocal = (MaterialEditorLocal*)instance;
			auto& paramDef = mLocal->dynamicMat->get_master_material()->param_defs.at(index);
			MaterialInstanceLocal* mInstLocal = (MaterialInstanceLocal*)mLocal->dynamicMat;
			auto& param = mInstLocal->params.at(index);
			pi.name = paramDef.name.c_str();
			pi.offset = offsetof(MaterialParameterValue, tex_ptr);
			if (param.type == MatParamType::Bool) {
				pi.type = core_type_id::Bool;
				internalEditor = std::make_unique<BooleanEditor>(&param, &pi);
			}
			else if (param.type == MatParamType::Float) {
				pi.type = core_type_id::Float;
				internalEditor = std::make_unique<FloatEditor>(&param, &pi);
			}
			else if (param.type == MatParamType::Vector) {
				pi.type = core_type_id::Int32;
				internalEditor = std::unique_ptr<IPropertyEditor>(IPropertyEditor::get_factory().createObject("ColorUint"));	// definedin editordoc.cpp
				internalEditor->post_construct_for_custom_type(&param, &pi);
			}
			else if (param.type == MatParamType::Texture2D) {
				pi = make_asset_ptr_property(paramDef.name.c_str(), pi.offset, PROP_DEFAULT, (AssetPtr<Texture>*)(0));
				internalEditor = std::unique_ptr<IPropertyEditor>(IPropertyEditor::get_factory().createObject(pi.custom_type_str));
				internalEditor->post_construct_for_custom_type(&param, &pi);
			}
			has_init = true;
		}
		if (!internalEditor)
			return false;
		return internalEditor->internal_update();
	}

	// cursed moment
	PropertyInfo pi;
	bool has_init = false;
	std::unique_ptr<IPropertyEditor> internalEditor;
};
ADDTOFACTORYMACRO_NAME(MaterialParamPropEditor, IPropertyEditor, "MaterialEditParam");


void MaterialEditorLocal::tick(float dt)
{
	auto window_sz = eng->get_game_viewport_size();
	float aratio = (float)window_sz.y / window_sz.x;
	{
		int x = 0, y = 0;
		if (eng->is_game_focused()) {
			SDL_GetRelativeMouseState(&x, &y);
			camera.update_from_input(eng->get_input_state()->keys, x, y, glm::mat4(1.f));
		}
	}
	view = View_Setup(camera.position, camera.front, glm::radians(70.f), 0.01, 100.0, window_sz.x, window_sz.y);
}

void MaterialEditorLocal::open_document_internal(const char* name, const char* arg)
{
	Cmd_Manager::get()->execute(Cmd_Execute_Mode::NOW, "load_imgui_ini MaterialEditor.ini");

	assert(!dynamicMat);
	assert(!outputEntity);

	const MaterialInstance* mat = imaterials->find_material_instance(name);
	if (!mat) {
		sys_print("!!! couldnt open material %s\n", name);
		Cmd_Manager::get()->execute(Cmd_Execute_Mode::NOW, "close_ed");
		return;
	}
	parentMat = (MaterialInstanceLocal*)mat;
	if (parentMat->is_a_default_inst)
		set_empty_name();
	else
		set_doc_name(name);
	
	dynamicMat = imaterials->create_dynmaic_material(mat);
	assert(dynamicMat);
	eng->open_level("__empty__");
	eng_local.on_map_load_return.add(this, &MaterialEditorLocal::on_map_load_callback);

	model.ptr = mods.find_or_load("skydome.cmdl");
	skyMaterial.ptr = (MaterialInstance*)imaterials->find_material_instance(ed_default_sky_material.get_string());

	// context params (model changes)
	myPropGrid.add_property_list_to_grid(get_props(), this);
	
	// material params
	propInfosForMats.clear();
	MaterialInstanceLocal* mLocal = (MaterialInstanceLocal*)dynamicMat;
	auto& paramDefs = mLocal->get_master_material()->param_defs;
	for (int i = 0; i < paramDefs.size(); i++) {
		auto& def = paramDefs[i];
		auto type = def.default_value.type;
		if (type == MatParamType::Bool || type == MatParamType::Float || type == MatParamType::Texture2D || type == MatParamType::Vector)
		{
			PropertyInfo pi;
			pi.offset = i;
			pi.name = def.name.c_str();
			pi.custom_type_str = "MaterialEditParam";
			pi.flags = PROP_DEFAULT;
			pi.type = core_type_id::Struct;
			propInfosForMats.push_back(pi);
		}
	}
	propInfoListForMats.count = propInfosForMats.size();
	propInfoListForMats.list = propInfosForMats.data();
	propInfoListForMats.type_name = dynamicMat->get_master_material()->get_name().c_str();
	materialParamGrid.add_property_list_to_grid(&propInfoListForMats, this);
}

void MaterialEditorLocal::close_internal()
{
	eng->leave_level();
	eng_local.on_map_load_return.remove(this);
	imaterials->free_dynamic_material(dynamicMat);
	dynamicMat = nullptr;
	outputEntity = nullptr;
	myPropGrid.clear_all();
	materialParamGrid.clear_all();
}
#include <fstream>
bool MaterialEditorLocal::save_document_internal()
{
	std::string output;
	output += "TYPE MaterialInstance\n";
	output += "PARENT ";
	output += dynamicMat->get_master_material()->get_name();
	output += "\n";

	MaterialInstanceLocal* mLocal = (MaterialInstanceLocal*)dynamicMat;
	auto& paramDefs = mLocal->get_master_material()->param_defs;
	auto& params = mLocal->params;
	assert(params.size() == paramDefs.size());
	for (int i = 0; i < params.size(); i++) {
		auto& def = paramDefs[i];
		auto type = def.default_value.type;
		if (type == MatParamType::Bool)
		{
			output += "VAR ";
			output += def.name;
			output += " ";
			output += (int)params.at(i).boolean ? "True" : "False";
			output += "\n";
		}
		else if (type == MatParamType::Texture2D && params.at(i).tex_ptr)
		{
			output += "VAR ";
			output += def.name;
			output += " ";
			output += params.at(i).tex_ptr->get_name();
			output += "\n";
		}
		else if (type == MatParamType::Float)
		{
			output += "VAR ";
			output += def.name;
			output += " ";
			output += std::to_string(params.at(i).scalar);
			output += "\n";
		}
		else if (type == MatParamType::Vector)
		{
			auto& p = params.at(i);
			Color32 c32 = *(Color32*)&p.color32;
			output += "VAR ";
			output += def.name;
			output += " ";
			output += std::to_string(c32.r);
			output += " ";
			output += std::to_string(c32.g);
			output += " ";
			output += std::to_string(c32.b);
			output += " ";
			output += std::to_string(c32.a);
			output += "\n";
		}
	}

	std::string path = "./Data/Materials/" + (std::string)get_name() +".mi";
	std::ofstream outfile(path);
	outfile.write(output.data(), output.size());
	outfile.close();

	matman.reload_material(get_name());

	return true;
}
