#pragma once
#ifdef EDITOR_BUILD
#include <string>
#include <optional>

namespace AssetTemplates {
    // Create a .tis sidecar for a .png — returns the tis gamepath on success
    std::optional<std::string> create_tis_for_png(const std::string& png_gamepath);
    // Create a .mis sidecar for a .glb — returns the mis gamepath on success
    std::optional<std::string> create_mis_for_glb(const std::string& glb_gamepath);
    // Create a .mi material instance in dir with the given name, optionally specifying master .mm
    std::optional<std::string> create_mi_from_template(const std::string& dir,
        const std::string& name, const std::string& master_mm_path = "eng/fallback.mm");

    std::optional<std::string> create_mm_from_template(const std::string& dir,
        const std::string& name, const std::string& domain = "Default");

    std::optional<std::string> create_empty_map(const std::string& dir, const std::string& name);

    std::optional<std::string> create_empty_particle(const std::string& dir, const std::string& name);

    std::optional<std::string> create_empty_prefab(const std::string& dir, const std::string& name);

    // Create a .sobj file in dir holding an empty (default-constructed) instance of the given
    // ScriptableObject subclass classname.
    std::optional<std::string> create_scriptable_object(const std::string& dir, const std::string& name,
        const std::string& classname);

    // Create a .tprefab in dir containing a single MeshComponent entity referencing model_gamepath.
    std::optional<std::string> create_prefab_for_model(const std::string& dir, const std::string& name,
        const std::string& model_gamepath);

    std::optional<std::string> create_mi_from_master(const std::string& dir,
        const std::string& name, const std::string& master_mm_path);

    // Scan all .png files in the game directory and create .tis sidecars for any missing ones.
    // Returns the number of .tis files created.
    int auto_import_all_png();

    // Create a .ais sidecar for a source audio file (.wav) — returns the ais gamepath on success
    std::optional<std::string> create_ais_for_wav(const std::string& wav_gamepath);

    // Scan all source audio files in the game directory and create .ais sidecars for any missing ones.
    // Returns the number of .ais files created.
    int auto_import_all_wav();
}

#endif
