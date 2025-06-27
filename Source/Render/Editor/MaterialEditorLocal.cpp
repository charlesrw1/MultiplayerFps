#ifdef EDITOR_BUILD
#include "MaterialEditorLocal.h"

static MaterialEditorLocal g_mateditor_local;
IEditorTool* g_mateditor = &g_mateditor_local;

#include "Assets/AssetDatabase.h"
#include <SDL2/SDL.h>
#include "Framework/AddClassToFactory.h"
#include "LevelEditor/PropertyEditors.h"


MaterialEditorLocal::MaterialEditorLocal() : materialParamGrid(factory),myPropGrid(factory)
{
	PropertyFactoryUtil::register_basic(factory);
	PropertyFactoryUtil::register_mat_editor(*this, factory);
}

void MaterialEditorLocal::close_internal()
{
	EditorTool3d::close_internal();

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

	MaterialInstance* mLocal = (MaterialInstance*)dynamicMat.get();
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
	auto ptr = g_assets.find_sync<MaterialInstance>(get_doc_name());
	g_assets.reload_sync(ptr);

	return true;
}
#endif