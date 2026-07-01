#pragma once
#ifdef EDITOR_BUILD
#include <memory>
#include <string>
#include "Framework/MeshBuilder.h"
#include "Framework/Handle.h"

class Model;
struct MeshBuilder_Object;

// Skeleton section rendered inside the model asset inspector (.cmdl/.mis).
// Shows the bone hierarchy as a collapsible tree; selecting a bone debug-draws
// it (no depth test) on a scene MeshComponent that uses the same model:
//   - green sphere at the selected joint,
//   - green bone pyramids out to each child joint,
//   - an orange bone pyramid back to the parent joint.
class SkeletonEditor {
public:
    SkeletonEditor();
    ~SkeletonEditor();

    // Call when the selected model asset changes; resolves the Model from the
    // given .cmdl game path (no-op if unchanged). Pins the Model alive while shown.
    void set_asset(const std::string& cmdl_path);

    // Draw the skeleton section (no Begin/End wrapper — caller owns the window).
    // Renders nothing if the resolved model has no skeleton.
    void imgui_draw();

    const std::string& get_asset_path() const { return cmdl_path_; }

private:
    void draw_bone_tree();
    void update_debug_viz(); // rebuilds the no-depth meshbuilder each frame

    std::string cmdl_path_;
    std::shared_ptr<Model> model_;
    int selected_bone_ = -1;

    MeshBuilder mb_;
    handle<MeshBuilder_Object> mb_handle_;
};

#endif // EDITOR_BUILD
