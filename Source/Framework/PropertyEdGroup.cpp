#ifdef EDITOR_BUILD
#include "PropertyEd.h"
#include "imgui.h"
#include "Assets/AssetDatabase.h"
#include "MyImguiLib.h"
#include "Render/Texture.h"
#include "StructReflection.h"
#include "FnFactory.h"
#include "EditorTheme.h"

IGridRow* create_row(const FnFactory<IPropertyEditor>& factory, IGridRow* parent, const PropertyRef& target,
					 int row_idx, uint32_t property_flag_mask);

static uint32_t color32_to_uint_grp(Color32 color) {
	return *(uint32_t*)&color;
}

// ---- GroupRow ----

GroupRow::GroupRow(const FnFactory<IPropertyEditor>& factory, IGridRow* parent, const PropertyRef& base, int row_idx,
				   uint32_t property_flag_mask)
	: IGridRow(parent, row_idx), property(&base.info()) {
	ASSERT(base.root && property->type == core_type_id::ActualStruct);
	proplist = property->struct_type->properties;
	name = row_idx != -1 ? string_format("[ %d ]", row_idx) : property->name;
	add_rows(factory, base, property_flag_mask);
}

GroupRow::GroupRow(const FnFactory<IPropertyEditor>& factory, IGridRow* parent, void* root,
				   const PropertyInfoList* list, int row_idx, uint32_t property_flag_mask)
	: IGridRow(parent, row_idx), proplist(list) {
	ASSERT(root && list);
	name = row_idx != -1 ? string_format("[ %d ]", row_idx) : list->type_name;
	add_rows(factory, PropertyRef{root}, property_flag_mask);
}

void GroupRow::add_rows(const FnFactory<IPropertyEditor>& factory, const PropertyRef& base,
						uint32_t property_flag_mask) {
	if (!proplist)
		return;
	for (int i = 0; i < proplist->count; i++) {
		const PropertyInfo& prop = proplist->list[i];
		if (!prop.can_edit() || prop.attrs.hidden || !(prop.flags & property_flag_mask))
			continue;
		PropertyRef member{base.root, base.path.empty() ? PropertyPath(&prop) : base.path.child(&prop)};
		if (IGridRow* row = create_row(factory, this, member, -1, property_flag_mask))
			child_rows.push_back(std::unique_ptr<IGridRow>(row));
	}
}

bool GroupRow::draw_children() {
	return !passthrough_to_child();
}


void GroupRow::draw_header(float ofs) {
	ASSERT(proplist || property);

	// Full-row hover detection (before Dummy shifts the cursor)
	const float fh = ImGui::GetFrameHeight();
	const float row_top = ImGui::GetCursorScreenPos().y - ImGui::GetStyle().FramePadding.y;
	const float my = ImGui::GetIO().MousePos.y;
	const bool row_hov = (my >= row_top && my < row_top + fh + ImGui::GetStyle().CellPadding.y * 2.f);

	ImGui::Dummy(ImVec2(ofs, 0));
	ImGui::SameLine();

	bool is_array_item = (row_index != -1);
	ArrayRow* array_ = is_array_item ? (ArrayRow*)parent : nullptr;
	bool can_edit = array_ != nullptr;

	// Drag handle — all array items when editable
	if (is_array_item && can_edit) {
		ImU32 drag_col = row_hov ? IM_COL32(255, 255, 255, 220) : IM_COL32(255, 255, 255, 50);

		auto drag_tex = g_assets.find<Texture>("eng/icons/drag.png");
		ImGui::PushStyleColor(ImGuiCol_Button, 0);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, color32_to_uint_grp({255, 255, 255, 20}));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, 0);
		if (drag_tex) {
			ImVec2 btn_pos = ImGui::GetCursorScreenPos();
			ImGui::InvisibleButton("##drag", ImVec2(14, fh));
			// Draw as square, centered vertically
			const float ico = std::min(14.f, fh);
			const float yo = (fh - ico) * 0.5f;
			ImGui::GetWindowDrawList()->AddImage(
				ImTextureID(uint64_t(drag_tex->get_internal_render_handle())),
				ImVec2(btn_pos.x, btn_pos.y + yo), ImVec2(btn_pos.x + ico, btn_pos.y + yo + ico),
				ImVec2(0, 0), ImVec2(1, 1), drag_col);
		} else {
			ImGui::Button("=##drag", ImVec2(14, 0));
		}
		ImGui::PopStyleColor(3);

		if (ImGui::BeginDragDropSource()) {
			ImGui::SetDragDropPayload("PROP_ARRAY_ITEM", &row_index, sizeof(int));
			ImGui::Text("Item %d", row_index);
			ImGui::EndDragDropSource();
		}
		ImGui::SameLine(0, 4);
	}

	ImGui::PushStyleColor(ImGuiCol_Header, 0);
	ImGui::PushStyleColor(ImGuiCol_HeaderActive, 0);
	ImGui::PushStyleColor(ImGuiCol_HeaderHovered, 0);

	// Bold only for top-level group rows (not nested structs or array items)
	bool is_top_level = (parent == nullptr);
	if (is_top_level && g_prop_bold_font) ImGui::PushFont(g_prop_bold_font);
	if (is_array_item)
		array_->hook_update_pre_tree_node();
	{
		uint32_t flags = (row_index == -1) ? ImGuiTreeNodeFlags_DefaultOpen : 0;
		expanded = ImGui::TreeNodeEx(name.c_str(), flags);
		if (property)
			draw_property_tooltip(property);
		if (expanded)
			ImGui::TreePop();
	}
	if (is_top_level && g_prop_bold_font) ImGui::PopFont();

	// Drop target on whatever label was drawn
	if (is_array_item && can_edit) {
		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("PROP_ARRAY_ITEM")) {
				int from = *(const int*)payload->Data;
				if (from != row_index)
					array_->reorder_index(from, row_index);
			}
			ImGui::EndDragDropTarget();
		}
	}

	ImGui::PopStyleColor(3);

	// Delete button inline at end of header for all editable array items
	if (is_array_item && can_edit) {
		auto trash1 = g_assets.find<Texture>("eng/icons/delete.png");
		ImGui::SameLine();
		ImGui::PushStyleColor(ImGuiCol_Button, 0);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, color32_to_uint_grp({245, 242, 242, 55}));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, 0);
#pragma warning(disable : 4312)
		if (my_imgui_image_button(trash1, 13))
			array_->delete_index(row_index);
#pragma warning(default : 4312)
		ImGui::PopStyleColor(3);
	}
}

bool GroupRow::internal_update() {
	bool ret = false;
	if (passthrough_to_child()) {
		ASSERT(!child_rows.empty());
		auto row = (IGridRow*)child_rows[0].get();

		ret = row->internal_update();
	}
	return ret;
}
#endif
