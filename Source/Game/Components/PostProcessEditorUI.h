#pragma once
#ifdef EDITOR_BUILD

#include "Game/EntityComponent.h"
#include "Framework/CurveEditorImgui.h"
#include <memory>

class PostProcessComponent;
class PostProcessSettings;
class PropertyGrid;

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

    // Bloom lens-dirt texture slot: synthetic-AssetPtr SharedAssetPropertyEditor in a
    // single-row PropertyGrid (same pattern as RendererMaterialEditor / MiTextureEditor),
    // rebuilt when the underlying settings asset changes.
    std::unique_ptr<PropertyGrid> lens_dirt_pg;
    PostProcessSettings* lens_dirt_pg_for = nullptr;
};

#endif
