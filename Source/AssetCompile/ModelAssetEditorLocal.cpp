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


static ModelEditorTool g_model_editor_static;
IEditorTool* g_model_editor = &g_model_editor_static;


void ModelEditorTool::tick(float dt)
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

	IEditorTool::imgui_draw();
}

void ModelEditorTool::draw_menu_bar()
{
	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("New")) {
				Cmd_Manager::get()->execute(Cmd_Execute_Mode::APPEND,"open Model \"\"");
			}
			if (ImGui::MenuItem("Open", "Ctrl+O")) {
				open_the_open_popup();

			}
			if (ImGui::MenuItem("Save", "Ctrl+S")) {
				save();
			}

			ImGui::EndMenu();
		}
		
		ImGui::EndMenuBar();
	}
}
const View_Setup& ModelEditorTool::get_vs()
{
	// TODO: insert return statement here
	return view;
}

void ModelEditorTool::overlay_draw()
{
}


void ModelEditorTool::init()
{
}

bool ModelEditorTool::can_save_document()
{
	return true;
}

const char* ModelEditorTool::get_editor_name()
{
	return "Model Editor";
}

bool ModelEditorTool::has_document_open() const
{
	return importSettings != nullptr;
}
#include "Assets/AssetDatabase.h"
extern ConfigVar ed_default_sky_material;

void ModelEditorTool::on_open_map_callback(bool good)
{
	assert(good);

	outputEntity = eng->spawn_entity_class<StaticMeshEntity>();

	auto dome = eng->spawn_entity_class<StaticMeshEntity>();
	dome->Mesh->set_model(default_asset_load<Model>("skydome.cmdl"));
	dome->Mesh->set_ls_transform(glm::vec3(0), {}, glm::vec3(10000.0));
	dome->Mesh->is_skybox = true;	// FIXME
	dome->Mesh->cast_shadows = false;
	dome->Mesh->set_material_override(default_asset_load<MaterialInstance>(ed_default_sky_material.get_string()));

	// i dont expose skylight through a header, could change that or just do this (only meant to be spawned by the level editor)
	auto skylight = eng->spawn_entity_from_classtype(ClassBase::find_class("SkylightEntity"));

	GetAssets().find_async<Model>(get_name(), [&](GenericAssetPtr ptr) {
		if (!importSettings || !outputEntity)
			return;

		assert(outputEntity);

		outputModel = ptr.cast_to<Model>().get();
		if (!outputModel)
			sys_print("*** compilied model didnt load but loading .def didnt error, continuing as normal\n");
		outputEntity->Mesh->set_model(outputModel);
		if (outputModel) {
			importSettings->myMaterials.clear();
			for (auto mat : outputModel->materials) {
				importSettings->myMaterials.push_back({ (MaterialInstance*)mat });
			}
		}


		});;	// find the compilied model, this could be an error and loading still 'works'


}

void ModelEditorTool::open_document_internal(const char* name, const char* arg)
{
	Cmd_Manager::get()->execute(Cmd_Execute_Mode::NOW, "load_imgui_ini ModelEditorImgui.ini");


	assert(!importSettings);
	assert(!outputEntity);
	assert(!outputModel);

	if (strlen(name) > 0) {
		// try to find def_name
		std::string def_name = strip_extension(name) + ".mis";
		std::string fullpath = "./Data/Models/" + def_name;
		auto file = FileSys::open_read_os(fullpath.c_str());

		if (!file) {
			sys_print("!!! ModelEditor: couldnt find path %s\n", fullpath.c_str());
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
		set_empty_name();
		importSettings = new ModelImportSettings;
	}
	else {
		set_doc_name(name);
	}


	eng->open_level("__empty__");
	eng->get_on_map_delegate().add(this, &ModelEditorTool::on_open_map_callback);
	assert(importSettings);

	auto ti = &importSettings->get_type();
	while (ti) {
		if (ti->props)
			propGrid.add_property_list_to_grid(ti->props, importSettings);
		ti = ti->super_typeinfo;
	}
}

void ModelEditorTool::close_internal()
{
	outputEntity = nullptr;	// gets cleaned up in the level
	outputModel = nullptr;
	delete importSettings;
	importSettings = nullptr;

	eng->leave_level();

	eng->get_on_map_delegate().remove(this);

	propGrid.clear_all();
}
#include "Compiliers.h"
bool ModelEditorTool::save_document_internal()
{
	ASSERT(importSettings);

	DictWriter write;
	write_object_properties(importSettings, nullptr, write);

	std::string path = "./Data/Models/" + strip_extension(get_name()) + ".mis";
	std::ofstream outfile(path);
	outfile.write(write.get_output().data(), write.get_output().size());
	outfile.close();

	ModelCompilier::compile_from_settings(path.c_str(), importSettings);

	if (!outputModel)
		outputModel = default_asset_load<Model>(get_name());
	else
		GetAssets().reload_sync(outputModel);

	return true;
}
