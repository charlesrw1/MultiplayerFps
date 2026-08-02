#ifdef EDITOR_BUILD
#include "AnimTreeEditor.h"
#include "Assets/AssetDatabase.h"
#include "Assets/AssetRegistry.h"
#include "Render/Model.h"
#include "Animation/SkeletonData.h"
#include "Framework/FnFactory.h"
#include "AssetCompile/Someutils.h"
#include "LevelEditor/Commands.h"
#include "imgui.h"
#include <algorithm>
#include <cstring>
#include <type_traits>

// Routes AnimTree edits through the same Command/UndoRedoSystem the level editor uses (see
// AnimTreeEditor::command_mgr_ in the header for why this editor owns its own instance). One
// session's worth of edits (see imgui_draw()) becomes one of these, holding the tree state from
// before the session started. execute() is a deliberate no-op: by construction the live state
// already equals what the session produced (that's *why* the session just closed), and reapplying
// it via restore_snapshot() would wrongly clear the current selection (restore_snapshot() clears it
// because undo genuinely swaps in fresh node pointers -- here nothing actually changed). undo() is
// only ever called once per instance (UndoRedoSystem's ring buffer is single-use per slot, no
// redo), so moving `before` out in undo() is safe.
class AnimTreeEditCommand : public Command {
public:
	AnimTreeEditCommand(AnimTreeEditor& editor, AnimTreeEditor::UndoSnapshot before)
		: editor(editor), before(std::move(before)) {}
	void execute() final {}
	void undo() final { editor.restore_snapshot(std::move(before)); }
	std::string to_string() final { return "Edit AnimTree"; }

private:
	AnimTreeEditor& editor;
	AnimTreeEditor::UndoSnapshot before;
};

// ---- manual (non-reflected) param widgets ----
// The AnimTree property panel is hand-written per node kind instead of going through the
// PropertyGrid/reflection system: param structs mix plain fields, mode-switched
// Float/Int/Bool/Vec3Value pairs, and bone names that need a skeleton-aware picker, none of
// which the generic reflection-driven editors handle well. The one exception is Clip's
// AssetPtr<AnimationSeqAsset> -- see draw_clip_params() below -- which reuses the asset-picker
// widget via a one-row "synthetic" PropertyGrid, the same pattern as
// ParticleSystemEditorUi::draw_renderer_module's material field.

