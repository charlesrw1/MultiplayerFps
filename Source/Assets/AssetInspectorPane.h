#pragma once
#ifdef EDITOR_BUILD
#include "Assets/AssetRegistry.h"
#include <string>
#include <memory>

// Opaque cache that owns the deserialized ClassBase object + PropertyGrid.
// Defined in AssetInspectorPane.cpp so headers don't pull in SerializerJson/PropertyEd.
struct InspectorCache;
// Opaque state for the .mi property editor.
struct MiEditorState;

class AnimSeqEditor;

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
    void draw_material_instance_editor(const std::string& gamepath);
    void draw_anim_seq_editor(const std::string& cmdl_gamepath);

    void load_for(const AssetOnDisk& selected);
    void apply_changes();

    AssetOnDisk last_selected;
    std::string raw_file_contents;
    std::string active_tis_path_; // resolved .tis path — may differ from last_selected when a .dds is selected
    int selected_mip_ = 0;
    bool settings_dirty = false;

    // Owns: MakeObjectFromPathGeneric + ReadSerializerBackendJson + PropertyGrid
    // Rebuilt only when selected asset changes.
    std::unique_ptr<InspectorCache> cache_;
    // Owns state for .mi property editor (rebuilt when selected asset changes).
    std::unique_ptr<MiEditorState> mi_state_;
    // Owns animation sequence editor (active when a .cmdl with a skeleton is selected).
    std::unique_ptr<AnimSeqEditor> anim_seq_editor_;
};

#endif
