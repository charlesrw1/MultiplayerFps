#pragma once
#ifdef EDITOR_BUILD

#include "Game/EntityComponent.h"

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
};

#endif
