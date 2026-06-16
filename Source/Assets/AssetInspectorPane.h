#pragma once
#ifdef EDITOR_BUILD
#include "Assets/AssetRegistry.h"
#include <string>
#include <memory>

// Opaque cache that owns the deserialized ClassBase object + PropertyGrid.
// Defined in AssetInspectorPane.cpp so headers don't pull in SerializerJson/PropertyEd.
struct InspectorCache;

// Separate dockable ImGui window showing per-asset settings.
// Call imgui_draw() every frame; it opens its own Begin/End.
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

    // Owns: MakeObjectFromPathGeneric + ReadSerializerBackendJson + PropertyGrid
    // Rebuilt only when selected asset changes.
    std::unique_ptr<InspectorCache> cache_;
};

#endif
