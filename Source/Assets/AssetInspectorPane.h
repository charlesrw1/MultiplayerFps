#pragma once
#ifdef EDITOR_BUILD
#include "Assets/AssetRegistry.h"
#include <string>

class AssetInspectorPane {
public:
    AssetInspectorPane();
    ~AssetInspectorPane();

    void imgui_draw(const AssetOnDisk& selected);

private:
    void draw_tis_settings(const std::string& gamepath);
    void draw_mis_settings(const std::string& gamepath);
    void draw_material_text(const std::string& gamepath);

    void load_for(const AssetOnDisk& selected);
    void apply_changes();

    AssetOnDisk last_selected;
    std::string raw_file_contents;
    bool settings_dirty = false;
};

#endif
