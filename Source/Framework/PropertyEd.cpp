#ifdef EDITOR_BUILD
#include "PropertyEd.h"
#include "imgui.h"
#include "Assets/AssetDatabase.h"
#include "FnFactory.h"
#include "MyImguiLib.h"

// ---- Internal helpers ----

static uint32_t color32_to_uint(Color32 color) {
	return *(uint32_t*)&color;
}

// Forward-declared in this TU; defined below after UniquePtrRow
IGridRow* create_row(const FnFactory<IPropertyEditor>& factory, IGridRow* parent, PropertyInfo* prop, void* inst,
					 int row_idx, uint32_t property_flag_mask);

// Defined in PropertyEdWidgets.cpp
IPropertyEditor* create_ipropertyed(const FnFactory<IPropertyEditor>& factory, PropertyInfo* prop,
									void* instance, IGridRow* parent);

// ---- UniquePtrRow ----

class UniquePtrRow : public IGridRow
{
public:
	UniquePtrRow(const FnFactory<IPropertyEditor>& factory, IGridRow* parent, void* instance, PropertyInfo* info,
				 int row_idx, uint32_t prop_flag_mask)
		: IGridRow(parent, row_idx), factory(factory) {
		ASSERT(info && instance);
		assert(info->type == core_type_id::StdUniquePtr);
		flagmask = prop_flag_mask;
		ClassBase** uniquePtr = (ClassBase**)info->get_ptr(instance);

		add_children(*uniquePtr);
		type_of_base = info->class_type;
		this->info = info;
		this->inst = instance;
	}
	bool internal_update() override {
		ASSERT(info && inst);
		auto classroot = info->class_type;

		if (!type_of_base) {
			ImGui::Text("Couldnt find base class: %s\n", info->class_type->classname);
			return false;
		}
		ClassBase** uniquePtr = (ClassBase**)info->get_ptr(inst);
		bool has_update = false;
		const ClassTypeInfo* thisType = (*uniquePtr) ? &(*uniquePtr)->get_type() : nullptr;
		const char* preview = (thisType) ? thisType->classname : "<empty>";

		if (ImGui::BeginCombo("##combocalsstype", preview)) {
			auto subclasses = ClassBase::get_subclasses(type_of_base);
			if (ImGui::Selectable("<empty>", !(*uniquePtr))) {
				thisType = nullptr;
				has_update = true;
			}
			for (; !subclasses.is_end(); subclasses.next()) {

				if (subclasses.get_type()->has_allocate_func()) {
					if (ImGui::Selectable(subclasses.get_type()->classname, subclasses.get_type() == thisType)) {
						thisType = subclasses.get_type();
						has_update = true;
					}
				}
			}
			ImGui::EndCombo();
		}

		if (has_update) {
			delete *uniquePtr;
			*uniquePtr = nullptr;
			if (thisType) {
				*uniquePtr = thisType->allocate_this_type();
			}

			add_children(*uniquePtr);
		}

		return has_update;
	}
	void draw_header(float header_ofs) override {
		ASSERT(info);
		ImGui::Dummy(ImVec2(header_ofs, 0));
		ImGui::SameLine();

		ImGui::PushStyleColor(ImGuiCol_Header, 0);
		ImGui::PushStyleColor(ImGuiCol_HeaderActive, 0);
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, 0);
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5, 1, 0.3, 1));
		{

			uint32_t flags = ImGuiTreeNodeFlags_DefaultOpen;

			expanded = ImGui::TreeNodeEx(info->name, flags);
			if (expanded)
				ImGui::TreePop();
		}
		ImGui::PopStyleColor(4);
	}
	void add_children(ClassBase* b) {
		ASSERT(info);
		child_rows.clear();

		if (!b)
			return;

		auto prop_list = b->get_type().props;
		if (!prop_list)
			return;

		for (int i = 0; i < prop_list->count; i++) {
			auto& prop = prop_list->list[i];
			if (!prop.can_edit())
				continue;
			bool passed_mask_check = (prop.flags & flagmask) != 0;
			if (!passed_mask_check)
				continue;

			auto row = create_row(factory, this, &prop, b, -1, flagmask);
			if (row)
				child_rows.push_back(std::unique_ptr<IGridRow>(row));
		}
	}

	const FnFactory<IPropertyEditor>& factory;
	uint32_t flagmask = 0;
	void* inst = nullptr;
	PropertyInfo* info = nullptr;
	const ClassTypeInfo* type_of_base = nullptr;
};

// ---- create_row factory ----

