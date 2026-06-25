#include "PostProcessSettings.h"
#include "Framework/Files.h"
#include "Framework/Util.h"
#include <json.hpp>

#ifdef EDITOR_BUILD
#include "Assets/AssetRegistry.h"

class PostProcessSettingsMetadata : public AssetMetadata {
public:
    PostProcessSettingsMetadata() { extensions.push_back("ppset"); }
    Color32 get_browser_color() const override { return {80, 200, 120}; }
    std::string get_type_name() const override { return "PostProcessSettings"; }
    const ClassTypeInfo* get_asset_class_type() const override { return &PostProcessSettings::StaticType; }
};
REGISTER_ASSETMETADATA_MACRO(PostProcessSettingsMetadata);
#endif

bool PostProcessSettings::load_asset() {
    auto file = FileSys::open_read_game(get_name());
    if (!file) {
        sys_print(Warning, "PostProcessSettings: failed to open %s\n", get_name().c_str());
        return false;
    }
    size_t sz = file->size();
    std::string text(sz, '\0');
    file->read(text.data(), sz);
    file->close();

    try {
        auto j     = nlohmann::json::parse(text);
        exposure        = j.value("exposure",        1.f);
        contrast        = j.value("contrast",        1.f);
        saturation      = j.value("saturation",      1.f);
        bloom_intensity = j.value("bloom_intensity", 0.05f);
        bloom_enabled   = j.value("bloom_enabled",   true);
        tonemap_type    = j.value("tonemap_type",    2);
    } catch (const nlohmann::json::exception& e) {
        sys_print(Warning, "PostProcessSettings: JSON error in %s: %s\n", get_name().c_str(), e.what());
        return false;
    }
    return true;
}

void PostProcessSettings::save_to_disk() {
    nlohmann::json j;
    j["exposure"]        = exposure;
    j["contrast"]        = contrast;
    j["saturation"]      = saturation;
    j["bloom_intensity"] = bloom_intensity;
    j["bloom_enabled"]   = bloom_enabled;
    j["tonemap_type"]    = tonemap_type;

    std::string text = j.dump(2);
    auto file = FileSys::open_write_game(get_name());
    if (!file) {
        sys_print(Warning, "PostProcessSettings: failed to write %s\n", get_name().c_str());
        return;
    }
    file->write(text.data(), text.size());
    file->close();
}
