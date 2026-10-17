#ifdef EDITOR_BUILD
#include "PropertyEd.h"
#include "imgui.h"
#include "Assets/AssetDatabase.h"
#include "MyImguiLib.h"
#include "Render/Texture.h"
#include "StructReflection.h"
#include "FnFactory.h"
#include "EditorTheme.h"

// Forward declarations needed by rebuild_child_rows
IGridRow* create_row(const FnFactory<IPropertyEditor>& factory, IGridRow* parent, const PropertyRef& target,
					 int row_idx, uint32_t property_flag_mask);

static uint32_t color32_to_uint_arr(Color32 color) {
	return *(uint32_t*)&color;
}

// ---- ArrayRow ----

ArrayRow::ArrayRow(const FnFactory<IPropertyEditor>& factory, IGridRow* parent, const PropertyRef& list, int row_idx,
				   uint32_t property_flag_mask)
	: IGridRow(parent, row_idx), list(list), prop(&list.info()), property_flag_mask(property_flag_mask),
	  factory(factory) {
	ASSERT(list.root && prop->type == core_type_id::List && !list.path.leaf().is_element());
	rebuild_child_rows();
}

int ArrayRow::get_size() {
	return list.list_size();
}

void ArrayRow::hook_update_pre_tree_node() {
	ASSERT(prop);
	if (set_next_state == next_state::hidden)
		ImGui::SetNextItemOpen(false);
	else if (set_next_state == next_state::visible)
		ImGui::SetNextItemOpen(true);
}

bool ArrayRow::are_any_nodes_open() {
	for (int i = 0; i < child_rows.size(); i++) {
		if (child_rows[i]->expanded)
			return true;
	}
	return false;
}


bool ArrayRow::internal_update() {
	bool ret = false;

	ImGui::TextColored(ImVec4(0.5, 0.5, 0.5, 1.0), "elements: %d", get_size());

	{
		auto addimg   = g_assets.find<Texture>("eng/icons/add_circle.png");
		auto trashimg = g_assets.find<Texture>("eng/icons/delete.png");

		ImGui::SameLine();
		ImGui::PushStyleColor(ImGuiCol_Button, 0);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, color32_to_uint_arr({245, 242, 242, 55}));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, 0);
#pragma warning(disable : 4312)
		if (my_imgui_image_button(addimg, 13))
			pending_add = true;
		ImGui::SameLine();
		ImGui::BeginDisabled(child_rows.empty());
		if (my_imgui_image_button(trashimg, 13))
			pending_clear = true;
		ImGui::EndDisabled();
#pragma warning(default : 4312)
		ImGui::PopStyleColor(3);
	}

	if (pending_add) {
		pending_add = false;
		clear_children();
		list.list_resize(get_size() + 1);
		rebuild_child_rows();
		ret = true;
	}
	if (pending_clear) {
		pending_clear = false;
		clear_children();
		list.list_resize(0);
		ret = true;
	}

	for (auto& cmd : commands) {
		switch (cmd.command) {
		case Delete: {
			const int size = get_size();
			if (cmd.index < size) {
				list.list_move(cmd.index, size - 1);
				list.list_resize(size - 1);
			}
		} break;

		case Reorder: {
			const int size = get_size();
			if (cmd.index < size && cmd.index2 < size)
				list.list_move(cmd.index, cmd.index2);
		} break;
		}
	}

	if (!commands.empty()) {
		ret = true;
		clear_children();
		rebuild_child_rows();
		commands.clear();
	}
	return ret;
}

void ArrayRow::draw_header(float header_ofs) {
	ASSERT(prop);
	ImGui::Dummy(ImVec2(header_ofs, 0));
	ImGui::SameLine();

	ImGui::PushStyleColor(ImGuiCol_Header, 0);
	ImGui::PushStyleColor(ImGuiCol_HeaderActive, 0);
	ImGui::PushStyleColor(ImGuiCol_HeaderHovered, 0);

	const char* name = prop->name;
	if (!name_override.empty())
		name = name_override.c_str();

	if (g_prop_regular_font) ImGui::PushFont(g_prop_regular_font);
	expanded = ImGui::TreeNodeEx(name, ImGuiTreeNodeFlags_DefaultOpen);
	if (g_prop_regular_font) ImGui::PopFont();
	draw_property_tooltip(prop);
	if (expanded)
		ImGui::TreePop();
	ImGui::PopStyleColor(3);

}

void ArrayRow::rebuild_child_rows() {
	ASSERT(child_rows.size() == 0); // clearing done before
	const int count = get_size();
	for (int i = 0; i < count; i++) {
		IGridRow* row = create_row(factory, this, PropertyRef{list.root, list.path.element(i)}, i, property_flag_mask);
		if (!row)
			continue;
		row->set_name_override(string_format("[ %d ]", i));
		child_rows.push_back(std::unique_ptr<IGridRow>(row));
	}
}
#endif
