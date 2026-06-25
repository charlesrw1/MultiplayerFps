#include "PostProcessComponent.h"
#include "BillboardComponent.h"
#include "PostProcessEditorUI.h"
#include "Render/PPManager.h"
#include "Render/Texture.h"
#include "Assets/AssetDatabase.h"
#include "GameEnginePublic.h"

void PostProcessComponent::start() {
#ifdef EDITOR_BUILD
    if (eng->is_editor_level()) {
        auto b = get_owner()->create_component<BillboardComponent>();
        b->set_texture(default_asset_load<Texture>("eng/icon/_nearest/worldsettings.png"));
        b->dont_serialize_or_edit = true;
        editor_billboard = b;
    }
#endif
    if (enabled && settings.get())
        pp_handle = PPManager::inst->register_settings(priority);

    sync_render_data();
}

void PostProcessComponent::stop() {
    PPManager::inst->remove_settings(pp_handle);
}

#ifdef EDITOR_BUILD
std::unique_ptr<IComponentEditorUi> PostProcessComponent::create_editor_ui() {
    return std::make_unique<PostProcessComponentEditorUi>(this);
}
#endif

void PostProcessComponent::on_sync_render_data() {
    ASSERT(PPManager::inst);
    const bool want_active = enabled && settings.get() != nullptr;

    if (want_active && !pp_handle.is_valid())
        pp_handle = PPManager::inst->register_settings(priority);
    else if (!want_active && pp_handle.is_valid())
        PPManager::inst->remove_settings(pp_handle);

    if (pp_handle.is_valid())
        PPManager::inst->update_settings(pp_handle, settings->to_params());
}
