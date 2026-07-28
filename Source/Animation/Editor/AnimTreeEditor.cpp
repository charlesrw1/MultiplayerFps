#ifdef EDITOR_BUILD
#include "AnimTreeEditor.h"
#include "Assets/AssetDatabase.h"
#include "Assets/AssetRegistry.h"
#include "Render/Model.h"
#include "Animation/SkeletonData.h"
#include "Framework/FnFactory.h"
#include "AssetCompile/Someutils.h"
#include "imgui.h"
#include <cstring>
#include <type_traits>

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

} // namespace

AnimTreeEditor::AnimTreeEditor() = default;
AnimTreeEditor::~AnimTreeEditor() = default;

void AnimTreeEditor::set_asset(const std::string& asset_path) {
	asset_path_ = asset_path;
	asset_ = g_assets.find<AnimTreeAsset>(asset_path).get();
	revert_from_disk();
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
	collapsed_.clear();
	active_root_index_ = -1;

	if (!asset_) {
		working_root_ = std::make_unique<AnimTreeNode>(AnimTreeNodeKind::BindPose);
		working_cached_roots_.clear();
		working_skeleton_ = AssetPtr<Model>{};
		dirty_ = false;
		return;
	}
	working_root_ = asset_->root ? asset_->root->clone() : std::make_unique<AnimTreeNode>(AnimTreeNodeKind::BindPose);
	working_cached_roots_.clear();
	for (auto& e : asset_->cached_pose_roots)
		working_cached_roots_.emplace_back(e.first, e.second->clone());
	working_skeleton_ = asset_->skeleton;
	dirty_ = false;
}

void AnimTreeEditor::apply_to_disk() {
	if (!asset_)
		return;
	asset_->root = working_root_->clone();
	asset_->cached_pose_roots.clear();
	for (auto& e : working_cached_roots_)
		asset_->cached_pose_roots.emplace_back(e.first, e.second->clone());
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

void AnimTreeEditor::move_node_before(AnimTreeNode* dragged, AnimTreeNode* new_parent, AnimTreeNode* before_node) {
	AnimTreeNode* root = active_root();
	AnimTreeNode* old_parent = nullptr;
	int old_index = -1;
	if (!find_parent(root, dragged, old_parent, old_index))
		return; // dragged is the active root itself -- it has no siblings to reorder among

	// dragged is about to leave old_parent's list, so when reordering within the same parent,
	// its own slot doesn't count against the capacity check.
	int effective_count = (int)new_parent->children.size() - (old_parent == new_parent ? 1 : 0);
	int arity = get_arity_for_kind(new_parent->kind);
	if (arity >= 0 && effective_count >= arity)
		return;

	std::unique_ptr<AnimTreeNode> moved = std::move(old_parent->children[old_index]);
	old_parent->children.erase(old_parent->children.begin() + old_index);

	int insert_at = (int)new_parent->children.size();
	for (int i = 0; i < (int)new_parent->children.size(); i++) {
		if (new_parent->children[i].get() == before_node) {
			insert_at = i;
			break;
		}
	}
	new_parent->children.insert(new_parent->children.begin() + insert_at, std::move(moved));
	mark_dirty();
}

void AnimTreeEditor::draw_reorder_gap(AnimTreeNode* before_node, AnimTreeNode* parent) {
	ImGui::PushID("gap");
	ImGui::InvisibleButton("##reorder_gap", ImVec2(-1, 6.f));
	bool dragging_over = ImGui::BeginDragDropTarget();
	if (dragging_over) {
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("AnimTreeNodeDrag")) {
			AnimTreeNode* dragged = *(AnimTreeNode**)payload->Data;
			if (dragged && dragged != before_node && !is_descendant(dragged, before_node))
				move_node_before(dragged, parent, before_node);
		}
		ImGui::EndDragDropTarget();
	}
	if (dragging_over) {
		ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
		float mid_y = (mn.y + mx.y) * 0.5f;
		ImGui::GetWindowDrawList()->AddLine(ImVec2(mn.x, mid_y), ImVec2(mx.x, mid_y), IM_COL32(90, 170, 255, 255), 2.5f);
	}
	ImGui::PopID();
}

void AnimTreeEditor::draw_nest_drop_target(AnimTreeNode* node) {
	if (!ImGui::BeginDragDropTarget())
		return;
	if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("AnimTreeNodeDrag")) {
		AnimTreeNode* dragged = *(AnimTreeNode**)payload->Data;
		if (dragged && dragged != node && !is_descendant(dragged, node)) {
			AnimTreeNode* root = active_root();
			AnimTreeNode* old_parent = nullptr;
			int old_index = -1;
			if (find_parent(root, dragged, old_parent, old_index)) {
				int arity = get_arity_for_kind(node->kind);
				if (arity < 0 || (int)node->children.size() < arity) {
					std::unique_ptr<AnimTreeNode> moved = std::move(old_parent->children[old_index]);
					old_parent->children.erase(old_parent->children.begin() + old_index);
					node->children.push_back(std::move(moved));
					mark_dirty();
				}
			}
		}
	}
	ImGui::EndDragDropTarget();
}