namespace {

bool draw_stringname_text(const char* imgui_id, StringName& s) {
	char buf[128];
	const char* cur = s.get_c_str();
	std::strncpy(buf, cur ? cur : "", sizeof(buf) - 1);
	buf[sizeof(buf) - 1] = 0;
	// Resynced from `s` every frame, but `s` is written immediately below whenever the widget
	// reports a change (not just on Enter/deactivate), so this is idempotent and never clobbers
	// an in-progress edit -- no persistent scratch buffer needed.
	bool changed = ImGui::InputText(imgui_id, buf, sizeof(buf));
	if (changed)
		s = StringName(buf);
	return changed;
}

bool draw_labeled_stringname(const char* label, StringName& s) {
	char buf[128];
	const char* cur = s.get_c_str();
	std::strncpy(buf, cur ? cur : "", sizeof(buf) - 1);
	buf[sizeof(buf) - 1] = 0;
	ImGui::SetNextItemWidth(200.f);
	bool changed = ImGui::InputText(label, buf, sizeof(buf));
	if (changed)
		s = StringName(buf);
	return changed;
}

bool draw_mode_combo(AnimTreeValueMode& mode) {
	int m = (int)mode;
	const char* modes[] = {"Constant", "Variable"};
	ImGui::SetNextItemWidth(88.f);
	bool changed = ImGui::Combo("##mode", &m, modes, 2);
	if (changed)
		mode = (AnimTreeValueMode)m;
	return changed;
}

bool draw_float_value(AnimTreeFloatValue& v) {
	bool changed = draw_mode_combo(v.mode);
	ImGui::SameLine();
	if (v.mode == AnimTreeValueMode::Constant) {
		ImGui::SetNextItemWidth(120.f);
		changed |= ImGui::DragFloat("##const", &v.constant, 0.01f);
	} else {
		ImGui::SetNextItemWidth(140.f);
		changed |= draw_stringname_text("##var", v.var_name);
	}
	return changed;
}
bool draw_int_value(AnimTreeIntValue& v) {
	bool changed = draw_mode_combo(v.mode);
	ImGui::SameLine();
	if (v.mode == AnimTreeValueMode::Constant) {
		ImGui::SetNextItemWidth(120.f);
		changed |= ImGui::DragInt("##const", &v.constant);
	} else {
		ImGui::SetNextItemWidth(140.f);
		changed |= draw_stringname_text("##var", v.var_name);
	}
	return changed;
}
bool draw_bool_value(AnimTreeBoolValue& v) {
	bool changed = draw_mode_combo(v.mode);
	ImGui::SameLine();
	if (v.mode == AnimTreeValueMode::Constant) {
		changed |= ImGui::Checkbox("##const", &v.constant);
	} else {
		ImGui::SetNextItemWidth(140.f);
		changed |= draw_stringname_text("##var", v.var_name);
	}
	return changed;
}
bool draw_vec3_value(AnimTreeVec3Value& v) {
	bool changed = draw_mode_combo(v.mode);
	ImGui::SameLine();
	if (v.mode == AnimTreeValueMode::Constant) {
		ImGui::SetNextItemWidth(180.f);
		changed |= ImGui::DragFloat3("##const", &v.constant.x, 0.01f);
	} else {
		ImGui::SetNextItemWidth(140.f);
		changed |= draw_stringname_text("##var", v.var_name);
	}
	return changed;
}

bool draw_labeled_float_value(const char* label, AnimTreeFloatValue& v) {
	ImGui::TextUnformatted(label);
	ImGui::SameLine(140.f);
	ImGui::PushID(label);
	bool changed = draw_float_value(v);
	ImGui::PopID();
	return changed;
}
bool draw_labeled_int_value(const char* label, AnimTreeIntValue& v) {
	ImGui::TextUnformatted(label);
	ImGui::SameLine(140.f);
	ImGui::PushID(label);
	bool changed = draw_int_value(v);
	ImGui::PopID();
	return changed;
}
bool draw_labeled_bool_value(const char* label, AnimTreeBoolValue& v) {
	ImGui::TextUnformatted(label);
	ImGui::SameLine(140.f);
	ImGui::PushID(label);
	bool changed = draw_bool_value(v);
	ImGui::PopID();
	return changed;
}
bool draw_labeled_vec3_value(const char* label, AnimTreeVec3Value& v) {
	ImGui::TextUnformatted(label);
	ImGui::SameLine(140.f);
	ImGui::PushID(label);
	bool changed = draw_vec3_value(v);
	ImGui::PopID();
	return changed;
}

// Dropdown bone picker backed by the working skeleton -- replaces free-text bone name entry.
bool draw_labeled_bone(const char* label, StringName& bone, const MSkeleton* skel) {
	ImGui::TextUnformatted(label);
	ImGui::SameLine(140.f);
	ImGui::PushID(label);
	const char* cur = bone.get_c_str();
	bool changed = false;
	ImGui::SetNextItemWidth(200.f);
	if (ImGui::BeginCombo("##bone", (cur && cur[0]) ? cur : "(none)")) {
		if (ImGui::Selectable("(none)", !(cur && cur[0]))) {
			bone = StringName();
			changed = true;
		}
		if (skel) {
			for (auto& b : skel->get_all_bones()) {
				bool is_sel = cur && b.strname == cur;
				if (ImGui::Selectable(b.strname.c_str(), is_sel)) {
					bone = b.name;
					changed = true;
				}
			}
		} else {
			ImGui::TextDisabled("Assign a skeleton above");
		}
		ImGui::EndCombo();
	}
	ImGui::PopID();
	return changed;
}

const char* g_modifybonetype_names[] = {"None", "Localspace", "LocalspaceAdd", "Bonespace", "BonespaceAdd", "Meshspace", "MeshspaceAdd"};
bool draw_labeled_modifybonetype(const char* label, ModifyBoneType& v) {
	int m = (int)v;
	ImGui::SetNextItemWidth(160.f);
	bool changed = ImGui::Combo(label, &m, g_modifybonetype_names, 7);
	if (changed)
		v = (ModifyBoneType)m;
	return changed;
}

const char* g_easing_names[] = {"Linear", "CubicEaseIn", "CubicEaseOut", "CubicEaseInOut", "Constant"};
bool draw_labeled_easing(const char* label, Easing& v) {
	int m = (int)v;
	ImGui::SetNextItemWidth(160.f);
	bool changed = ImGui::Combo(label, &m, g_easing_names, 5);
	if (changed)
		v = (Easing)m;
	return changed;
}

const char* g_sync_opt_names[] = {"Default", "Always Leader", "Always Follower"};
bool draw_labeled_sync_opt(const char* label, sync_opt& v) {
	int m = (int)v;
	ImGui::SetNextItemWidth(160.f);
	bool changed = ImGui::Combo(label, &m, g_sync_opt_names, 3);
	if (changed)
		v = (sync_opt)m;
	return changed;
}

FnFactory<IPropertyEditor>& get_empty_factory() {
	static FnFactory<IPropertyEditor> factory;
	return factory;
}

// Thin SharedAssetPropertyEditor wrapping AssetPtr<AnimationSeqAsset> -- mirrors
// ParticleEditorUI.cpp's RendererMaterialEditor for the material AssetPtr.
class ClipAssetEditor : public SharedAssetPropertyEditor {
public:
	explicit ClipAssetEditor(AssetPtr<AnimationSeqAsset>* clip) : clip_ptr(clip) {
		class_type_override = &AnimationSeqAsset::StaticType;
	}
	std::string get_str() override {
		return (clip_ptr && clip_ptr->get()) ? clip_ptr->get()->get_name() : "";
	}
	void set_asset(const std::string& str) override {
		if (!clip_ptr) return;
		*clip_ptr = str.empty() ? AssetPtr<AnimationSeqAsset>{} : g_assets.find<AnimationSeqAsset>(str);
	}
	AssetPtr<AnimationSeqAsset>* clip_ptr = nullptr;
};

// ---- tree helpers ----

bool find_parent(AnimTreeNode* root, AnimTreeNode* target, AnimTreeNode*& out_parent, int& out_index) {
	for (int i = 0; i < (int)root->children.size(); i++) {
		if (root->children[i].get() == target) {
			out_parent = root;
			out_index = i;
			return true;
		}
		if (find_parent(root->children[i].get(), target, out_parent, out_index))
			return true;
	}
	return false;
}

bool is_descendant(AnimTreeNode* maybe_ancestor, AnimTreeNode* node) {
	for (auto& c : maybe_ancestor->children) {
		if (c.get() == node || is_descendant(c.get(), node))
			return true;
	}
	return false;
}

struct OverviewVar { std::string name; std::string type; int uses = 0; };

void collect_variable(std::vector<OverviewVar>& vars, const std::string& name, const char* type) {
	if (name.empty())
		return;
	for (auto& v : vars) {
		if (v.name == name && v.type == type) {
			v.uses++;
			return;
		}
	}
	vars.push_back({name, type, 1});
}

void collect_from_node(const AnimTreeNode& node, std::vector<OverviewVar>& vars, std::vector<OverviewVar>& slots,
					   std::vector<OverviewVar>& syncgroups) {
	auto add_val = [&](auto&& val, const char* type) {
		if (val.mode == AnimTreeValueMode::Variable && val.var_name.get_c_str() && val.var_name.get_c_str()[0])
			collect_variable(vars, val.var_name.get_c_str(), type);
	};
	std::visit([&](auto&& p) {
		using T = std::decay_t<decltype(p)>;
		if constexpr (std::is_same_v<T, AnimTreeClipParams>) {
			add_val(p.speed, "Float");
			if (p.sync_group.get_c_str() && p.sync_group.get_c_str()[0])
				collect_variable(syncgroups, p.sync_group.get_c_str(), "SyncGroup");
		} else if constexpr (std::is_same_v<T, AnimTreeBlendParams>) {
			add_val(p.alpha, "Float");
		} else if constexpr (std::is_same_v<T, AnimTreeBlendMaskedParams>) {
			add_val(p.alpha, "Float");
		} else if constexpr (std::is_same_v<T, AnimTreeAddParams>) {
			add_val(p.alpha, "Float");
		} else if constexpr (std::is_same_v<T, AnimTreeIkParams>) {
			add_val(p.alpha, "Float");
			add_val(p.target, "Vec3");
			add_val(p.pole, "Vec3");
		} else if constexpr (std::is_same_v<T, AnimTreeModifyBoneParams>) {
			add_val(p.translation, "Vec3");
			add_val(p.rotation, "Vec3");
			add_val(p.scale, "Vec3");
			add_val(p.alpha, "Float");
		} else if constexpr (std::is_same_v<T, AnimTreeCopyBoneParams>) {
			add_val(p.copy_translation, "Bool");
			add_val(p.copy_rotation, "Bool");
			add_val(p.copy_scale, "Bool");
			add_val(p.alpha, "Float");
		} else if constexpr (std::is_same_v<T, AnimTreeSlotParams>) {
			if (p.slot_name.get_c_str() && p.slot_name.get_c_str()[0])
				collect_variable(slots, p.slot_name.get_c_str(), "Slot");
		} else if constexpr (std::is_same_v<T, AnimTreeBlendByIntParams>) {
			add_val(p.integer, "Int");
		}
	}, node.params);
	for (auto& c : node.children)
		collect_from_node(*c, vars, slots, syncgroups);
}

void draw_overview_table(const char* id, const char* header, std::vector<OverviewVar>& rows, bool show_type) {
	if (!ImGui::CollapsingHeader(header, ImGuiTreeNodeFlags_DefaultOpen))
		return;
	int cols = show_type ? 3 : 2;
	if (ImGui::BeginTable(id, cols, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
		ImGui::TableSetupColumn("Name");
		if (show_type)
			ImGui::TableSetupColumn("Type");
		ImGui::TableSetupColumn("Uses");
		ImGui::TableHeadersRow();
		for (auto& v : rows) {
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(v.name.c_str());
			if (show_type) {
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(v.type.c_str());
			}
			ImGui::TableNextColumn();
			ImGui::Text("%d", v.uses);
		}
		ImGui::EndTable();
	}
	if (rows.empty())
		ImGui::TextDisabled("(none)");
}

// Titlebar tint by rough node role, so the graph reads at a glance without having to read every
// label: pose sources/sinks (data in/out), bone manipulators (per-bone edits), and blends/mixers
// (everything that combines or selects between poses).
ImU32 category_color_for_kind(AnimTreeNodeKind kind) {
	switch (kind) {
	case AnimTreeNodeKind::Clip:
	case AnimTreeNodeKind::BindPose:
	case AnimTreeNodeKind::SaveCachedPose:
	case AnimTreeNodeKind::UseCachedPose:
		return IM_COL32(70, 110, 80, 255); // pose sources/sinks
	case AnimTreeNodeKind::IK:
	case AnimTreeNodeKind::ModifyBone:
	case AnimTreeNodeKind::CopyBone:
		return IM_COL32(150, 95, 60, 255); // per-bone manipulators
	case AnimTreeNodeKind::Blend:
	case AnimTreeNodeKind::BlendMasked:
	case AnimTreeNodeKind::Add:
	case AnimTreeNodeKind::MakeAdditive:
	case AnimTreeNodeKind::BlendByInt:
	case AnimTreeNodeKind::Slot:
		return IM_COL32(70, 95, 140, 255); // blends/mixers
	}
	return IM_COL32(60, 60, 66, 255);
}

} // namespace

AnimTreeEditor::AnimTreeEditor() : command_mgr_(std::make_unique<UndoRedoSystem>()) {}
AnimTreeEditor::~AnimTreeEditor() = default;

void AnimTreeEditor::set_asset(const std::string& asset_path) {
	asset_path_ = asset_path;
	asset_ = g_assets.find<AnimTreeAsset>(asset_path).get();
	revert_from_disk();
}

int AnimTreeEditor::id_for(const AnimTreeNode* node) {
	auto it = node_ids_.find(node);
	if (it != node_ids_.end())
		return it->second;
	int id = next_node_id_++;
	node_ids_[node] = id;
	return id;
}

AnimTreeNode* AnimTreeEditor::active_root() const {
	if (active_root_index_ < 0)
		return working_root_.get();
	if (active_root_index_ < (int)working_cached_roots_.size())
		return working_cached_roots_[active_root_index_].second.get();
	return working_root_.get();
}

void AnimTreeEditor::revert_from_disk() {
	selected_ = nullptr;
	clip_pg_owner_ = nullptr;
	node_ids_.clear();
	active_root_index_ = -1;
	// Reverting/reloading swaps out the whole editable state from under any recorded history, so
	// there's nothing left for those commands to meaningfully undo back to -- start fresh.
	command_mgr_->clear_all();
	edit_session_active_ = false;

	if (!asset_) {
		working_root_ = std::make_unique<AnimTreeNode>(AnimTreeNodeKind::BindPose);
		working_cached_roots_.clear();
		working_orphans_.clear();
		working_skeleton_ = AssetPtr<Model>{};
		dirty_ = false;
		return;
	}
	working_root_ = asset_->root ? asset_->root->clone() : std::make_unique<AnimTreeNode>(AnimTreeNodeKind::BindPose);
	working_cached_roots_.clear();
	for (auto& e : asset_->cached_pose_roots)
		working_cached_roots_.emplace_back(e.first, e.second->clone());
	working_orphans_.clear();
	for (auto& o : asset_->orphans)
		working_orphans_.push_back(o->clone());
	working_skeleton_ = asset_->skeleton;
	dirty_ = false;
}

AnimTreeEditor::UndoSnapshot AnimTreeEditor::capture_snapshot() const {
	UndoSnapshot snap;
	snap.root = working_root_ ? working_root_->clone() : nullptr;
	for (auto& e : working_cached_roots_)
		snap.cached_roots.emplace_back(e.first, e.second ? e.second->clone() : nullptr);
	for (auto& o : working_orphans_)
		snap.orphans.push_back(o->clone());
	snap.skeleton = working_skeleton_;
	snap.active_root_index = active_root_index_;
	return snap;
}

void AnimTreeEditor::restore_snapshot(UndoSnapshot&& snap) {
	working_root_ = std::move(snap.root);
	working_cached_roots_ = std::move(snap.cached_roots);
	working_orphans_ = std::move(snap.orphans);
	working_skeleton_ = snap.skeleton;
	active_root_index_ = snap.active_root_index;
	// Node pointers changed (fresh clones) -- stale canvas ids/selection would otherwise misattach
	// to whatever ends up at the same address.
	selected_ = nullptr;
	clip_pg_owner_ = nullptr;
	node_ids_.clear();
	dirty_ = true;
}

void AnimTreeEditor::apply_to_disk() {
	if (!asset_)
		return;
	asset_->root = working_root_->clone();
	asset_->cached_pose_roots.clear();
	for (auto& e : working_cached_roots_)
		asset_->cached_pose_roots.emplace_back(e.first, e.second->clone());
	asset_->orphans.clear();
	for (auto& o : working_orphans_)
		asset_->orphans.push_back(o->clone());
	asset_->skeleton = working_skeleton_;
	asset_->save_to_disk();
	dirty_ = false;
}

std::string AnimTreeEditor::format_node_label(const AnimTreeNode& node) const {
	std::string detail;
	switch (node.kind) {
	case AnimTreeNodeKind::Clip: {
		auto& p = std::get<AnimTreeClipParams>(node.params);
		if (p.clip.get()) {
			detail = p.clip.get()->get_name();
			get_filename(detail); // gamepath -> bare clip name, no directories/extension
		} else {
			detail = "(none)";
		}
		break;
	}
	case AnimTreeNodeKind::IK: {
		auto& p = std::get<AnimTreeIkParams>(node.params);
		detail = (p.bone_name.get_c_str() && p.bone_name.get_c_str()[0]) ? p.bone_name.get_c_str() : "(no bone)";
		break;
	}
	case AnimTreeNodeKind::Slot: {
		auto& p = std::get<AnimTreeSlotParams>(node.params);
		detail = (p.slot_name.get_c_str() && p.slot_name.get_c_str()[0]) ? p.slot_name.get_c_str() : "(no name)";
		break;
	}
	case AnimTreeNodeKind::BlendByInt: {
		auto& p = std::get<AnimTreeBlendByIntParams>(node.params);
		detail = (p.integer.mode == AnimTreeValueMode::Variable && p.integer.var_name.get_c_str())
					 ? p.integer.var_name.get_c_str()
					 : "(constant)";
		break;
	}
	case AnimTreeNodeKind::Add: {
		auto& p = std::get<AnimTreeAddParams>(node.params);
		if (p.alpha.mode == AnimTreeValueMode::Variable && p.alpha.var_name.get_c_str())
			detail = p.alpha.var_name.get_c_str();
		break;
	}
	case AnimTreeNodeKind::ModifyBone: {
		auto& p = std::get<AnimTreeModifyBoneParams>(node.params);
		if (p.bone_name.get_c_str())
			detail = p.bone_name.get_c_str();
		break;
	}
	case AnimTreeNodeKind::CopyBone: {
		auto& p = std::get<AnimTreeCopyBoneParams>(node.params);
		if (p.source_bone.get_c_str() && p.target_bone.get_c_str())
			detail = std::string(p.source_bone.get_c_str()) + " -> " + p.target_bone.get_c_str();
		break;
	}
	case AnimTreeNodeKind::SaveCachedPose: {
		auto& p = std::get<AnimTreeSaveCachedPoseParams>(node.params);
		if (p.cache_name.get_c_str())
			detail = p.cache_name.get_c_str();
		break;
	}
	case AnimTreeNodeKind::UseCachedPose: {
		auto& p = std::get<AnimTreeUseCachedPoseParams>(node.params);
		if (p.cache_name.get_c_str())
			detail = p.cache_name.get_c_str();
		break;
	}
	default:
		break;
	}

	std::string label = get_kind_display_name(node.kind);
	if (!detail.empty())
		label += " (" + detail + ")";
	if (!node.name.empty())
		label = node.name + "  [" + label + "]";
	return label;
}

void AnimTreeEditor::draw_context_menu(AnimTreeNode* node, AnimTreeNode* parent, bool is_orphan) {
	int arity = get_arity_for_kind(node->kind);
	bool can_add = (arity < 0) || ((int)node->children.size() < arity);
	if (ImGui::BeginMenu("Add Child", can_add)) {
		for (int k = 0; k <= (int)AnimTreeNodeKind::UseCachedPose; k++) {
			auto kind = (AnimTreeNodeKind)k;
			if (ImGui::MenuItem(get_kind_display_name(kind))) {
				auto child = std::make_unique<AnimTreeNode>(kind);
				child->editor_pos = node->editor_pos + glm::vec2(240.f, 80.f * (float)node->children.size());
				node->children.push_back(std::move(child));
				mark_dirty();
			}
		}
		ImGui::EndMenu();
	}

	// Change Kind: the only way to fix a node created with the wrong (or default) kind --
	// e.g. every fresh AnimTree starts with a BindPose root, which is a 0-arity leaf, so this
	// is also the only way to turn the root into something that can have children at all.
	// Extra children beyond the new kind's arity are dropped since they'd be unreachable.
	if (ImGui::BeginMenu("Change Kind")) {
		for (int k = 0; k <= (int)AnimTreeNodeKind::UseCachedPose; k++) {
			auto kind = (AnimTreeNodeKind)k;
			if (kind == node->kind)
				continue;
			if (ImGui::MenuItem(get_kind_display_name(kind))) {
				node->kind = kind;
				node->params = make_default_params_for_kind(kind);
				int new_arity = get_arity_for_kind(kind);
				if (new_arity >= 0 && (int)node->children.size() > new_arity)
					node->children.resize(new_arity);
				if (selected_ == node)
					clip_pg_owner_ = nullptr;
				mark_dirty();
			}
		}
		ImGui::EndMenu();
	}

	// Parent to New Node: inserts a fresh node above `node`, taking its old slot (root or a
	// specific child index), with `node` becoming its sole child. Only kinds that can actually
	// hold a child are offered.
	if (ImGui::BeginMenu("Parent to New Node")) {
		for (int k = 0; k <= (int)AnimTreeNodeKind::UseCachedPose; k++) {
			auto kind = (AnimTreeNodeKind)k;
			if (get_arity_for_kind(kind) == 0)
				continue;
			if (ImGui::MenuItem(get_kind_display_name(kind))) {
				insert_parent_above(node, parent, is_orphan, kind);
			}
		}
		ImGui::EndMenu();
	}

	// Delete/Duplicate are unavailable only for the active root itself (parent == nullptr and not
	// an orphan) -- there's nowhere to remove it *from*. Orphans have parent == nullptr too but are
	// top-level entries of working_orphans_, so they can be deleted/duplicated like any other node.
	bool can_remove = (parent != nullptr) || is_orphan;
	if (ImGui::MenuItem("Duplicate", nullptr, false, can_remove)) {
		if (is_orphan) {
			auto dup = node->clone();
			dup->editor_pos = node->editor_pos + glm::vec2(40.f, 40.f);
			working_orphans_.push_back(std::move(dup));
			mark_dirty();
		} else {
			int p_arity = get_arity_for_kind(parent->kind);
			if (p_arity < 0 || (int)parent->children.size() < p_arity) {
				for (int i = 0; i < (int)parent->children.size(); i++) {
					if (parent->children[i].get() == node) {
						auto dup = node->clone();
						dup->editor_pos = node->editor_pos + glm::vec2(40.f, 40.f);
						parent->children.insert(parent->children.begin() + i + 1, std::move(dup));
						break;
					}
				}
				mark_dirty();
			}
		}
	}
	// Deletes only `node` itself -- its children are promoted to top-level orphans rather than
	// destroyed along with it, so detaching a subtree's root doesn't take the rest of it out too.
	if (ImGui::MenuItem("Delete", nullptr, false, can_remove)) {
		delete_node_unparent_children(node);
		mark_dirty();
		ImGui::CloseCurrentPopup();
		return;
	}

	ImGui::Separator();
	if (ImGui::IsWindowAppearing())
		std::strncpy(rename_buf_, node->name.c_str(), sizeof(rename_buf_) - 1);
	ImGui::SetNextItemWidth(160.f);
	if (ImGui::InputText("Name", rename_buf_, sizeof(rename_buf_), ImGuiInputTextFlags_EnterReturnsTrue)) {
		node->name = rename_buf_;
		mark_dirty();
		ImGui::CloseCurrentPopup();
	}
}

void AnimTreeEditor::insert_parent_above(AnimTreeNode* node, AnimTreeNode* parent, bool is_orphan, AnimTreeNodeKind new_kind,
										  const glm::vec2* pos_override) {
	auto new_node = std::make_unique<AnimTreeNode>(new_kind);
	new_node->editor_pos = pos_override ? *pos_override : node->editor_pos - glm::vec2(240.f, 0.f);
	if (is_orphan) {
		for (auto& o : working_orphans_) {
			if (o.get() == node) {
				new_node->children.push_back(std::move(o));
				o = std::move(new_node);
				break;
			}
		}
	} else if (!parent) {
		// node is the active root: draw_graph_canvas() only ever calls draw_canvas_node(root, nullptr, false, ...),
		// so parent == nullptr && !is_orphan implies node == active_root().
		if (active_root_index_ < 0) {
			new_node->children.push_back(std::move(working_root_));
			working_root_ = std::move(new_node);
		} else {
			auto& slot = working_cached_roots_[active_root_index_].second;
			new_node->children.push_back(std::move(slot));
			slot = std::move(new_node);
		}
	} else {
		for (auto& c : parent->children) {
			if (c.get() == node) {
				new_node->children.push_back(std::move(c));
				c = std::move(new_node);
				break;
			}
		}
	}
	mark_dirty();
}

bool AnimTreeEditor::find_owner(AnimTreeNode* child, AnimTreeNode*& out_parent, int& out_index) {
	AnimTreeNode* root = active_root();
	if (root && find_parent(root, child, out_parent, out_index))
		return true;
	for (auto& o : working_orphans_) {
		if (find_parent(o.get(), child, out_parent, out_index))
			return true;
	}
	return false;
}

bool AnimTreeEditor::is_top_level_orphan(AnimTreeNode* node, int* out_index) {
	for (int i = 0; i < (int)working_orphans_.size(); i++) {
		if (working_orphans_[i].get() == node) {
			if (out_index) *out_index = i;
			return true;
		}
	}
	return false;
}

void AnimTreeEditor::delete_node_unparent_children(AnimTreeNode* node) {
	if (selected_ == node) { selected_ = nullptr; clip_pg_owner_ = nullptr; }

	for (auto& c : node->children)
		working_orphans_.push_back(std::move(c));
	node->children.clear();

	int orphan_index = -1;
	if (is_top_level_orphan(node, &orphan_index)) {
		working_orphans_.erase(working_orphans_.begin() + orphan_index);
		return;
	}
	AnimTreeNode* owner_parent = nullptr;
	int owner_index = -1;
	if (find_owner(node, owner_parent, owner_index))
		owner_parent->children.erase(owner_parent->children.begin() + owner_index);
}

void AnimTreeEditor::draw_canvas_node(AnimTreeNode* node, AnimTreeNode* parent, bool is_orphan, std::vector<LinkToDraw>& out_links) {
	int nid = id_for(node);
	int arity = get_arity_for_kind(node->kind);
	int child_count = (int)node->children.size();
	bool extra_slot = (arity < 0) || (child_count < arity);
	int input_pins = std::max(child_count + (extra_slot ? 1 : 0), 0);

	ImVec2 size(190.f, 34.f + std::max(input_pins, 1) * 20.f);
	ImVec2 pos_im(node->editor_pos.x, node->editor_pos.y);

	canvas_.begin_node(nid, pos_im, size, format_node_label(*node).c_str(), category_color_for_kind(node->kind));
	node->editor_pos = glm::vec2(pos_im.x, pos_im.y);

	if (canvas_.is_last_node_hovered()) {
		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
			selected_ = node;
		if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
			// draw_context_menu gates Duplicate/Delete on can_remove, so the active root (the only
			// node with parent == nullptr that isn't an orphan) still gets Add Child / Change Kind /
			// Rename from the same popup, just without Duplicate/Delete.
			context_menu_node_ = node;
			context_menu_parent_ = parent;
			context_menu_is_orphan_ = is_orphan;
			ImGui::OpenPopup("##at_node_ctx");
		}
	}

