#include "CustomComponentEditor.h"
#include "Game/Components/LightComponents.h"
#include "Framework/MeshBuilder.h"
#include "Render/RenderObj.h"
#include "Render/DrawPublic.h"
class AllLightEditor : public ICustomComponentEditor
{
public:
	~AllLightEditor() override {
		idraw->get_scene()->remove_meshbuilder(handle);
	}

	bool init(EntityComponent* e) override {
	

		//spotlight = e->cast_to<SpotLightComponent>();
		//assert(spotlight);

		MeshBuilder_Object mbo;
		mbo.visible = true;
		mbo.meshbuilder = &mb;
		handle = idraw->get_scene()->register_meshbuilder(mbo);

		return true;
	}
	bool tick() override {
		return true;
	}


	handle<MeshBuilder_Object> handle;

	MeshBuilder mb;

	SpotLightComponent* spotlight = nullptr;
};

REGISTER_COMPONENTEDITOR_MACRO(AllLightEditor, SpotLightComponent);
REGISTER_COMPONENTEDITOR_MACRO(AllLightEditor, PointLightComponent);
REGISTER_COMPONENTEDITOR_MACRO(AllLightEditor, SunLightComponent);