void AnimTreeEditor::draw_context_menu(AnimTreeNode* node, AnimTreeNode* parent) {
	if (!ImGui::BeginPopupContextItem("##node_ctx"))
		return;

	int arity = get_arity_for_kind(node->kind);
	bool can_add = (arity < 0) || ((int)node->children.size() < arity);
	if (ImGui::BeginMenu("Add Child", can_add)) {
		for (int k = 0; k <= (int)AnimTreeNodeKind::UseCachedPose; k++) {
			auto kind = (AnimTreeNodeKind)k;
			if (ImGui::MenuItem(get_kind_display_name(kind))) {
				node->children.push_back(std::make_unique<AnimTreeNode>(kind));
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
				if (new_arity >= 0 && (int)node->children.size() > new_arity) {
					for (size_t i = new_arity; i < node->children.size(); i++)
						collapsed_.erase(node->children[i].get());
					node->children.resize(new_arity);
				}
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
				insert_parent_above(node, parent, kind);
			}
		}
		ImGui::EndMenu();
	}

	bool has_parent = parent != nullptr;
	if (ImGui::MenuItem("Duplicate", nullptr, false, has_parent)) {
		int p_arity = get_arity_for_kind(parent->kind);
		if (p_arity < 0 || (int)parent->children.size() < p_arity) {
			for (int i = 0; i < (int)parent->children.size(); i++) {
				if (parent->children[i].get() == node) {
					parent->children.insert(parent->children.begin() + i + 1, node->clone());
					break;
				}
			}
			mark_dirty();
		}
	}
	if (ImGui::MenuItem("Delete", nullptr, false, has_parent)) {
		for (int i = 0; i < (int)parent->children.size(); i++) {
			if (parent->children[i].get() == node) {
				if (selected_ == node) { selected_ = nullptr; clip_pg_owner_ = nullptr; }
				collapsed_.erase(node);
				parent->children.erase(parent->children.begin() + i);
				break;
			}
		}
		mark_dirty();
		ImGui::EndPopup();
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

	ImGui::EndPopup();
}

void AnimTreeEditor::insert_parent_above(AnimTreeNode* node, AnimTreeNode* parent, AnimTreeNodeKind new_kind) {
	auto new_node = std::make_unique<AnimTreeNode>(new_kind);
	if (!parent) {
		// node is the active root: draw_tree() only ever calls draw_node_row(root, nullptr, 0),
		// so parent == nullptr implies node == active_root().
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

void AnimTreeEditor::draw_node_row(AnimTreeNode* node, AnimTreeNode* parent, int depth) {
	bool has_children = !node->children.empty();
	bool row_collapsed = collapsed_.count(node) != 0;

	ImGui::TableNextRow();
	ImGui::TableNextColumn();
	ImGui::PushID(node);

	// Reorder target: drop above this row to make the dragged node its immediately preceding
	// sibling. Root has no siblings, so it gets no gap.
	if (parent)
		draw_reorder_gap(node, parent);

	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_OpenOnArrow;
	if (!has_children)
		flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
	if (selected_ == node)
		flags |= ImGuiTreeNodeFlags_Selected;
	if (has_children)
		ImGui::SetNextItemOpen(!row_collapsed, ImGuiCond_Always);

	bool open = ImGui::TreeNodeEx(format_node_label(*node).c_str(), flags);
	if (has_children && ImGui::IsItemToggledOpen()) {
		if (row_collapsed)
			collapsed_.erase(node);
		else
			collapsed_.insert(node);
		row_collapsed = !row_collapsed;
	}
	if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen())
		selected_ = node;

	if (parent && ImGui::BeginDragDropSource()) {
		ImGui::SetDragDropPayload("AnimTreeNodeDrag", &node, sizeof(AnimTreeNode*));
		ImGui::TextUnformatted(format_node_label(*node).c_str());
		ImGui::EndDragDropSource();
	}
	// Dropping on the row body itself reparents the dragged node as this node's last child
	// (as opposed to draw_reorder_gap above, which reorders it as a sibling).
	draw_nest_drop_target(node);
	draw_context_menu(node, parent);

	ImGui::TableNextColumn();
	ImGui::TextDisabled("%d", depth);

	ImGui::PopID();

	// TreeNodeEx only pushes an ID-stack entry when it returns true (open); with
	// ImGuiTreeNodeFlags_OpenOnArrow a collapsed/leaf row returns false and pushes nothing, so a
	// matching TreePop() would pop a frame that was never pushed.
	if (has_children && open) {
		for (auto& c : node->children)
			draw_node_row(c.get(), node, depth + 1);
		ImGui::TreePop();
	}
}

void AnimTreeEditor::draw_tree() {
	AnimTreeNode* root = active_root();
	if (!root)
		return;

	ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
							ImGuiTableFlags_ScrollY;
	if (ImGui::BeginTable("##at_node_table", 2, flags)) {
		ImGui::TableSetupColumn("Node", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Depth", ImGuiTableColumnFlags_WidthFixed, 55.f);
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableHeadersRow();
		draw_node_row(root, nullptr, 0);
		ImGui::EndTable();
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
	ImGui::BeginChild("##at_tree_region", ImVec2(0, avail_h * 0.55f), true);
	draw_tree();
	ImGui::EndChild();
	ImGui::BeginChild("##at_prop_region", ImVec2(0, 0), true);
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

	ImGui::TextWrapped("Cross-reference of every variable/slot/sync-group name used across the whole "
						"tree (main root + all cached pose roots) -- a typo'd name shows up as its own "
						"row instead of silently merging with the intended one.");
	ImGui::Spacing();
	draw_overview_table("##ov_vars", "Variables", vars, true);
	draw_overview_table("##ov_slots", "Slots", slots, false);
	draw_overview_table("##ov_sync", "Sync Groups", syncgroups, false);
}

void AnimTreeEditor::imgui_draw() {
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
}
#endif
