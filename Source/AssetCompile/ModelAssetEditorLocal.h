#pragma once
#include "IEditorTool.h"
#include "Render/DrawPublic.h"
#include "Types.h"

#include "Framework/CurveEditorImgui.h"
#include "Framework/PropertyEd.h"

#include "ModelCompilierLocal.h"
#include "Animation/Runtime/Animation.h"

#include "ModelAsset2.h"

#include "Game/StdEntityTypes.h"
#include "EditorTool3d.h"

class StaticMeshEntity;
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


	PropertyGrid propGrid;
	StaticMeshEntity* outputEntity = nullptr;
	Model* outputModel = nullptr;
	ModelImportSettings* importSettings = nullptr;
};