	// output pin: this node's produced pose, consumed by whichever node's input pin it's linked to
	canvas_.pin(nid * 1000, true, "");
	pin_lookup_[nid * 1000] = PinRef{node, true, -1};

	for (int i = 0; i < input_pins; i++) {
		bool is_empty_slot = (i >= child_count);
		int pin_id = nid * 1000 + 1 + i;
		canvas_.pin(pin_id, false, is_empty_slot ? "+" : "", is_empty_slot ? IM_COL32(120, 200, 120, 255) : IM_COL32(210, 210, 210, 255));
		pin_lookup_[pin_id] = PinRef{node, false, i};
	}

	canvas_.end_node();

	for (int i = 0; i < child_count; i++) {
		AnimTreeNode* c = node->children[i].get();
		int cid = id_for(c);
		out_links.push_back(LinkToDraw{cid * 1000, cid * 1000, nid * 1000 + 1 + i});
		draw_canvas_node(c, node, false, out_links);
	}
}

void AnimTreeEditor::draw_graph_canvas() {
	AnimTreeNode* root = active_root();
	if (!root)
		return;

	pin_lookup_.clear();

	canvas_.begin_canvas("##at_canvas", ImVec2(0, 0));
	std::vector<LinkToDraw> links;
	draw_canvas_node(root, nullptr, false, links);
	for (auto& o : working_orphans_)
		draw_canvas_node(o.get(), nullptr, true, links);
	for (auto& l : links)
		canvas_.link(l.link_id, l.start_pin, l.end_pin);

	canvas_.end_canvas();

	// must read after end_canvas(): that's where the background-right-click event is resolved.
	ImVec2 bg_click_pos;
	bool bg_right_clicked = canvas_.is_background_right_clicked(&bg_click_pos);

	// Right-click on empty canvas space: pick a kind, spawns a new free-floating node at that
	// position in working_orphans_ -- drag its output pin onto some node's empty input slot to
	// attach it, or leave it floating (Apply preserves unattached nodes across sessions).
	if (bg_right_clicked) {
		show_add_node_popup_ = true;
		add_node_popup_pos_ = glm::vec2(bg_click_pos.x, bg_click_pos.y);
	}
	if (show_add_node_popup_) {
		ImGui::OpenPopup("##at_add_node");
		show_add_node_popup_ = false;
	}
	if (ImGui::BeginPopup("##at_add_node")) {
		for (int k = 0; k <= (int)AnimTreeNodeKind::UseCachedPose; k++) {
			auto kind = (AnimTreeNodeKind)k;
			if (ImGui::MenuItem(get_kind_display_name(kind))) {
				auto node = std::make_unique<AnimTreeNode>(kind);
				node->editor_pos = add_node_popup_pos_;
				working_orphans_.push_back(std::move(node));
				mark_dirty();
			}
		}
		ImGui::EndPopup();
	}

	// Drag a link out of a pin and release over empty canvas: pick a kind, spawns a new node at the
	// drop position and auto-connects it to the pin the drag started from -- dropping from an
	// output pin wraps the dragged node in a new parent (like "Parent to New Node" but drag-driven);
	// dropping from an empty input slot attaches the new node directly into that slot.
	int dropped_pin;
	ImVec2 dropped_pos;
	if (canvas_.is_link_dropped_on_empty(&dropped_pin, &dropped_pos)) {
		auto it = pin_lookup_.find(dropped_pin);
		if (it != pin_lookup_.end() && (it->second.is_output || it->second.slot_index == (int)it->second.node->children.size())) {
			show_drop_add_node_popup_ = true;
			drop_add_node_pin_ = it->second;
			drop_add_node_pos_ = glm::vec2(dropped_pos.x, dropped_pos.y);
		}
	}
	if (show_drop_add_node_popup_) {
		ImGui::OpenPopup("##at_drop_add_node");
		show_drop_add_node_popup_ = false;
	}
	if (ImGui::BeginPopup("##at_drop_add_node")) {
		for (int k = 0; k <= (int)AnimTreeNodeKind::UseCachedPose; k++) {
			auto kind = (AnimTreeNodeKind)k;
			bool enabled = drop_add_node_pin_.is_output ? (get_arity_for_kind(kind) != 0) : true;
			if (ImGui::MenuItem(get_kind_display_name(kind), nullptr, false, enabled)) {
				if (drop_add_node_pin_.is_output) {
					AnimTreeNode* dragged = drop_add_node_pin_.node;
					AnimTreeNode* owner_parent = nullptr;
					int owner_index = -1;
					int orphan_index = -1;
					bool is_orph = false;
					if (!find_owner(dragged, owner_parent, owner_index))
						is_orph = is_top_level_orphan(dragged, &orphan_index);
					insert_parent_above(dragged, owner_parent, is_orph, kind, &drop_add_node_pos_);
				} else {
					AnimTreeNode* parent = drop_add_node_pin_.node;
					int arity = get_arity_for_kind(parent->kind);
					if (arity < 0 || (int)parent->children.size() < arity) {
						auto node = std::make_unique<AnimTreeNode>(kind);
						node->editor_pos = drop_add_node_pos_;
						parent->children.push_back(std::move(node));
						mark_dirty();
					}
				}
			}
		}
		ImGui::EndPopup();
	}

	if (ImGui::BeginPopup("##at_node_ctx")) {
		if (context_menu_node_)
			draw_context_menu(context_menu_node_, context_menu_parent_, context_menu_is_orphan_);
		ImGui::EndPopup();
	}

	// Del/Backspace deletes every canvas-selected node -- guarded by WantTextInput so it doesn't eat
	// keystrokes while renaming/editing a field in the property panel. The active root can't be
	// deleted (nothing to remove it from). Each selected node deletes just itself, promoting its
	// own children to orphans (delete_node_unparent_children) -- safe to do independently per
	// selected node, including ancestor+descendant pairs, since a deleted node's children survive
	// as orphans rather than being destroyed with it.
	if (!ImGui::GetIO().WantTextInput && !canvas_.get_selected_nodes().empty() &&
		(ImGui::IsKeyPressed(ImGuiKey_Delete) || ImGui::IsKeyPressed(ImGuiKey_Backspace))) {
		std::vector<AnimTreeNode*> selected_ptrs;
		for (int nid : canvas_.get_selected_nodes()) {
			auto it = pin_lookup_.find(nid * 1000);
			if (it != pin_lookup_.end() && it->second.node != root)
				selected_ptrs.push_back(it->second.node);
		}
		for (AnimTreeNode* n : selected_ptrs) {
			delete_node_unparent_children(n);
			mark_dirty();
		}
	}

	int start_pin, end_pin;
	if (canvas_.is_link_created(&start_pin, &end_pin)) {
		auto it_s = pin_lookup_.find(start_pin);
		auto it_e = pin_lookup_.find(end_pin);
		if (it_s != pin_lookup_.end() && it_e != pin_lookup_.end()) {
			AnimTreeNode* child = it_s->second.node;
			AnimTreeNode* new_parent = it_e->second.node;
			// only accept drops onto the trailing empty slot -- occupied slots are rejected so a
			// drag can't silently clobber an existing child.
			if (it_e->second.slot_index == (int)new_parent->children.size() &&
				child != new_parent && !is_descendant(child, new_parent)) {
				int arity = get_arity_for_kind(new_parent->kind);
				if (arity < 0 || (int)new_parent->children.size() < arity) {
					// Check the target slot's capacity before touching the source so a rejected
					// drop can't leave the node detached nowhere.
					std::unique_ptr<AnimTreeNode> moved;
					AnimTreeNode* old_parent = nullptr;
					int old_index = -1;
					int orphan_index = -1;
					if (find_owner(child, old_parent, old_index)) {
						moved = std::move(old_parent->children[old_index]);
						old_parent->children.erase(old_parent->children.begin() + old_index);
					} else if (is_top_level_orphan(child, &orphan_index)) {
						moved = std::move(working_orphans_[orphan_index]);
						working_orphans_.erase(working_orphans_.begin() + orphan_index);
					}
					if (moved) {
						new_parent->children.push_back(std::move(moved));
						mark_dirty();
					}
				}
			}
		}
	}

	int destroyed_link;
	if (canvas_.is_link_destroyed(&destroyed_link)) {
		// link_id == the child's own output pin id (see draw_canvas_node), so it uniquely
		// identifies the one edge feeding that child's (single) parent. Detaching parks the
		// subtree in working_orphans_ instead of destroying it -- drag its output pin onto another
		// node's empty slot to reattach, or delete it explicitly via its own context menu.
		auto it = pin_lookup_.find(destroyed_link);
		if (it != pin_lookup_.end()) {
			AnimTreeNode* child = it->second.node;
			AnimTreeNode* old_parent = nullptr;
			int old_index = -1;
			if (find_owner(child, old_parent, old_index)) {
				std::unique_ptr<AnimTreeNode> detached = std::move(old_parent->children[old_index]);
				old_parent->children.erase(old_parent->children.begin() + old_index);
				working_orphans_.push_back(std::move(detached));
				mark_dirty();
			}
		}
	}
}

