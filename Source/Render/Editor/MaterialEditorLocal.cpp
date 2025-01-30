#ifdef EDITOR_BUILD
#include "MaterialEditorLocal.h"

static MaterialEditorLocal g_mateditor_local;
IEditorTool* g_mateditor = &g_mateditor_local;

#include "Assets/AssetDatabase.h"
#include <SDL2/SDL.h>
#include "OsInput.h"
#include "Framework/AddClassToFactory.h"

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
			MaterialInstance* mInstLocal = (MaterialInstance*)mLocal->dynamicMat;
			auto& param = mInstLocal->impl->params.at(index);
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
				internalEditor->post_construct_for_custom_type(&param, &pi, nullptr);
			}
			else if (param.type == MatParamType::Texture2D) {
				pi = make_asset_ptr_property(paramDef.name.c_str(), pi.offset, PROP_DEFAULT, (AssetPtr<Texture>*)(0));
				internalEditor = std::unique_ptr<IPropertyEditor>(IPropertyEditor::get_factory().createObject(pi.custom_type_str));
				internalEditor->post_construct_for_custom_type(&param, &pi, nullptr);
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




void MaterialEditorLocal::close_internal()
{
	EditorTool3d::close_internal();

	imaterials->free_dynamic_material(dynamicMat);
	dynamicMat = nullptr;
	outputEntity = nullptr;
	myPropGrid.clear_all();
	materialParamGrid.clear_all();
}
#include <fstream>
bool MaterialEditorLocal::save_document_internal()
{
	if (!dynamicMat || !dynamicMat->get_master_material())
		return false;

	std::string output;
	output += "TYPE MaterialInstance\n";
	output += "PARENT ";
	output += dynamicMat->get_master_material()->self->get_name();
	output += "\n";

	MaterialInstance* mLocal = (MaterialInstance*)dynamicMat;
	auto& paramDefs = mLocal->get_master_material()->param_defs;
	auto& params = mLocal->impl->params;
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

	auto outfile = FileSys::open_write_game(get_doc_name());
	outfile->write(output.data(), output.size());
	outfile->close();

	// kinda shit ngl
	auto ptr = GetAssets().find_sync<MaterialInstance>(get_doc_name());
	GetAssets().reload_sync(ptr);

	return true;
}
#endif