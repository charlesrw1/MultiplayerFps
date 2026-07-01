#pragma once
#ifdef EDITOR_BUILD

#include "Game/EntityComponent.h"
#include "Framework/CurveEditorImgui.h"
#include "LevelEditor/PropertyEditors.h"

class PostProcessComponent;

class PostProcessComponentEditorUi : public IComponentEditorUi
{
public:
    PostProcessComponentEditorUi(PostProcessComponent* comp) : comp(comp) {}
    bool draw() override;

private:
    PostProcessComponent* comp;
    bool show_create_popup = false;
    char create_name[128] = {};

    // Bloom mip-weight curve popup (same pattern as ParticleSystemEditorUi's MinMaxCurve editor)
    CurveEditorImgui curve_editor_popup;
    EditingCurve* editing_bloom_curve = nullptr;
    bool show_curve_popup = false;

    // Bloom lens-dirt texture slot
    AssetSlotWidget lens_dirt_slot;
};

#endif
