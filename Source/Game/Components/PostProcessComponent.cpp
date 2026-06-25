#include "PostProcessComponent.h"
#include "Render/PPManager.h"

void PostProcessComponent::start() {
    if (enabled && settings.get())
        pp_handle = PPManager::inst->register_settings(priority);
}

void PostProcessComponent::stop() {
    PPManager::inst->remove_settings(pp_handle);
}

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
