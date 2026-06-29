#pragma once
#include <string>

class Model;

// Reads/writes the .amd sidecar file that lives next to a .cmdl.
// The sidecar stores per-animation metadata (is_additive, events) that
// is not baked into the compiled .cmdl itself.
namespace AnimSidecarFile {

// Derives the .amd path from a .cmdl game path.
std::string amd_path_for_model(const std::string& cmdl_game_path);

// Merges sidecar data into the model's AnimationSeq objects.
// No-ops silently if the .amd doesn't exist.
void apply_to_model(Model* model);

// Writes all AnimationSeq events/flags from the model back to its .amd.
// Returns true on success.
bool save_from_model(Model* model);

} // namespace AnimSidecarFile
