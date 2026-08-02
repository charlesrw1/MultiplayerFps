#pragma once
#ifdef EDITOR_BUILD
#include "Animation/AnimTreeAsset.h"
#include "Animation/Editor/BoneMaskEditor.h"
#include "Framework/PropertyEd.h"
#include "Framework/FnFactory.h"
#include "Framework/NodeCanvasImgui.h"
#include "LevelEditor/PropertyEditors.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class UndoRedoSystem; // LevelEditor/Commands.h -- only used via unique_ptr here, see command_mgr_

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

	// Free-form graph view of active_root()'s tree plus working_orphans_, drawn with
	// NodeCanvasImgui: one node per AnimTreeNode, one output pin (this node's produced pose) + one
	// input pin per child slot (plus one trailing empty slot when arity allows more children), one
	// link per parent-child edge. AnimTreeNode::children is still the source of truth for what's
	// attached to what -- this is a graph *view* over it, not a full graph data model -- but
	// working_orphans_ makes unattached subtrees a first-class state instead of forcing every node
	// to have a parent. Dragging a link onto the trailing empty slot reparents (from either the
	// tree or the orphan list); deleting a link detaches into working_orphans_ rather than
	// destroying the subtree; right-clicking empty canvas space spawns a fresh orphan node.
	void draw_graph_canvas();
	struct PinRef { AnimTreeNode* node; bool is_output; int slot_index; };
	struct LinkToDraw { int link_id, start_pin, end_pin; };
	// is_orphan: node is a top-level entry of working_orphans_ (no parent, but unlike the active
	// root it can be freely deleted/duplicated -- see draw_context_menu).
	void draw_canvas_node(AnimTreeNode* node, AnimTreeNode* parent, bool is_orphan, std::vector<LinkToDraw>& out_links);
	int id_for(const AnimTreeNode* node);

	// Finds where `child` currently lives, searching the active root's tree and every orphan's
	// subtree (but not the orphan-list membership itself -- see is_top_level_orphan). Used by both
	// link-drag reparenting and link-delete detaching so both operations work uniformly regardless
	// of whether the dragged node started out attached or free-floating.
	bool find_owner(AnimTreeNode* child, AnimTreeNode*& out_parent, int& out_index);
	// True if `node` is itself an entry of working_orphans_ (as opposed to living inside one).
	bool is_top_level_orphan(AnimTreeNode* node, int* out_index = nullptr);

	// Removes just `node` (wherever it currently lives -- tree or top-level orphan) and promotes
	// its own children to top-level orphans instead of destroying the whole subtree. Does not call
	// mark_dirty() -- callers batch that themselves. `node` must not be the active root.
	void delete_node_unparent_children(AnimTreeNode* node);

	void draw_context_menu(AnimTreeNode* node, AnimTreeNode* parent, bool is_orphan);
	void draw_property_panel();
	// Clip's AssetPtr<AnimationSeqAsset> field is the only param that needs the asset-picker
	// widget (thumbnail/browse/drag-drop) -- built as a one-row "synthetic" PropertyGrid the
	// same way ParticleSystemEditorUi::draw_renderer_module does for its material AssetPtr.
	// Everything else in this file is drawn with plain ImGui calls, no reflection involved.
	bool draw_clip_params(struct AnimTreeClipParams& p);
	void draw_overview_tab();

	// Right-click "Parent to New Node": wraps `node` in a freshly created node of `new_kind`,
	// taking `node`'s old slot (root or a specific child index of `parent`). `node` itself is
	// unaffected -- selected_ stays valid since it's moved, not recreated.
	// pos_override, if given, is used verbatim for the new node's editor_pos instead of the default
	// (node->editor_pos, offset left) -- used when the new node's position is already known, e.g.
	// dropping a dragged link on empty canvas space.
	void insert_parent_above(AnimTreeNode* node, AnimTreeNode* parent, bool is_orphan, AnimTreeNodeKind new_kind,
							  const glm::vec2* pos_override = nullptr);

	// Marks a mutation as having happened this frame; opens an undo session if one isn't already
	// active (see imgui_draw() for how sessions are captured/closed and pushed to command_mgr_).
	// Callers don't need to think about undo at all beyond calling this.
	void mark_dirty() { edit_session_active_ = true; dirty_ = true; }
	void apply_to_disk();
	void revert_from_disk();

	// Full copy of everything undo needs to restore: the editable trees + skeleton + which root is
	// active. Cheap enough to clone wholesale every idle frame for an anim tree's typical size --
	// see imgui_draw() for when that actually happens. AnimTreeEditCommand (routes AnimTree edits
	// through the same Command/UndoRedoSystem the level editor uses, see LevelEditor/Commands.h)
	// needs direct access to capture/restore, hence the friend rather than making these public.
	friend class AnimTreeEditCommand;
	struct UndoSnapshot {
		std::unique_ptr<AnimTreeNode> root;
		std::vector<std::pair<std::string, std::unique_ptr<AnimTreeNode>>> cached_roots;
		std::vector<std::unique_ptr<AnimTreeNode>> orphans;
		AssetPtr<class Model> skeleton;
		int active_root_index = -1;
	};
	UndoSnapshot capture_snapshot() const;
	void restore_snapshot(UndoSnapshot&& snap);

	AnimTreeNode* active_root() const; // working_root_.get() or working_cached_roots_[active_root_index_]
	std::string format_node_label(const AnimTreeNode& node) const;

	std::string asset_path_;
	class AnimTreeAsset* asset_ = nullptr; // non-owning, managed by AssetDatabase

	std::unique_ptr<AnimTreeNode> working_root_;
	std::vector<std::pair<std::string, std::unique_ptr<AnimTreeNode>>> working_cached_roots_;
	std::vector<std::unique_ptr<AnimTreeNode>> working_orphans_; // free-floating nodes, not yet attached anywhere -- see AnimTreeAsset::orphans
	AssetPtr<class Model> working_skeleton_;
	bool dirty_ = false;

	int active_root_index_ = -1; // -1 == main root, else index into working_cached_roots_
	AnimTreeNode* selected_ = nullptr;

	std::unique_ptr<PropertyGrid> clip_pg_;
	const AnimTreeNode* clip_pg_owner_ = nullptr; // rebuild clip_pg_ only when selection changes
	BoneMaskEditor bone_mask_editor_;
	AssetSlotWidget skeleton_slot_;

	bool show_add_cached_pose_popup_ = false;
	char add_cached_pose_name_[64] = {};
	bool renaming_ = false;
	char rename_buf_[128] = {};

	NodeCanvasImgui canvas_;
	std::unordered_map<const AnimTreeNode*, int> node_ids_; // stable-for-the-session int id per node, for NodeCanvasImgui pin ids
	int next_node_id_ = 1;
	std::unordered_map<int, PinRef> pin_lookup_; // rebuilt every draw_graph_canvas() call
	AnimTreeNode* context_menu_node_ = nullptr; // node the currently-open right-click popup applies to
	AnimTreeNode* context_menu_parent_ = nullptr;
	bool context_menu_is_orphan_ = false;

	// Right-click empty canvas space -> pick a kind -> spawns a new orphan node at that position.
	bool show_add_node_popup_ = false;
	glm::vec2 add_node_popup_pos_ = glm::vec2(0.f);

	// Drag a link off a pin and release over empty space -> pick a kind -> spawns a new node at
	// that position, auto-connected to the pin the drag started from (see draw_graph_canvas).
	bool show_drop_add_node_popup_ = false;
	PinRef drop_add_node_pin_{nullptr, false, -1};
	glm::vec2 drop_add_node_pos_ = glm::vec2(0.f);

	// Draggable-splitter width of the property panel (##at_prop_region), in pixels.
	float prop_panel_width_ = 380.f;

	// Undo routes through the same Command/UndoRedoSystem the level editor uses (see
	// LevelEditor/Commands.h), via one AnimTreeEditCommand per edit "session" -- this editor isn't
	// an EditorDoc (it's embedded in AssetInspectorPane), so it owns its own UndoRedoSystem rather
	// than sharing the level editor's. A session runs from the first mark_dirty() after an idle
	// frame until nothing's held active (mouse up, no focused item) -- see imgui_draw() -- so a
	// whole drag or a burst of typing becomes one undo step, matching EdPropertyGrid.cpp's pattern
	// for entity property edits. Note UndoRedoSystem has no redo(), so neither does this.
	std::unique_ptr<UndoRedoSystem> command_mgr_;
	bool edit_session_active_ = false;
	UndoSnapshot session_before_snapshot_;
};
#endif
