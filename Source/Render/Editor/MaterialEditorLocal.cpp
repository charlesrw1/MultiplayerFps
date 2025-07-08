#ifdef EDITOR_BUILD
#include "MaterialEditorLocal.h"

//static MaterialEditorLocal g_mateditor_local;
//IEditorTool* g_mateditor = &g_mateditor_local;

#include "Assets/AssetDatabase.h"
#include <SDL2/SDL.h>
#include "Framework/AddClassToFactory.h"
#include "LevelEditor/PropertyEditors.h"


MaterialEditorLocal::MaterialEditorLocal(opt<string> assetName) : myPropGrid(factory)
{
	PropertyFactoryUtil::register_basic(factory);
	PropertyFactoryUtil::register_mat_editor(*this, factory);
}
MaterialEditorLocal::~MaterialEditorLocal() {

}

void MaterialEditorLocal::close_internal()
{
	EditorTool3d::close_internal();

	dynamicMat = nullptr;
	outputEntity = nullptr;
	myPropGrid.clear_all();
}
#include "EditorPopupTemplate.h"
#include <fstream>
bool MaterialEditorLocal::save_document_internal()
{
	if (!dynamicMat || !dynamicMat->get_master_material())
		return false;
	if (assetName.has_value() && assetName.value().empty()) {
		sys_print(Warning, "EditorDoc::save_document_internal has an empty name?\n");
		assetName = std::nullopt;
	}
	if (!assetName.has_value()) {
		PopupTemplate::create_file_save_as(EditorPopupManager::inst, [&](string path) {
			sys_print(Debug, "EditorDoc::save_document_internal: popup returned with path %s\n", path.c_str());
			this->set_document_path(path);
			save_document_internal();
			}, get_save_file_extension());
		sys_print(Debug, "EditorDoc::save_document_internal: no path to save, so adding popup\n");
		return false;
	}

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

	auto outfile = FileSys::open_write_game(assetName.value());
	outfile->write(output.data(), output.size());
	outfile->close();

	MaterialInstance* ptr = MaterialInstance::load(assetName.value());
	if(ptr)
		g_assets.reload_sync(ptr);
	else {
		sys_print(Warning, "MaterialEditorLocal::save_document_internal: couldnt load the material just saved?\n");
	}

	return true;
}
inline void MaterialEditorLocal::imgui_draw()
{
	if (ImGui::Begin("Material Editor Window")) {
		myPropGrid.update();
		if (myPropGrid.rows_had_changes) {
			//if (outputEntity)
			//	outputEntity->set_model(model.get());
			//if (skyEntity) {
			//	skyEntity->Mesh->set_material_override(skyMaterial.get());
			//	
			//	// refresh the skylight, do this better tbh
			//	eng->remove_entity(skylightEntity);
			//	skylightEntity = eng->spawn_entity_from_classtype(ClassBase::find_class("SkylightEntity"));
			//}
		}
		//materialParamGrid.update();
		//if (materialParamGrid.rows_had_changes) {
		//	matman.add_to_dirty_list(dynamicMat.get());
		//}
	}
	ImGui::End();

	IEditorTool::imgui_draw();
}
void MaterialEditorLocal::post_map_load_callback() {
	Cmd_Manager::get()->execute(Cmd_Execute_Mode::NOW, "load_imgui_ini MaterialEditor.ini");

	assert(!dynamicMat);
	assert(!outputEntity);

	auto skydomeModel = g_assets.find_sync<Model>("eng/skydome.cmdl");
	auto skyMat = g_assets.find_global_sync<MaterialInstance>(ed_default_sky_material.get_string());
	settings.outputModel = skydomeModel.get();


	auto mat = g_assets.find_sync<MaterialInstance>(get_doc_name());
	if (!mat) {
		sys_print(Error, "couldnt open material %s\n", get_doc_name().c_str());
		Cmd_Manager::get()->execute(Cmd_Execute_Mode::APPEND, "close_ed");
		return;
	}

	settings.parent = (MaterialInstance*)mat.get();
	if (settings.parent->is_this_a_master_material())
		set_empty_doc();
	dynamicMat = imaterials->create_dynmaic_material(mat.get());
	assert(dynamicMat);


	// context params (model changes)
	//myPropGrid.add_property_list_to_grid(get_props(), this);

	// material params
	//propInfosForMats.clear();
	MaterialInstance* mLocal = (MaterialInstance*)dynamicMat.get();
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
			//propInfosForMats.push_back(pi);
		}
	}
	//propInfoListForMats.count = propInfosForMats.size();
	//propInfoListForMats.list = propInfosForMats.data();
	//propInfoListForMats.type_name = dynamicMat->get_master_material()->self->get_name().c_str();
	//materialParamGrid.add_property_list_to_grid(&propInfoListForMats, this);

	outputEntity = eng->get_level()->spawn_entity()->create_component<MeshComponent>();
	//outputEntity->set_model(model.get());
	outputEntity->get_owner()->set_ws_transform(glm::vec3(0, 1, 0), {}, glm::vec3(1.f));
	outputEntity->set_material_override(dynamicMat.get());
}
#endif

void OpenMaterialEditor::execute(Callback callback) {
	OpenMapCommand* cmd = new OpenMapCommand(std::nullopt, false);
	opt<string> myAsset = this->assetName;
	cmd->callback = [callback, myAsset](OpenMapReturnCode ret) {
		assert(ret == OpenMapReturnCode::Success);
		callback(uptr<IEditorTool>(new MaterialEditorLocal(myAsset)));
	};
	Cmd_Manager::inst->append_cmd(uptr<SystemCommand>(cmd));
}
