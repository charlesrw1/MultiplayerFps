#pragma once
#ifdef EDITOR_BUILD
#include "Animation/AnimTreeAsset.h"
#include "Animation/Editor/BoneMaskEditor.h"
#include "Framework/PropertyEd.h"
#include "Framework/FnFactory.h"
#include "LevelEditor/PropertyEditors.h"
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

// AnimTree editor panel. Embedded in AssetInspectorPane when an AnimTreeAsset is selected --
// see AnimSeqEditor for the precedent this follows (set_asset()/imgui_draw(), no Begin/End,
// caller owns the window).
//
// Editing happens on an in-memory working copy (working_root_/working_cached_roots_/
// working_skeleton_), not the AnimTreeAsset directly -- Apply writes it back + saves to disk,
// Revert discards it and re-copies from the asset. This mirrors AnimSeqEditor's
// apply_sidecar()/revert_editor() split.
class AnimTreeEditor
{
public:
	AnimTreeEditor();
	~AnimTreeEditor();

	void set_asset(const std::string& asset_path);
	void imgui_draw();
	const std::string& get_asset_path() const { return asset_path_; }

private:
	void draw_graph_tab();
	void draw_toolbar();
	// Node tree drawn as a table (grid lines between rows + an explicit Depth column) instead of
	// a plain indented list, so nesting depth is unambiguous at a glance.
	void draw_tree();
	void draw_node_row(AnimTreeNode* node, AnimTreeNode* parent, int depth);
	void draw_context_menu(AnimTreeNode* node, AnimTreeNode* parent);
	// Thin drop target above a row: dropping here reorders the dragged node to sit right before
	// `before_node` in `parent`'s children list.
	void draw_reorder_gap(AnimTreeNode* before_node, AnimTreeNode* parent);
	// Drop target for the row body itself: dropping here nests the dragged node as the last
	// child of `node` (reparenting), subject to `node`'s kind arity.
	void draw_nest_drop_target(AnimTreeNode* node);
	void move_node_before(AnimTreeNode* dragged, AnimTreeNode* new_parent, AnimTreeNode* before_node);
	void draw_property_panel();
	// Clip's AssetPtr<AnimationSeqAsset> field is the only param that needs the asset-picker
	// widget (thumbnail/browse/drag-drop) -- built as a one-row "synthetic" PropertyGrid the
	// same way ParticleSystemEditorUi::draw_renderer_module does for its material AssetPtr.
	// Everything else in this file is drawn with plain ImGui calls, no reflection involved.
	bool draw_clip_params(struct AnimTreeClipParams& p);
	void draw_overview_tab();

	// Right-click "Parent to New Node": wraps `node` in a freshly created node of `new_kind`,
	// taking `node`'s old slot (root or a specific child index of `parent`). `node` itself is
	// unaffected -- selected_/collapsed_ pointers stay valid since it's moved, not recreated.
	void insert_parent_above(AnimTreeNode* node, AnimTreeNode* parent, AnimTreeNodeKind new_kind);

	void mark_dirty() { dirty_ = true; }
	void apply_to_disk();
	void revert_from_disk();

	AnimTreeNode* active_root() const; // working_root_.get() or working_cached_roots_[active_root_index_]
	std::string format_node_label(const AnimTreeNode& node) const;

	std::string asset_path_;
	class AnimTreeAsset* asset_ = nullptr; // non-owning, managed by AssetDatabase

	std::unique_ptr<AnimTreeNode> working_root_;
	std::vector<std::pair<std::string, std::unique_ptr<AnimTreeNode>>> working_cached_roots_;
	AssetPtr<class Model> working_skeleton_;
	bool dirty_ = false;

	int active_root_index_ = -1; // -1 == main root, else index into working_cached_roots_
	AnimTreeNode* selected_ = nullptr;
	std::unordered_set<const AnimTreeNode*> collapsed_;

	std::unique_ptr<PropertyGrid> clip_pg_;
	const AnimTreeNode* clip_pg_owner_ = nullptr; // rebuild clip_pg_ only when selection changes
	BoneMaskEditor bone_mask_editor_;
	AssetSlotWidget skeleton_slot_;

	bool show_add_cached_pose_popup_ = false;
	char add_cached_pose_name_[64] = {};
	bool renaming_ = false;
	char rename_buf_[128] = {};
};
#endif