void AnimTreeEditor::draw_toolbar() {
	// Apply/Revert get their own row up front so they're never pushed off-screen by the rest of
	// the toolbar overflowing a narrow inspector panel (ImGui::SameLine() doesn't wrap).
	if (ImGui::Button("Apply"))
		apply_to_disk();
	ImGui::SameLine();
	if (ImGui::Button("Revert"))
		revert_from_disk();
	ImGui::SameLine();
	if (dirty_)
		ImGui::TextColored(ImVec4(1.f, 0.7f, 0.2f, 1.f), "* unsaved changes");
	else
		ImGui::TextDisabled("(no changes)");

	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted("Skeleton:");
	ImGui::SameLine();
	std::string cur = working_skeleton_.get() ? working_skeleton_.get()->get_name() : "";
	const AssetMetadata* meta = AssetRegistrySystem::get().find_type("Model");
	std::string out;
	if (skeleton_slot_.draw(cur, meta, 240.f, out)) {
		working_skeleton_ = out.empty() ? AssetPtr<Model>{} : g_assets.find<Model>(out);
		mark_dirty();
	}

	ImGui::SameLine();
	std::string cur_root_name = active_root_index_ < 0 ? "Root" : working_cached_roots_[active_root_index_].first;
	ImGui::SetNextItemWidth(160.f);
	if (ImGui::BeginCombo("##root_select", cur_root_name.c_str())) {
		if (ImGui::Selectable("Root", active_root_index_ < 0)) {
			active_root_index_ = -1;
			selected_ = nullptr;
		}
		for (int i = 0; i < (int)working_cached_roots_.size(); i++) {
			if (ImGui::Selectable(working_cached_roots_[i].first.c_str(), active_root_index_ == i)) {
				active_root_index_ = i;
				selected_ = nullptr;
			}
		}
		ImGui::EndCombo();
	}
	ImGui::SameLine();
	if (ImGui::SmallButton("+ Cached Pose")) {
		show_add_cached_pose_popup_ = true;
		add_cached_pose_name_[0] = 0;
	}
	if (show_add_cached_pose_popup_) {
		ImGui::OpenPopup("Add Cached Pose Root");
		show_add_cached_pose_popup_ = false;
	}
	if (ImGui::BeginPopupModal("Add Cached Pose Root", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::InputText("Name", add_cached_pose_name_, sizeof(add_cached_pose_name_));
		if (ImGui::Button("Create") && add_cached_pose_name_[0]) {
			working_cached_roots_.emplace_back(add_cached_pose_name_, std::make_unique<AnimTreeNode>(AnimTreeNodeKind::BindPose));
			active_root_index_ = (int)working_cached_roots_.size() - 1;
			selected_ = nullptr;
			mark_dirty();
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel"))
			ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}

}

bool AnimTreeEditor::draw_clip_params(AnimTreeClipParams& p) {
	if (clip_pg_owner_ != selected_) {
		static PropertyInfo s_clip_prop = make_assetptr_property_new("Clip", 0, 0, "", &AnimationSeqAsset::StaticType);
		auto* editor = new ClipAssetEditor(&p.clip);
		editor->prop = &s_clip_prop;
		clip_pg_ = std::make_unique<PropertyGrid>(get_empty_factory());
		clip_pg_->add_iproped_manual(editor);
		clip_pg_owner_ = selected_;
	}
	clip_pg_->rows_had_changes = false;
	clip_pg_->update();
	bool changed = clip_pg_->rows_had_changes;

	changed |= ImGui::Checkbox("Looping", &p.looping);
	changed |= draw_labeled_float_value("Speed", p.speed);
	changed |= draw_labeled_stringname("Sync Group", p.sync_group);
	changed |= draw_labeled_sync_opt("Sync Type", p.sync_type);
	return changed;
}

void AnimTreeEditor::draw_property_panel() {
	if (!selected_) {
		ImGui::TextDisabled("(select a node to edit its properties)");
		return;
	}
	ImGui::TextUnformatted(format_node_label(*selected_).c_str());
	ImGui::Separator();

	const MSkeleton* skel = working_skeleton_.get() ? working_skeleton_.get()->get_skel() : nullptr;
	bool changed = false;
	std::visit([&](auto& p) {
		using T = std::decay_t<decltype(p)>;
		if constexpr (std::is_same_v<T, AnimTreeClipParams>) {
			changed |= draw_clip_params(p);
		} else if constexpr (std::is_same_v<T, AnimTreeBindPoseParams>) {
			ImGui::TextDisabled("(no properties)");
		} else if constexpr (std::is_same_v<T, AnimTreeBlendParams>) {
			changed |= draw_labeled_float_value("Alpha", p.alpha);
		} else if constexpr (std::is_same_v<T, AnimTreeBlendMaskedParams>) {
			changed |= draw_labeled_float_value("Alpha", p.alpha);
			changed |= ImGui::Checkbox("Meshspace Blend", &p.meshspace_blend);
		} else if constexpr (std::is_same_v<T, AnimTreeAddParams>) {
			changed |= draw_labeled_float_value("Alpha", p.alpha);
		} else if constexpr (std::is_same_v<T, AnimTreeMakeAdditiveParams>) {
			// masked_bones is edited entirely by the BoneMaskEditor below.
		} else if constexpr (std::is_same_v<T, AnimTreeIkParams>) {
			changed |= draw_labeled_bone("Bone", p.bone_name, skel);
			changed |= draw_labeled_bone("Other Bone", p.other_bone, skel);
			changed |= ImGui::Checkbox("Take Rotation Of Other", &p.take_rotation_of_other);
			changed |= ImGui::Checkbox("IK In Bone Space", &p.ik_in_bone_space);
			changed |= draw_labeled_bone("Pole Bone", p.pole_bone, skel);
			changed |= ImGui::Checkbox("Pole In Bone Space", &p.pole_in_bone_space);
			changed |= ImGui::Checkbox("Allow Stretching", &p.allow_stretching);
			if (p.allow_stretching) {
				ImGui::Indent();
				changed |= ImGui::DragFloat("Max Stretch Scale", &p.max_stretch_scale, 0.01f);
				changed |= ImGui::DragFloat("Start Stretch Ratio", &p.start_stretch_ratio, 0.01f);
				ImGui::Unindent();
			}
			changed |= draw_labeled_float_value("Alpha", p.alpha);
			changed |= draw_labeled_vec3_value("Target", p.target);
			changed |= draw_labeled_vec3_value("Pole", p.pole);
		} else if constexpr (std::is_same_v<T, AnimTreeModifyBoneParams>) {
			changed |= draw_labeled_bone("Bone", p.bone_name, skel);
			changed |= draw_labeled_modifybonetype("Translation Mode", p.translation_mode);
			if (p.translation_mode != ModifyBoneType::None)
				changed |= draw_labeled_vec3_value("Translation", p.translation);
			changed |= draw_labeled_modifybonetype("Rotation Mode", p.rotation_mode);
			if (p.rotation_mode != ModifyBoneType::None)
				changed |= draw_labeled_vec3_value("Rotation", p.rotation);
			changed |= draw_labeled_modifybonetype("Scale Mode", p.scale_mode);
			if (p.scale_mode != ModifyBoneType::None)
				changed |= draw_labeled_vec3_value("Scale", p.scale);
			changed |= draw_labeled_float_value("Alpha", p.alpha);
		} else if constexpr (std::is_same_v<T, AnimTreeCopyBoneParams>) {
			changed |= draw_labeled_bone("Source Bone", p.source_bone, skel);
			changed |= draw_labeled_bone("Target Bone", p.target_bone, skel);
			changed |= draw_labeled_bool_value("Copy Translation", p.copy_translation);
			changed |= draw_labeled_bool_value("Copy Rotation", p.copy_rotation);
			changed |= draw_labeled_bool_value("Copy Scale", p.copy_scale);
			changed |= ImGui::Checkbox("Copy Bonespace", &p.copy_bonespace);
			changed |= draw_labeled_float_value("Alpha", p.alpha);
		} else if constexpr (std::is_same_v<T, AnimTreeSlotParams>) {
			changed |= draw_labeled_stringname("Slot Name", p.slot_name);
			changed |= ImGui::Checkbox("Update Children When Playing", &p.update_children_when_playing);
		} else if constexpr (std::is_same_v<T, AnimTreeBlendByIntParams>) {
			changed |= draw_labeled_int_value("Integer", p.integer);
			changed |= draw_labeled_easing("Easing", p.easing);
			changed |= ImGui::DragFloat("Blend Duration", &p.blend_duration, 0.01f);
		} else if constexpr (std::is_same_v<T, AnimTreeSaveCachedPoseParams>) {
			changed |= draw_labeled_stringname("Cache Name", p.cache_name);
		} else if constexpr (std::is_same_v<T, AnimTreeUseCachedPoseParams>) {
			changed |= draw_labeled_stringname("Cache Name", p.cache_name);
		}
	}, selected_->params);
	if (changed)
		mark_dirty();

	if (selected_->kind == AnimTreeNodeKind::BlendMasked) {
		auto& p = std::get<AnimTreeBlendMaskedParams>(selected_->params);
		if (working_skeleton_.get() && working_skeleton_.get()->get_skel()) {
			ImGui::SeparatorText("Bone Mask");
			if (bone_mask_editor_.draw(*working_skeleton_.get()->get_skel(), p.mask, p.default_weight))
				mark_dirty();
		} else {
			ImGui::TextDisabled("Assign a skeleton above to edit the bone mask.");
		}
	} else if (selected_->kind == AnimTreeNodeKind::MakeAdditive) {
		auto& p = std::get<AnimTreeMakeAdditiveParams>(selected_->params);
		if (working_skeleton_.get() && working_skeleton_.get()->get_skel()) {
			ImGui::SeparatorText("Masked Bones");
			if (bone_mask_editor_.draw_binary(*working_skeleton_.get()->get_skel(), p.masked_bones))
				mark_dirty();
		} else {
			ImGui::TextDisabled("Assign a skeleton above to edit masked bones.");
		}
	}
}

void AnimTreeEditor::draw_graph_tab() {
	draw_toolbar();
	ImGui::Separator();
	float avail_h = ImGui::GetContentRegionAvail().y;
	float avail_w = ImGui::GetContentRegionAvail().x;
	const float splitter_w = 6.f;
	prop_panel_width_ = std::clamp(prop_panel_width_, 260.f, std::max(260.f, avail_w - 200.f - splitter_w));

	ImGui::BeginChild("##at_canvas_region", ImVec2(avail_w - prop_panel_width_ - splitter_w, avail_h), true);
	draw_graph_canvas();
	ImGui::EndChild();
	ImGui::SameLine();

	ImGui::PushID("##at_splitter");
	ImGui::InvisibleButton("##split", ImVec2(splitter_w, avail_h));
	if (ImGui::IsItemActive())
		prop_panel_width_ -= ImGui::GetIO().MouseDelta.x;
	if (ImGui::IsItemHovered() || ImGui::IsItemActive())
		ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
	ImGui::PopID();
	ImGui::SameLine();

	ImGui::BeginChild("##at_prop_region", ImVec2(0, avail_h), true);
	draw_property_panel();
	ImGui::EndChild();
}

void AnimTreeEditor::draw_overview_tab() {
	std::vector<OverviewVar> vars, slots, syncgroups;
	if (working_root_)
		collect_from_node(*working_root_, vars, slots, syncgroups);
	for (auto& e : working_cached_roots_)
		if (e.second)
			collect_from_node(*e.second, vars, slots, syncgroups);
	for (auto& o : working_orphans_)
		if (o)
			collect_from_node(*o, vars, slots, syncgroups);

	ImGui::TextWrapped("Cross-reference of every variable/slot/sync-group name used across the whole "
						"tree (main root + all cached pose roots + unattached nodes) -- a typo'd name shows up as its own "
						"row instead of silently merging with the intended one.");
	ImGui::Spacing();
	draw_overview_table("##ov_vars", "Variables", vars, true);
	draw_overview_table("##ov_slots", "Slots", slots, false);
	draw_overview_table("##ov_sync", "Sync Groups", syncgroups, false);
}

void AnimTreeEditor::imgui_draw() {
	// Snapshot "before" state at the start of every frame that isn't already mid-edit-session --
	// this is what makes it "before" rather than "after": it's taken before any of this frame's
	// widgets have had a chance to mutate anything. If nothing ends up changing this frame, it's
	// simply overwritten by the next idle frame's snapshot, cheaply.
	if (!edit_session_active_)
		session_before_snapshot_ = capture_snapshot();

	ImGuiIO& io = ImGui::GetIO();
	if (!io.WantTextInput && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false))
		command_mgr_->undo();

	if (ImGui::BeginTabBar("##animtree_tabs")) {
		if (ImGui::BeginTabItem("Graph")) {
			draw_graph_tab();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Overview")) {
			draw_overview_tab();
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}

	// Close the session once nothing's still being interacted with (mouse released, no item has
	// keyboard focus) -- so a whole drag or a burst of typing lands as one undo step instead of one
	// per frame. Mirrors EdPropertyGrid.cpp's before/after session capture for entity properties.
	if (edit_session_active_ && !ImGui::IsAnyMouseDown() && !ImGui::IsAnyItemActive()) {
		command_mgr_->add_command(new AnimTreeEditCommand(*this, std::move(session_before_snapshot_)));
		command_mgr_->execute_queued_commands();
		edit_session_active_ = false;
	}
}
#endif
