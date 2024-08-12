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

class StaticMeshEntity;
class ModelEditorTool : public IEditorTool
{
public:
	void draw_menu_bar() override;
	// Inherited via IEditorTool

	virtual void tick(float dt) override;
	virtual const View_Setup& get_vs() override;
	virtual void overlay_draw() override;
	virtual void init() override;
	virtual bool can_save_document() override;
	virtual const char* get_editor_name() override;
	virtual bool has_document_open() const override;
	virtual void open_document_internal(const char* name, const char* arg) override;
	virtual void close_internal() override;
	virtual bool save_document_internal() override;
	void imgui_draw() override;

	void on_open_map_callback(bool good);

	View_Setup view;
	User_Camera camera;

	PropertyGrid propGrid;
	StaticMeshEntity* outputEntity = nullptr;
	Model* outputModel = nullptr;
	ModelImportSettings* importSettings = nullptr;
};
