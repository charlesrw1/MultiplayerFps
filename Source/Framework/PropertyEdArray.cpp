#ifdef EDITOR_BUILD
#include "PropertyEd.h"
#include "imgui.h"
#include "Assets/AssetDatabase.h"
#include "MyImguiLib.h"
#include "Render/Texture.h"
#include "StructReflection.h"
#include "FnFactory.h"

// Forward declarations needed by rebuild_child_rows
IGridRow* create_row(const FnFactory<IPropertyEditor>& factory, IGridRow* parent, PropertyInfo* prop, void* inst,
					 int row_idx, uint32_t property_flag_mask);

static uint32_t color32_to_uint_arr(Color32 color) {
	return *(uint32_t*)&color;
}

// ---- ArrayRow ----

Factory<std::string, IArrayHeader>& IArrayHeader::get_factory() {
	static Factory<std::string, IArrayHeader> inst;
	return inst;
}

ArrayRow::ArrayRow(const FnFactory<IPropertyEditor>& factory, IGridRow* parent, void* instance, PropertyInfo* prop,
				   int row_idx, uint32_t property_flag_mask)
	: IGridRow(parent, row_idx), instance(instance), prop(prop), property_flag_mask(property_flag_mask),
	  factory(factory) {
	ASSERT(prop && instance);
	header = std::unique_ptr<IArrayHeader>(IArrayHeader::get_factory().createObject(prop->custom_type_str));
	if (header)
		header->post_construct(instance, prop);

	rebuild_child_rows();
}

int ArrayRow::get_size() {
	ASSERT(prop && instance);
	return prop->list_ptr->get_size(prop->get_ptr(instance));
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

bool ArrayRow::draw_row_controls() {
	ASSERT(prop && instance);
	if (header && !header->can_edit_array())
		return false;
	bool ret = false;
	auto trashimg = g_assets.find<Texture>("eng/icon/trash.png");
	auto addimg = g_assets.find<Texture>("eng/icon/plus.png");

	auto visible_icon = g_assets.find<Texture>("eng/icon/visible.png");
	auto hidden_icon = g_assets.find<Texture>("eng/icon/hidden.png");

	bool are_any_open = are_any_nodes_open();

	uint8_t* list_instance_ptr = prop->get_ptr(instance);

	ImGui::PushStyleColor(ImGuiCol_Button, 0);
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, color32_to_uint_arr({245, 242, 242, 55}));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, 0);

#pragma warning(disable : 4312)
	if (my_imgui_image_button(addimg, 16)) {
		ret = true;
		clear_children();
		prop->list_ptr->resize(list_instance_ptr, prop->list_ptr->get_size(list_instance_ptr) +
													  1); // might invalidate childrens ptrs, so refresh
		rebuild_child_rows();
	}

	ImGui::SameLine();

	if (!header || header->has_delete_all()) {
		ImGui::BeginDisabled(child_rows.empty());
		if (my_imgui_image_button(trashimg, 16)) {
			ret = true;
			clear_children();
			prop->list_ptr->resize(list_instance_ptr, 0);
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
	}

	set_next_state = next_state::keep;

	if (are_any_open) {
		if (my_imgui_image_button(visible_icon, 16)) {
			set_next_state = next_state::hidden;
		}
	} else {
		if (my_imgui_image_button(hidden_icon, 16)) {
			set_next_state = next_state::visible;
		}
	}
#pragma warning(default : 4312)

	ImGui::PopStyleColor(3);

	return ret;
}

bool ArrayRow::internal_update() {
	ASSERT(prop && instance);
	bool ret = false;

	uint8_t* list_instance_ptr = prop->get_ptr(instance);
	{
		int count = get_size();

		ImGui::TextColored(ImVec4(0.5, 0.5, 0.5, 1.0), "elements: %d", count);
	}

	if (header && !header->can_edit_array())
		commands.clear();

	for (int i = 0; i < commands.size(); i++) {
		switch (commands[i].command) {
		case Delete: {

			// do this here so destructors dont access stale pointers
			child_rows.erase(child_rows.begin() + i);

			int index_to_delete = commands[i].index;
			int size = get_size();
			for (int i = index_to_delete; i < size - 1; i++) {

				prop->list_ptr->swap_elements(list_instance_ptr, i, i + 1);
			}
			prop->list_ptr->resize(list_instance_ptr, size - 1);

		} break;

		case Movedown: {
			int size = get_size();

			int index = commands[i].index;
			if (index < size - 1) {
				prop->list_ptr->swap_elements(list_instance_ptr, index, index + 1);
			}

		} break;

		case Moveup: {
			int index = commands[i].index;
			if (index > 0) {
				prop->list_ptr->swap_elements(list_instance_ptr, index - 1, index);
			}

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

static std::string type_to_string_arr(const PropertyInfo* p) {
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
		return "vector<" + type_to_string_arr(p->list_ptr->get_property()) + ">";
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

static void draw_tooltip_arr(const PropertyInfo* prop) {
	ASSERT(prop);
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
		ImGui::BeginTooltip();
		ImGui::Text("%s", type_to_string_arr(prop).c_str());
		if (*prop->tooltip) {
			ImGui::TextColored(ImVec4(0.8, 0.8, 0.8, 1), "%s", prop->tooltip);
		}
		ImGui::EndTooltip();
	}
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

	expanded = ImGui::TreeNodeEx(name, ImGuiTreeNodeFlags_DefaultOpen);
	draw_tooltip_arr(prop);
	if (expanded)
		ImGui::TreePop();
	ImGui::PopStyleColor(3);
}

void ArrayRow::rebuild_child_rows() {
	ASSERT(child_rows.size() == 0); // clearing done before
	ASSERT(prop->type == core_type_id::List);

	IListCallback* list = prop->list_ptr;
	int count = list->get_size(prop->get_ptr(instance));

	for (int i = 0; i < count; i++) {
		IGridRow* row = nullptr;
		if (list->get_is_new_list_type()) { // new way
			void* inst = list->get_index(prop->get_ptr(instance), i);
			row = create_row(factory, this, (PropertyInfo*)list->get_property(), inst, i, property_flag_mask);
		} else { // old way
			const PropertyInfoList* struct_ = list->props_in_list;
			row = new GroupRow(factory, this, list->get_index(prop->get_ptr(instance), i), struct_, i,
							   property_flag_mask);
		}
		assert(row);
		child_rows.push_back(std::unique_ptr<IGridRow>(row));
	}
}
#endif
