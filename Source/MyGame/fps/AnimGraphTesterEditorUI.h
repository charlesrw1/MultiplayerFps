#pragma once
#ifdef EDITOR_BUILD

#include "Game/EntityComponent.h"

class AnimGraphTester;

// Blend-space-2D preview for AnimGraphTester's BlendSpace2D mode: a draggable marker over the
// 4-corner sample grid (see MyImDrawBlendSpace), inline in the component's inspector panel
// with a button to pop the same view out into a larger floating window.
class AnimGraphTesterEditorUi : public IComponentEditorUi
{
public:
    explicit AnimGraphTesterEditorUi(AnimGraphTester* comp) : comp(comp) {}
    bool draw() override;

private:
    void draw_blendspace_canvas();

    AnimGraphTester* comp = nullptr;
    bool show_blendspace_popup = false;
};

#endif
