#pragma once
#include "Particles/ParticlesLocal.h"
#include "IEditorTool.h"
#include "Framework/PropertyEd.h"
#include "Types.h"
#include "Render/DrawPublic.h"
class ParticleEditorTool : public IEditorTool
{
	// Inherited via IEditorTool
	virtual void tick(float dt) override;
	virtual const View_Setup& get_vs() override { return view; }
	virtual void overlay_draw() override {}
	virtual void init() override {}
	virtual bool can_save_document() override { return true; }
	virtual const char* get_editor_name() override { return "Particle Editor"; }
	virtual bool has_document_open() const override;
	virtual void open_document_internal(const char* name, const char* arg) override;
	virtual void close_internal() override;
	virtual bool save_document_internal() override;
	void imgui_draw() override;

	View_Setup view;
	User_Camera camera;

	PropertyGrid systems;
	PropertyGrid modules;
	PropertyGrid selectedModule;
	
	ParticleFXAsset* assetEditing = nullptr;
};