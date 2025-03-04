#ifdef EDITOR_BUILD
#include "AssetCompile/ModelAssetEditorLocal.h"
#include <stdexcept>
#include <SDL2/SDL.h>

#include "Someutils.h"
#include "Framework/MyImguiLib.h"
#include "Framework/Files.h"
#include "Framework/DictParser.h"
#include "Animation/AnimationUtil.h"

#include "GameEnginePublic.h"
#include "OsInput.h"

#include "Render/RenderObj.h"

#include "Framework/ObjectSerialization.h"

#include "Framework/DictWriter.h"
#include <fstream>

#include "Framework/MulticastDelegate.h"

#include "Level.h"

static ModelEditorTool g_model_editor_static;
IEditorTool* g_model_editor = &g_model_editor_static;



void ModelEditorTool::imgui_draw()
{
	if (ImGui::Begin("Main properties")) {

		if (!outputModel) {
			ImGui::TextColored(color32_to_imvec4({ 150,150,150 }), "No compilied model");
		}
		propGrid.update();

		if (propGrid.rows_had_changes&&outputModel) {
			bool needsRefresh = false;
			for (int i = 0; i < outputModel->materials.size() && i < importSettings->myMaterials.size(); i++) {
				auto& m0 = outputModel->materials[i];
				auto& m1 = importSettings->myMaterials[i];
				if (m0 != m1.get()) {
					m0 = m1.get();
					needsRefresh = true;
				}
			}
		}
	}
	ImGui::End();
}

#include "Assets/AssetDatabase.h"
extern ConfigVar ed_default_sky_material;

void ModelEditorTool::post_map_load_callback()
{
	Cmd_Manager::get()->execute(Cmd_Execute_Mode::NOW, "load_imgui_ini ModelEditorImgui.ini");


	assert(!importSettings);
	assert(!outputEntity);
	assert(!outputModel);

	if (!get_doc_name().empty()) {
		// try to find def_name
		std::string def_name = strip_extension(get_doc_name()) + ".mis";
		auto file = FileSys::open_read_game(def_name);

		if (!file) {
			sys_print(Error, "ModelEditor: couldnt find path %s\n", def_name.c_str());
		}
		else {
			DictParser dp;
			dp.load_from_file(file.get());
			StringView tok;
			dp.read_string(tok);

			importSettings = read_object_properties<ModelImportSettings>(nullptr, dp, tok);
		}
	}
	if (!importSettings) {
		set_empty_doc();
		importSettings = new ModelImportSettings;
	}

	assert(importSettings);

	auto ti = &importSettings->get_type();
	while (ti) {
		if (ti->props)
			propGrid.add_property_list_to_grid(ti->props, importSettings);
		ti = ti->super_typeinfo;
	}


	outputEntity = eng->get_level()->spawn_entity()->create_component<MeshComponent>();


	g_assets.find_async<Model>(get_doc_name(), [&](GenericAssetPtr ptr) {
		if (!importSettings || !outputEntity)
			return;
		if (!ptr)
		{
			sys_print(Warning,"no output model\n");
			return;
		}

		assert(outputEntity);

		outputModel = ptr.cast_to<Model>().get();
		if (!outputModel)
			sys_print(Warning,"compilied model didnt load but loading .def didnt error, continuing as normal\n");
		outputEntity->set_model(outputModel);
		if (outputModel) {
			importSettings->myMaterials.clear();
			for (auto mat : outputModel->materials) {
				importSettings->myMaterials.push_back({ (MaterialInstance*)mat });
			}
		}


		});;	// find the compilied model, this could be an error and loading still 'works'

}


void ModelEditorTool::close_internal()
{
	EditorTool3d::close_internal();

	outputEntity = nullptr;	// gets cleaned up in the level
	outputModel = nullptr;
	delete importSettings;
	importSettings = nullptr;

	propGrid.clear_all();
}
#include "Compiliers.h"
bool ModelEditorTool::save_document_internal()
{
	if (!importSettings)
		return false;

	ASSERT(importSettings);

	DictWriter write;
	write_object_properties(importSettings, nullptr, write);

	std::string path = strip_extension(get_doc_name()) + ".mis";
	auto outfile = FileSys::open_write_game(path);
	outfile->write(write.get_output().data(), write.get_output().size());
	outfile->close();

	ModelCompilier::compile_from_settings(path.c_str(), importSettings);

	if (!outputModel)
		outputModel = default_asset_load<Model>(get_doc_name());
	else
		g_assets.reload_sync(outputModel);

	return true;
}
#endif