#include "StructReflection.h"
IGridRow* create_row(const FnFactory<IPropertyEditor>& factory, IGridRow* parent, PropertyInfo* prop, void* inst,
					 int row_idx, uint32_t property_flag_mask) {
	ASSERT(prop && inst);
	if (prop->type == core_type_id::List) {
		ArrayRow* array_ = new ArrayRow(factory, nullptr, inst, prop, row_idx, property_flag_mask);
		return array_;
	} else if (prop->type == core_type_id::StdUniquePtr) {
		auto row = new UniquePtrRow(factory, nullptr, inst, prop, row_idx, property_flag_mask);
		return row;
	} else if (prop->type == core_type_id::ActualStruct) {
		// try finding a property editor first
		auto create = factory.create(prop->struct_type->structname);
		if (create) {
			create->post_construct_for_custom_type(inst, prop, parent);
			return new PropertyRow(create, parent, inst, prop, row_idx); // create a property row
		}
		auto row = new GroupRow(factory, parent, prop->get_ptr(inst), prop, row_idx, property_flag_mask);
		return row;
	} else {
		PropertyRow* prop_ = new PropertyRow(factory, nullptr, inst, prop, row_idx);

		if (prop_->prop_editor)
			return prop_;
		delete prop_;
		return nullptr;
	}
}

// ---- PropertyGrid ----

PropertyGrid::PropertyGrid(const FnFactory<IPropertyEditor>& factory) : factory(factory) {}

void PropertyGrid::add_property_list_to_grid(const PropertyInfoList* list, void* inst, uint32_t flags,
											 uint32_t property_flag_mask) {
	ASSERT(list && inst);
	IGridRow* row = nullptr;

	if (flags & PG_LIST_PASSTHROUGH && list->count == 1 && list->list[0].type == core_type_id::List) {
		row = create_row(factory, nullptr, list->list, inst, -1, property_flag_mask);
		row->set_name_override(list->type_name);
		ASSERT(row);
	} else {
		row = new GroupRow(factory, nullptr, inst, list, -1, property_flag_mask);
	}

	rows.push_back(std::unique_ptr<IGridRow>(row));
}

void PropertyGrid::add_iproped_manual(IPropertyEditor* editor) {
	ASSERT(editor);
	PropertyRow* prop_ = new PropertyRow(editor, nullptr, editor, editor->prop /* hack job*/, -1);
	rows.push_back(std::unique_ptr<IGridRow>(prop_));
}

void PropertyGrid::add_class_to_grid(ClassBase* classinst) {
	ASSERT(classinst);
	auto ti = &classinst->get_type();
	while (ti) {
		if (ti->props)
			add_property_list_to_grid(ti->props, classinst);
		ti = ti->super_typeinfo;
	}
}

void PropertyGrid::update() {
	rows_had_changes = false;

	if (rows.empty()) {
		ImGui::Text("nothing to edit");
		return;
	}

	ImGui::BeginDisabled(read_only);

	ImGuiTableFlags const flags =
		ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit;
	if (ImGui::BeginTable("Table", 2, flags)) {

		ImGui::TableSetupColumn("##Header", ImGuiTableColumnFlags_WidthFixed, 200);
		ImGui::TableSetupColumn("##Editor", ImGuiTableColumnFlags_WidthStretch);

		for (int i = 0; i < rows.size(); i++) {
			rows[i]->update(this, 0.0);
		}

		ImGui::EndTable();
	}

	ImGui::EndDisabled();
}

// ---- IGridRow ----

#include "Render/Texture.h"
void IGridRow::clear_children() {
	child_rows.clear(); // unique_ptr handles destruction
}

void IGridRow::update(PropertyGrid* parentGrid, float header_ofs) {
	ASSERT(parentGrid);
	ImGui::PushID(this);

	ImGui::TableNextRow();
	ImGui::TableNextColumn();
	ImGui::AlignTextToFramePadding();

	draw_header(header_ofs);

	ImGui::TableNextColumn();
	ImGuiTableFlags const flags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingFixedFit;
	int num_cols = (has_row_controls()) ? 3 : 2;
	if (ImGui::BeginTable("GridTable", num_cols, flags)) {
		ImGui::TableSetupColumn("##Editor", ImGuiTableColumnFlags_WidthStretch);
		if (num_cols == 3)
			ImGui::TableSetupColumn("##Controls", ImGuiTableColumnFlags_WidthFixed, 60.0);
		ImGui::TableSetupColumn("##Reset", ImGuiTableColumnFlags_WidthFixed, 30.0);

		ImGui::TableNextRow();

		ImGui::TableNextColumn();
		ImGui::AlignTextToFramePadding();

		const bool had_changes = internal_update();
		if (had_changes)
			parentGrid->set_rows_had_changes();

		if (has_row_controls()) {
			ImGui::TableNextColumn();
			const bool changes = draw_row_controls();
			if (changes)
				parentGrid->set_rows_had_changes();
		}

		ImGui::TableNextColumn();
		if (has_reset_button()) {
			auto reset_img = g_assets.find_global_sync<Texture>("eng/icon/undo.png");
			if (my_imgui_image_button(reset_img, 14)) {
				on_reset();
				parentGrid->set_rows_had_changes();
			}
		}

		ImGui::EndTable();
	}

	ImGui::PopID();

	if (draw_children() && expanded) {
		for (int i = 0; i < child_rows.size(); i++)
			child_rows[i]->update(parentGrid, header_ofs + get_indent_width());
	}
}

