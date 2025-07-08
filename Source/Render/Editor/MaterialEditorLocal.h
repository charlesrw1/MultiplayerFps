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
#include "Render/Model.h"
#include "Render/Texture.h"
#include "Assets/AssetDatabase.h"
#include "EditorTool3d.h"
#include "Framework/Config.h"
#include "GameEnginePublic.h"
#include "Level.h"
#include "Game/Components/MeshComponent.h"
#include "Game/Entity.h"
#include <string>
#include "Framework/FnFactory.h"
#include "EngineSystemCommands.h"
extern ConfigVar ed_default_sky_material;

class OpenMaterialEditor : public CreateEditorAsync {
public:
	OpenMaterialEditor(opt<string> assetName) : assetName(assetName) {}
	void execute(Callback callback) final;
	string get_tab_name() final {
		return assetName.value_or("<empty>");
	}
	opt<string> get_asset_name() final {
		return assetName;
	}
	opt<string> assetName;
};

class MaterialEdSettings : public ClassBase {
public:
	CLASS_BODY(MaterialEdSettings);
	REF Model* outputModel = nullptr;
	REF MaterialInstance* parent = nullptr;
};

class StaticMeshEntity;
class MaterialEditorLocal : public EditorTool3d
{
public:
	using MyClassType = MaterialEditorLocal;	// to work with REG_ASSET_PTR macros (they expect a ClassBase which has this defined, otherwise they work fine)
	MaterialEditorLocal(opt<string> assetName);
	~MaterialEditorLocal();
	virtual void init() { }
	uptr<CreateEditorAsync> create_command_to_load_back() final { return nullptr; }
	const char* get_save_file_extension() const { return "mi"; }
	const ClassTypeInfo& get_asset_type_info() const override { return MaterialInstance::StaticType; }
	virtual void close_internal();
	virtual bool save_document_internal() override;
	void imgui_draw() override;
	void post_map_load_callback() override;
	void set_document_path(const std::string& s) {
		assetName = s;
	}
	MaterialEdSettings settings;

	FnFactory<IPropertyEditor> factory;
	PropertyGrid myPropGrid;	// settings

	// dynamic material to edit params into
	DynamicMatUniquePtr dynamicMat = nullptr;
	MeshComponent* outputEntity = nullptr;
	opt<string> assetName;
};
#endif