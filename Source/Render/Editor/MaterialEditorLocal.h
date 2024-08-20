#pragma once

#include "IEditorTool.h"
#include "Render/MaterialLocal.h"
#include "Framework/ClassBase.h"

#include "Render/DrawPublic.h"
#include "Types.h"
#include "Framework/PropertyEd.h"
#include "Render/MaterialLocal.h"
#include "Game/SerializePtrHelpers.h"// AssetPtr
#include "Framework/ReflectionMacros.h"
#include "Framework/MulticastDelegate.h"
#include "Game/StdEntityTypes.h"
#include "Render/Model.h"
#include "Render/Texture.h"
#include "Assets/AssetDatabase.h"
extern ConfigVar ed_default_sky_material;

class StaticMeshEntity;
class MaterialEditorLocal : public IEditorTool
{
public:
	using MyClassType = MaterialEditorLocal;	// to work with REG_ASSET_PTR macros (they expect a ClassBase which has this defined, otherwise they work fine)

	// Inherited via IEditorTool
	virtual void tick(float dt) override;
	virtual const View_Setup& get_vs() override {
		return view;
	}
	virtual void overlay_draw() override {}
	virtual void init() override {
	}
	virtual bool can_save_document() override { return true; }
	virtual const char* get_editor_name() override { return "Material Editor"; }
	virtual bool has_document_open() const override { return dynamicMat != nullptr; }
	virtual void open_document_internal(const char* name, const char* arg) override;
	virtual void close_internal() override;
	virtual bool save_document_internal() override;
	void imgui_draw() override
	{
		if (ImGui::Begin("Material Editor Window")) {
			myPropGrid.update();
			if (myPropGrid.rows_had_changes) {
				if (outputEntity)
					outputEntity->Mesh->set_model(model.get());
				if (skyEntity) {
					skyEntity->Mesh->set_material_override(skyMaterial.get());
					
					// refresh the skylight, do this better tbh
					eng->remove_entity(skylightEntity);
					skylightEntity = eng->spawn_entity_from_classtype(ClassBase::find_class("SkylightEntity"));
				}
			}
			materialParamGrid.update();
			if (materialParamGrid.rows_had_changes) {
				matman.add_to_dirty_list(dynamicMat);
			}
		}
		ImGui::End();

		IEditorTool::imgui_draw();
	}

	void draw_menu_bar() override
	{
		if (ImGui::BeginMenuBar())
		{
			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::MenuItem("New")) {
					std::string cmd = "open Material ";
					cmd += dynamicMat->get_name();

					Cmd_Manager::get()->execute(Cmd_Execute_Mode::APPEND, cmd.c_str());
				}
				if (ImGui::MenuItem("Save", "Ctrl+S")) {
					save();
				}

				ImGui::EndMenu();
			}

			ImGui::EndMenuBar();
		}
	}

	static PropertyInfoList* get_props() {
		START_PROPS(MaterialEditorLocal)
			REG_ASSET_PTR(model,PROP_DEFAULT),
			REG_ASSET_PTR(skyMaterial,PROP_DEFAULT)
		END_PROPS(MaterialEditorLocal)
	}

	void on_map_load_callback(bool good) {
		assert(good);


		auto skydomeModel = GetAssets().find_sync<Model>("skydome.cmdl");

		auto dome = eng->spawn_entity_class<StaticMeshEntity>();
		dome->Mesh->set_model(skydomeModel.get());
		dome->Mesh->set_ls_transform(glm::vec3(0), {}, glm::vec3(10000.0));
		dome->Mesh->is_skybox = true;	// FIXME
		dome->Mesh->cast_shadows = false;
		dome->Mesh->set_material_override(skyMaterial.get());
		skyEntity = dome;

		auto plane = eng->spawn_entity_class<StaticMeshEntity>();
		plane->Mesh->set_model(mods.get_default_plane_model());
		plane->set_ws_transform({}, {}, glm::vec3(20.0));

		auto sun = eng->spawn_entity_class<SunLightEntity>();
		sun->Sun->intensity = 3.0;
		sun->Sun->visible = true;
		sun->Sun->log_lin_lerp_factor = 0.8;
		sun->Sun->max_shadow_dist = 40.0;
		sun->Sun->set_ls_euler_rotation(glm::vec3(0.f, glm::radians(15.f), -glm::radians(45.f)));

		// i dont expose skylight through a header, could change that or just do this (only meant to be spawned by the level editor)
		skylightEntity = eng->spawn_entity_from_classtype(ClassBase::find_class("SkylightEntity"));

		outputEntity = eng->spawn_entity_class<StaticMeshEntity>();
		outputEntity->Mesh->set_model(model.get());
		outputEntity->set_ws_transform(glm::vec3(0, 1, 0), {}, glm::vec3(1.f));

		outputEntity->Mesh->set_material_override(dynamicMat);
	}

	AssetPtr<Model> model;
	AssetPtr<MaterialInstance> skyMaterial;

	View_Setup view;
	User_Camera camera;

	PropertyGrid myPropGrid;	// model,parent
	PropertyGrid materialParamGrid; // material params
	std::vector<PropertyInfo> propInfosForMats;	// immutable!!
	PropertyInfoList propInfoListForMats;

	Entity* skylightEntity = nullptr;
	StaticMeshEntity* outputEntity = nullptr;
	StaticMeshEntity* skyEntity = nullptr;

	// dynamic material to edit params into
	MaterialInstance* parentMat = nullptr;
	MaterialInstance* dynamicMat = nullptr;
};