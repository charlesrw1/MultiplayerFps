#ifdef EDITOR_BUILD
#include "PropertyEd.h"
#include "imgui.h"
#include "Assets/AssetDatabase.h"
#include "MyImguiLib.h"
#include "Render/Texture.h"
#include "StructReflection.h"
#include "FnFactory.h"

// Forward declarations
IGridRow* create_row(const FnFactory<IPropertyEditor>& factory, IGridRow* parent, PropertyInfo* prop, void* inst,
					 int row_idx, uint32_t property_flag_mask);

static uint32_t color32_to_uint_grp(Color32 color) {
	return *(uint32_t*)&color;
}

static std::string type_to_string_grp(const PropertyInfo* p);

static void draw_tooltip_grp(const PropertyInfo* prop) {
	ASSERT(prop);
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
		ImGui::BeginTooltip();
		ImGui::Text("%s", type_to_string_grp(prop).c_str());
		if (*prop->tooltip) {
			ImGui::TextColored(ImVec4(0.8, 0.8, 0.8, 1), "%s", prop->tooltip);
		}
		ImGui::EndTooltip();
	}
}

static std::string type_to_string_grp(const PropertyInfo* p) {
	ASSERT(p);
	switch (p->type) {
	case core_type_id::Bool:
		return "Bool";
	case core_type_id::Int8:
		return "Int8";
	case core_type_id::Int16:
		return "Int16";
	case core_type_id::Int32:
		return "Int32";
	case core_type_id::Int64:
		return "Int64";
	case core_type_id::Float:
		return "Float";
	case core_type_id::ActualStruct: {
		return p->struct_type->structname;
	} break;
	case core_type_id::List: {
		if (!p->list_ptr->get_is_new_list_type())
			return "vector";
		return "vector<" + type_to_string_grp(p->list_ptr->get_property()) + ">";
	} break;
	case core_type_id::StdString:
		return "String";
	case core_type_id::Quat:
		return "Quat";
	case core_type_id::Vec3:
		return "Vec3";
	case core_type_id::Enum8:
	case core_type_id::Enum16:
	case core_type_id::Enum32:
		return p->enum_type->name;
	case core_type_id::Struct: {
		return p->custom_type_str;
	}
	default:
		break;
	}
	return "Unknown Type";
}

// ---- GroupRow ----

GroupRow::GroupRow(const FnFactory<IPropertyEditor>& factory, IGridRow* parent, void* instance,
				   const PropertyInfo* info, int row_idx, uint32_t property_flag_mask)
	: IGridRow(parent, row_idx), property(info), inst(instance) {
	ASSERT(instance && info);
	assert(info->type == core_type_id::ActualStruct);
	proplist = info->struct_type->properties;
	auto list = proplist;
	for (int i = 0; i < list->count; i++) {
		auto& prop = list->list[i];
		if (!prop.can_edit() || prop.attrs.hidden)
			continue;
		bool passed_mask_check = (prop.flags & property_flag_mask) != 0;
		if (!passed_mask_check)
			continue;

		auto row = create_row(factory, this, &prop, inst, -1, property_flag_mask);
		if (row)
			child_rows.push_back(std::unique_ptr<IGridRow>(row));
	}
	name = info->name; // the variable name
	if (row_idx != -1) {
		name = string_format("[ %d ]", row_idx);
	}
}

GroupRow::GroupRow(const FnFactory<IPropertyEditor>& factory, IGridRow* parent, void* instance,
				   const PropertyInfoList* list, int row_idx, uint32_t property_flag_mask)
	: IGridRow(parent, row_idx), proplist(list), inst(instance) {
	ASSERT(instance && list);

	for (int i = 0; i < list->count; i++) {
		auto& prop = list->list[i];
		if (!prop.can_edit() || prop.attrs.hidden)
			continue;
		bool passed_mask_check = (prop.flags & property_flag_mask) != 0;
		if (!passed_mask_check)
			continue;

		auto row = create_row(factory, this, &prop, inst, -1, property_flag_mask);
		if (row)
			child_rows.push_back(std::unique_ptr<IGridRow>(row));
	}

	name = list->type_name;

	if (row_idx != -1) {
		name = string_format("[ %d ]", row_idx);
	}
}

bool GroupRow::draw_children() {
	return !passthrough_to_child();
}


void GroupRow::draw_header(float ofs) {
	ASSERT(proplist || property);
	ImGui::Dummy(ImVec2(ofs, 0));
	ImGui::SameLine();

	bool is_array_item = (row_index != -1);
	ArrayRow* array_ = is_array_item ? (ArrayRow*)parent : nullptr;
	bool can_edit = array_ && (!array_->header || array_->header->can_edit_array());

	// Drag handle — all array items when editable
	if (is_array_item && can_edit) {
		ImGui::PushStyleColor(ImGuiCol_Button, 0);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, color32_to_uint_grp({255, 255, 255, 30}));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, 0);
		ImGui::Button("=##drag", ImVec2(14, 0));
		ImGui::PopStyleColor(3);

		if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
			ImGui::SetDragDropPayload("PROP_ARRAY_ITEM", &row_index, sizeof(int));
			ImGui::Text("Item %d", row_index);
			ImGui::EndDragDropSource();
		}
		ImGui::SameLine(0, 4);
	}

	ImGui::PushStyleColor(ImGuiCol_Header, 0);
	ImGui::PushStyleColor(ImGuiCol_HeaderActive, 0);
	ImGui::PushStyleColor(ImGuiCol_HeaderHovered, 0);

	bool has_drawn = false;
	if (is_array_item) {
		array_->hook_update_pre_tree_node();

		if (array_->header) {
			expanded = array_->header->imgui_draw_header(row_index);
			has_drawn = true;
		}
	}
	if (!has_drawn) {
		uint32_t flags = (row_index == -1) ? ImGuiTreeNodeFlags_DefaultOpen : 0;
		expanded = ImGui::TreeNodeEx(name.c_str(), flags);
		if (property)
			draw_tooltip_grp(property);
		if (expanded)
			ImGui::TreePop();
	}

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
		auto trash1 = g_assets.find<Texture>("eng/icon/trash1.png");
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

	bool is_array = row_index != -1;
	if (!expanded && is_array) {
		ArrayRow* array_ = (ArrayRow*)parent;
		if (array_->header) {
			array_->header->imgui_draw_closed_body(row_index);
		}
	}
	return ret;
}
#endif
