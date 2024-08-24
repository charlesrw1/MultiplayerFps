#pragma once
#include "Particles/ParticlesLocal.h"
#include "IEditorTool.h"
#include "Framework/PropertyEd.h"
#include "Types.h"
#include "Render/DrawPublic.h"
#include "EditorTool3d.h"

class ParticleEditorTool : public EditorTool3d
{
	// Inherited via IEditorTool
	virtual void tick(float dt) override;

	virtual void init() override {}
	virtual bool can_save_document() override { return true; }

	virtual void close_internal() override;
	virtual bool save_document_internal() override;
	void imgui_draw() override;


	PropertyGrid systems;
	PropertyGrid modules;
	PropertyGrid selectedModule;
	
	ParticleFXAsset* assetEditing = nullptr;
};