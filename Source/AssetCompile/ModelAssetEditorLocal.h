#pragma once
#ifdef EDITOR_BUILD
#include "IEditorTool.h"
#include "Render/DrawPublic.h"
#include "Types.h"

#include "Framework/CurveEditorImgui.h"
#include "Framework/PropertyEd.h"

#include "ModelCompilierLocal.h"
#include "Animation/Runtime/Animation.h"

#include "ModelAsset2.h"


#include "EditorTool3d.h"

class MeshComponent;
class ModelEditorTool : public EditorTool3d
{
public:
	const ClassTypeInfo& get_asset_type_info() const override {
		return Model::StaticType;
	}

	void post_map_load_callback() override;

	virtual void close_internal() override;
	virtual bool save_document_internal() override;
	void imgui_draw() override;

	const char* get_save_file_extension() const {
		return "cmdl";
	}

	PropertyGrid propGrid;
	MeshComponent* outputEntity = nullptr;
	Model* outputModel = nullptr;
	ModelImportSettings* importSettings = nullptr;
};
#endif