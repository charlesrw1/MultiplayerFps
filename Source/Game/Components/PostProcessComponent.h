#pragma once
#include "Game/EntityComponent.h"
#include "Game/EntityPtr.h"
#include "Framework/Reflection2.h"
#include "Assets/IAsset.h"
#include "Render/PostProcessSettings.h"

class BillboardComponent;

// Pushes a PostProcessSettings asset to PPManager each frame.
// Add one to a level entity to set the global post-process look.
// Multiple components are supported; higher priority wins.
class PostProcessComponent : public Component {
public:
    CLASS_BODY(PostProcessComponent);
    void start() final;
    void stop() final;
    void on_sync_render_data() final;
#ifdef EDITOR_BUILD
    void editor_on_change_property() final { sync_render_data(); }
    const char* get_editor_outliner_icon() const final { return "eng/icon/_nearest/worldsettings.png"; }
#endif

    REF AssetPtr<PostProcessSettings> settings;
    REF bool enabled  = true;
    REF int  priority = 0;

private:
    PPManager::Handle     pp_handle;
    obj<BillboardComponent> editor_billboard;
};
