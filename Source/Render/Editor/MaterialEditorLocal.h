#pragma once
#ifdef EDITOR_BUILD
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

#include "EditorTool3d.h"
#include "Framework/Config.h"
#include "GameEnginePublic.h"
#include "Level.h"

#include "Game/AssetPtrMacro.h"

#include <string>

extern ConfigVar ed_default_sky_material;

class StaticMeshEntity;
class MaterialEditorLocal : public EditorTool3d
{
public:
	using MyClassType = MaterialEditorLocal;	// to work with REG_ASSET_PTR macros (they expect a ClassBase which has this defined, otherwise they work fine)

	virtual void init() override {
	}
	const char* get_save_file_extension() const {
		return "mi";
	}
	const ClassTypeInfo& get_asset_type_info() const override { return MaterialInstance::StaticType; }
	virtual void close_internal() override;
	virtual bool save_document_internal() override;
	void imgui_draw() override
	{
		if (ImGui::Begin("Material Editor Window")) {
			myPropGrid.update();
			if (myPropGrid.rows_had_changes) {
				if (outputEntity)
					outputEntity->Mesh->set_model(model.get());
				//if (skyEntity) {
				//	skyEntity->Mesh->set_material_override(skyMaterial.get());
				//	
				//	// refresh the skylight, do this better tbh
				//	eng->remove_entity(skylightEntity);
				//	skylightEntity = eng->spawn_entity_from_classtype(ClassBase::find_class("SkylightEntity"));
				//}
			}
			materialParamGrid.update();
			if (materialParamGrid.rows_had_changes) {
				matman.add_to_dirty_list(dynamicMat.get());
			}
		}
		ImGui::End();

		IEditorTool::imgui_draw();

		outputEntity->Mesh->sync_render_data();
	}
#if 0
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
#endif

	static PropertyInfoList* get_props() {
		START_PROPS(MaterialEditorLocal)
			REG_ASSET_PTR(model,PROP_DEFAULT),
		END_PROPS(MaterialEditorLocal)
	}

	void post_map_load_callback() override {
		Cmd_Manager::get()->execute(Cmd_Execute_Mode::NOW, "load_imgui_ini MaterialEditor.ini");

		assert(!dynamicMat);
		assert(!outputEntity);

		auto skydomeModel = g_assets.find_sync<Model>("eng/skydome.cmdl");
		auto skyMat = g_assets.find_global_sync<MaterialInstance>(ed_default_sky_material.get_string());
		model.ptr = skydomeModel.get();


		auto mat = g_assets.find_sync<MaterialInstance>(get_doc_name());
		if (!mat) {
			sys_print(Error, "couldnt open material %s\n", get_doc_name().c_str());
			Cmd_Manager::get()->execute(Cmd_Execute_Mode::APPEND, "close_ed");
			return;
		}

		parentMat = (MaterialInstance*)mat.get();
		if (parentMat->impl->masterImpl)
			set_empty_doc();
		dynamicMat = imaterials->create_dynmaic_material(mat.get());
		assert(dynamicMat);


		// context params (model changes)
		myPropGrid.add_property_list_to_grid(get_props(), this);

		// material params
		propInfosForMats.clear();
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
				propInfosForMats.push_back(pi);
			}
		}
		propInfoListForMats.count = propInfosForMats.size();
		propInfoListForMats.list = propInfosForMats.data();
		propInfoListForMats.type_name = dynamicMat->get_master_material()->self->get_name().c_str();
		materialParamGrid.add_property_list_to_grid(&propInfoListForMats, this);

		outputEntity = eng->get_level()->spawn_entity_class<StaticMeshEntity>();
		outputEntity->Mesh->set_model(model.get());
		outputEntity->set_ws_transform(glm::vec3(0, 1, 0), {}, glm::vec3(1.f));
		outputEntity->Mesh->set_material_override(dynamicMat.get());
	}

	AssetPtr<Model> model;

	PropertyGrid myPropGrid;	// model,parent
	PropertyGrid materialParamGrid; // material params
	std::vector<PropertyInfo> propInfosForMats;	// immutable!!
	PropertyInfoList propInfoListForMats;

	StaticMeshEntity* outputEntity = nullptr;

	// dynamic material to edit params into
	MaterialInstance* parentMat = nullptr;
	DynamicMatUniquePtr dynamicMat = nullptr;
};
#endif