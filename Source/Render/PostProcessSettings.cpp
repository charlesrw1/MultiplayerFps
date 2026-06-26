#include "PostProcessSettings.h"
#include "Framework/Files.h"
#include "Framework/Util.h"
#include "glm/vec3.hpp"
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
        exposure           = j.value("exposure",           1.f);
        contrast           = j.value("contrast",           1.f);
        saturation         = j.value("saturation",         1.f);
        bloom_intensity    = j.value("bloom_intensity",    0.05f);
        bloom_enabled      = j.value("bloom_enabled",      true);
        tonemap_type       = j.value("tonemap_type",       0);
        vignette_intensity = j.value("vignette_intensity", 0.f);
        vignette_falloff   = j.value("vignette_falloff",   1.5f);
        chromatic_ab       = j.value("chromatic_ab",       0.f);
        grain_intensity    = j.value("grain_intensity",    0.f);
        grain_size         = j.value("grain_size",         1.f);
        sharpness          = j.value("sharpness",          0.f);
        color_temp         = j.value("color_temp",         0.f);
        auto load_vec3 = [&](const char* key, glm::vec3 def) -> glm::vec3 {
            if (!j.contains(key) || !j[key].is_array()) return def;
            auto& a = j[key];
            return { a.size()>0 ? a[0].get<float>() : def.r,
                     a.size()>1 ? a[1].get<float>() : def.g,
                     a.size()>2 ? a[2].get<float>() : def.b };
        };
        lift      = load_vec3("lift",      {0.f,0.f,0.f});
        gamma_rgb = load_vec3("gamma_rgb", {1.f,1.f,1.f});
        gain      = load_vec3("gain",      {1.f,1.f,1.f});
        auto_exposure = j.value("auto_exposure", false);
        ae_method     = j.value("ae_method",     0);
        ae_min_ev     = j.value("ae_min_ev",    -3.f);
        ae_max_ev     = j.value("ae_max_ev",     3.f);
        ae_speed      = j.value("ae_speed",      1.f);
        ae_key        = j.value("ae_key",        0.18f);
    } catch (const nlohmann::json::exception& e) {
        sys_print(Warning, "PostProcessSettings: JSON error in %s: %s\n", get_name().c_str(), e.what());
        return false;
    }
    return true;
}

void PostProcessSettings::save_to_disk() {
    nlohmann::json j;
    j["exposure"]           = exposure;
    j["contrast"]           = contrast;
    j["saturation"]         = saturation;
    j["bloom_intensity"]    = bloom_intensity;
    j["bloom_enabled"]      = bloom_enabled;
    j["tonemap_type"]       = tonemap_type;
    j["vignette_intensity"] = vignette_intensity;
    j["vignette_falloff"]   = vignette_falloff;
    j["chromatic_ab"]       = chromatic_ab;
    j["grain_intensity"]    = grain_intensity;
    j["grain_size"]         = grain_size;
    j["sharpness"]          = sharpness;
    j["color_temp"]         = color_temp;
    j["auto_exposure"] = auto_exposure;
    j["ae_method"]     = ae_method;
    j["ae_min_ev"]     = ae_min_ev;
    j["ae_max_ev"]     = ae_max_ev;
    j["ae_speed"]      = ae_speed;
    j["ae_key"]        = ae_key;
    j["lift"]          = {lift.r, lift.g, lift.b};
    j["gamma_rgb"] = {gamma_rgb.r, gamma_rgb.g, gamma_rgb.b};
    j["gain"]      = {gain.r,      gain.g,      gain.b};

    std::string text = j.dump(2);
    auto file = FileSys::open_write_game(get_name());
    if (!file) {
        sys_print(Warning, "PostProcessSettings: failed to write %s\n", get_name().c_str());
        return;
    }
    file->write(text.data(), text.size());
    file->close();
}