// ---- IPropertyEditor ----

bool IPropertyEditor::update() {
	ASSERT(prop && instance);
	ImGui::PushID(this);
	bool ret = internal_update();
	ImGui::PopID();
	return ret;
}

// ---- Imgui text callback helpers ----

static int imgui_input_text_callback_completion(ImGuiInputTextCallbackData* data,
												ImguiInputTextCallbackUserStruct* user) {
	ASSERT(data && user);
	const char* word_end = data->Buf + data->CursorPos;
	const char* word_start = word_end;
	while (word_start > data->Buf) {
		const char c = word_start[-1];
		if (c == ' ' || c == '\t' || c == ',' || c == ';')
			break;
		word_start--;
	}

	auto candidates = *user->fcsc(user->fcsc_user_data, word_start, (int)(word_end - word_start));

	if (candidates.empty()) {
		// No match
		sys_print(Info, "No match for \"%.*s\"!\n", (int)(word_end - word_start), word_start);
	} else if (candidates.size() == 1) {
		// Single match. Delete the beginning of the word and replace it entirely so we've got nice casing.
		data->DeleteChars((int)(word_start - data->Buf), (int)(word_end - word_start));
		data->InsertChars(data->CursorPos, candidates[0]);
		data->InsertChars(data->CursorPos, " ");
	} else {
		// Multiple matches. Complete as much as we can.
		int match_len = (int)(word_end - word_start);
		for (;;) {
			int c = 0;
			bool all_candidates_matches = true;
			for (int i = 0; i < candidates.size() && all_candidates_matches; i++)
				if (i == 0)
					c = toupper(candidates[i][match_len]);
				else if (c == 0 || c != toupper(candidates[i][match_len]))
					all_candidates_matches = false;
			if (!all_candidates_matches)
				break;
			match_len++;
		}

		if (match_len > 0) {
			data->DeleteChars((int)(word_start - data->Buf), (int)(word_end - word_start));
			data->InsertChars(data->CursorPos, candidates[0], candidates[0] + match_len);
		}

		// List matches
		sys_print(Info, "Possible matches:\n");
		for (int i = 0; i < candidates.size(); i++)
			sys_print(Info, "- %s\n", candidates[i]);
	}
	return 0;
}

int imgui_input_text_callback_function(ImGuiInputTextCallbackData* data) {
	ImguiInputTextCallbackUserStruct* user = (ImguiInputTextCallbackUserStruct*)data->UserData;
	ASSERT(user);

	if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
		assert(user->string);
		user->string->resize(data->BufSize);
		data->Buf = (char*)user->string->data();
	} else if (data->EventFlag == ImGuiInputTextFlags_CallbackCompletion) {
		imgui_input_text_callback_completion(data, user);
	}

	return 0;
}

// ---- PropertyRow ----

PropertyRow::PropertyRow(const FnFactory<IPropertyEditor>& factory, IGridRow* parent, void* instance,
						 PropertyInfo* prop, int row_idx)
	: IGridRow(parent, row_idx), instance(instance), prop(prop) {
	ASSERT(prop && instance);
	prop_editor = std::unique_ptr<IPropertyEditor>(create_ipropertyed(factory, prop, instance, this));
}
PropertyRow::PropertyRow(IPropertyEditor* editor, IGridRow* parent, void* instance, PropertyInfo* prop, int row_idx)
	: IGridRow(parent, row_idx), instance(instance), prop(prop) {
	ASSERT(editor && prop && instance);
	prop_editor.reset(editor);
}

static std::string type_to_string(const PropertyInfo* p);

static void draw_tooltip(const PropertyInfo* prop) {
	ASSERT(prop);
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
		ImGui::BeginTooltip();
		ImGui::Text("%s", type_to_string(prop).c_str());
		if (*prop->tooltip) {
			ImGui::TextColored(ImVec4(0.8, 0.8, 0.8, 1), "%s", prop->tooltip);
		}
		ImGui::EndTooltip();
	}
}

static std::string type_to_string(const PropertyInfo* p) {
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
		return "vector<" + type_to_string(p->list_ptr->get_property()) + ">";
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

void PropertyRow::draw_header(float ofs) {
	ASSERT(prop);
	ImGui::Dummy(ImVec2(ofs, 0));
	ImGui::SameLine();
	if (name_override.empty())
		ImGui::Text("%s", prop->name);
	else
		ImGui::Text("%s", name_override.c_str());
	draw_tooltip(prop);
}

bool PropertyRow::internal_update() {
	ASSERT(prop_editor);
	return prop_editor->update();
}
#endif
