#include "PropertyEd.h"
#include "imgui.h"

static uint32_t color32_to_uint(Color32 color) {
	return *(uint32_t*)&color;
}


static IGridRow* create_row(IGridRow* parent, PropertyInfo* prop, void* inst, int row_idx)
{
	if (prop->type == core_type_id::List) {
		ArrayRow* array_ = new ArrayRow(nullptr, inst, prop, row_idx);
		return array_;
	}
	else {
		PropertyRow* prop_ = new PropertyRow(nullptr, inst, prop, row_idx);

		if (prop_->prop_editor)
			return prop_;
		delete prop_;
		return nullptr;
	}
}

void PropertyGrid::add_property_list_to_grid(PropertyInfoList* list, void* inst, uint32_t flags)
{
	IGridRow* row = nullptr;

	if (flags & PG_LIST_PASSTHROUGH && list->count == 1 && list->list[0].type == core_type_id::List) {
		row = create_row(nullptr, list->list, inst, -1);
		row->set_name_override(list->type_name);
		ASSERT(row);
	}
	else {
		row = new GroupRow(nullptr, inst, list, -1);
	}

	rows.push_back(std::unique_ptr<IGridRow>(row));

}

void PropertyGrid::update()
{
	if (rows.empty()) {
		ImGui::Text("nothing to edit");
		return;
	}

	ImGui::BeginDisabled(read_only);

	ImGuiTableFlags const flags =  ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit;
	//if (ImGui::Begin("PropEdit")) {
		if(ImGui::BeginTable("Table", 2, flags) ){

			ImGui::TableSetupColumn("##Header", ImGuiTableColumnFlags_WidthFixed, 200);
			ImGui::TableSetupColumn("##Editor", ImGuiTableColumnFlags_WidthStretch);

			for (int i = 0; i < rows.size(); i++) {
				rows[i]->update(0.0);
			}

			ImGui::EndTable();
		}

		ImGui::EndDisabled();
//	}
	//ImGui::End();
}
#include "Texture.h"
void IGridRow::clear_children()
{
	child_rows.clear();	// unique_ptr handles destruction
}
void IGridRow::update(float header_ofs)
{
	ImGui::PushID(this);

	ImGui::TableNextRow();
	ImGui::TableNextColumn();
	ImGui::AlignTextToFramePadding();


	draw_header(header_ofs);

	ImGui::TableNextColumn();
	ImGuiTableFlags const flags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingFixedFit;
	int num_cols = (has_row_controls()) ? 3 : 2;
	if (ImGui::BeginTable("GridTable", num_cols, flags))
	{
		ImGui::TableSetupColumn("##Editor", ImGuiTableColumnFlags_WidthStretch);
		if(num_cols == 3)
			ImGui::TableSetupColumn("##Controls", ImGuiTableColumnFlags_WidthFixed, 100.0);
		ImGui::TableSetupColumn("##Reset", ImGuiTableColumnFlags_WidthFixed, 50.0);


		ImGui::TableNextRow();

		ImGui::TableNextColumn();
		ImGui::AlignTextToFramePadding();
		
		internal_update();

		if (has_row_controls())
		{
			ImGui::TableNextColumn();
			draw_row_controls();
		}

		ImGui::TableNextColumn();
		if (has_reset_button())
		{
			auto reset_img = mats.find_texture("icon/undo.png");
			if (ImGui::ImageButton(ImTextureID(reset_img->gl_id), ImVec2(14, 14)))
				on_reset();
		}

		ImGui::EndTable();
	}

	ImGui::PopID();

	if (draw_children() && expanded) {
		for (int i = 0; i < child_rows.size(); i++)
			child_rows[i]->update(header_ofs + get_indent_width());
	}
}

void IPropertyEditor::update()
{
	ASSERT(prop && instance);
	ImGui::PushID(this);
	internal_update();
	ImGui::PopID();
}

void StringEditor::internal_update()
{
	ASSERT(prop->type == core_type_id::StdString);

	auto str = (std::string*)((char*)instance + prop->offset);

	ImguiInputTextCallbackUserStruct user;
	user.string = str;
	if (ImGui::InputText("##input_text", (char*)str->data(), str->size() + 1/* null terminator byte */, ImGuiInputTextFlags_CallbackResize, imgui_input_text_callback_function, &user))
		str->resize(strlen(str->c_str()));	// imgui messes with buffer size

}

bool StringEditor::can_reset()
{
	ASSERT(prop->type == core_type_id::StdString);

	auto str = (std::string*)((char*)instance + prop->offset);
	*str != prop->range_hint;
}

void StringEditor::reset_value()
{
	ASSERT(prop->type == core_type_id::StdString);

	auto str = (std::string*)((char*)instance + prop->offset);
	*str = prop->range_hint;
}

void FloatEditor::internal_update()
{
	ASSERT(prop->type == core_type_id::Float);

	float* ptr = (float*)((char*)instance + prop->offset);

	ImGui::InputFloat("##input_float", ptr, 0.05);
}

void EnumEditor::internal_update()
{
	ASSERT(prop->type == core_type_id::Enum8 || prop->type == core_type_id::Enum16 || prop->type == core_type_id::Enum32);

	int enum_val = prop->get_int(instance);
	auto& enum_def = Enum::get_enum_def(prop->enum_type_id);
	ASSERT(enum_val >= 0 && enum_val < enum_def.count);

	ImGui::Combo("##combo", &enum_val, enum_def.strs, enum_def.count);

	prop->set_int(instance, enum_val);
}

void BooleanEditor::internal_update()
{
	ASSERT(prop->type == core_type_id::Bool);

	bool b = prop->get_int(instance);

	ImGui::Checkbox("##checkbox", &b);

	prop->set_int(instance, b);
}

void IntegerEditor::internal_update()
{
	ASSERT(prop->type == core_type_id::Int8 || prop->type == core_type_id::Int16 || prop->type == core_type_id::Int32 || prop->type == core_type_id::Int64);

	int val = prop->get_int(instance);

	ImGui::InputInt("##input_int", &val);

	prop->set_int(instance, val);
}


static IPropertyEditor* create_ipropertyed(PropertyInfo* prop, void* instance) {

	IPropertyEditor* out = nullptr;
	out = IPropertyEditor::get_factory().createObject(prop->custom_type_str);
	if (out) {
		out->post_construct_for_custom_type(instance, prop);
		return out;
	}

	switch (prop->type)
	{
	case core_type_id::Bool:
		return new BooleanEditor(instance, prop);
	case core_type_id::Enum8:
	case core_type_id::Enum16:
	case core_type_id::Enum32:
		return new EnumEditor(instance, prop);
	case core_type_id::StdString:
		return new StringEditor(instance, prop);
	case core_type_id::Float:
		return new FloatEditor(instance, prop);
	case core_type_id::Int8:
	case core_type_id::Int16:
	case core_type_id::Int32:
	case core_type_id::Int64:
		return new IntegerEditor(instance, prop);

	default:
		printf("!!!! NO TYPE DEFINED FOR IPropertyEditorFactory %s !!!\n", prop->name);
		return nullptr;
	}

}

int imgui_input_text_callback_completion(ImGuiInputTextCallbackData* data, ImguiInputTextCallbackUserStruct* user)
{
	const char* word_end = data->Buf + data->CursorPos;
	const char* word_start = word_end;
	while (word_start > data->Buf)
	{
		const char c = word_start[-1];
		if (c == ' ' || c == '\t' || c == ',' || c == ';')
			break;
		word_start--;
	}

	auto candidates = *user->fcsc(user->fcsc_user_data, word_start, (int)(word_end - word_start));

	if (candidates.empty())
	{
		// No match
		sys_print("No match for \"%.*s\"!\n", (int)(word_end - word_start), word_start);
	}
	else if (candidates.size() == 1)
	{
		// Single match. Delete the beginning of the word and replace it entirely so we've got nice casing.
		data->DeleteChars((int)(word_start - data->Buf), (int)(word_end - word_start));
		data->InsertChars(data->CursorPos, candidates[0]);
		data->InsertChars(data->CursorPos, " ");
	}
	else
	{
		// Multiple matches. Complete as much as we can..
		// So inputing "C"+Tab will complete to "CL" then display "CLEAR" and "CLASSIFY" as matches.
		int match_len = (int)(word_end - word_start);
		for (;;)
		{
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

		if (match_len > 0)
		{
			data->DeleteChars((int)(word_start - data->Buf), (int)(word_end - word_start));
			data->InsertChars(data->CursorPos, candidates[0], candidates[0] + match_len);
		}

		// List matches
		sys_print("Possible matches:\n");
		for (int i = 0; i < candidates.size(); i++)
			sys_print("- %s\n", candidates[i]);
	}
	return 0;
}

int imgui_input_text_callback_function(ImGuiInputTextCallbackData* data)
{
	ImguiInputTextCallbackUserStruct* user = (ImguiInputTextCallbackUserStruct*)data->UserData;
	assert(user);

	if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
		assert(user->string);
		user->string->resize(data->BufSize);
		data->Buf = (char*)user->string->data();
	}
	else if (data->EventFlag == ImGuiInputTextFlags_CallbackCompletion) {
		imgui_input_text_callback_completion(data, user);
	}


	return 0;
}

Factory<std::string, IPropertyEditor>& IPropertyEditor::get_factory()
{
	static Factory<std::string, IPropertyEditor> inst;
	return inst;
}

Factory<std::string, IArrayHeader>& IArrayHeader::get_factory()
{
	static Factory<std::string, IArrayHeader> inst;
	return inst;
}

ArrayRow::ArrayRow(IGridRow* parent, void* instance, PropertyInfo* prop, int row_idx) : IGridRow(parent, row_idx), instance(instance), prop(prop)
{
	header = std::unique_ptr<IArrayHeader>(IArrayHeader::get_factory().createObject(prop->custom_type_str));
	if(header)
		header->post_construct(instance, prop);

	rebuild_child_rows();
}

int ArrayRow::get_size()
{
	return prop->list_ptr->get_size(prop->get_ptr(instance));
}

PropertyRow::PropertyRow(IGridRow* parent, void* instance, PropertyInfo* prop, int row_idx) : IGridRow(parent, row_idx), instance(instance), prop(prop)
{
	prop_editor = std::unique_ptr<IPropertyEditor>(create_ipropertyed(prop,instance));
}

void ArrayRow::hook_update_pre_tree_node()
{
	if (set_next_state == next_state::hidden)
		ImGui::SetNextItemOpen(false);
	else if (set_next_state == next_state::visible)
		ImGui::SetNextItemOpen(true);
}

bool ArrayRow::are_any_nodes_open()
{
	for (int i = 0; i < child_rows.size(); i++) {
		if (child_rows[i]->expanded)
			return true;
	}
	return false;
}

void ArrayRow::draw_row_controls()
{
	if (header && !header->can_edit_array())
		return;

	auto trashimg = mats.find_texture("icon/trash.png");
	auto addimg = mats.find_texture("icon/plus.png");

	auto visible_icon = mats.find_texture("icon/visible.png");
	auto hidden_icon = mats.find_texture("icon/hidden.png");

	bool are_any_open = are_any_nodes_open();

	uint8_t* list_instance_ptr = prop->get_ptr(instance);

	ImGui::PushStyleColor(ImGuiCol_Button, 0);
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, color32_to_uint({ 245, 242, 242, 55 }));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, 0);

	if (ImGui::ImageButton(ImTextureID(addimg->gl_id), ImVec2(16, 16))) {
		clear_children();
		prop->list_ptr->resize(list_instance_ptr, prop->list_ptr->get_size(list_instance_ptr) + 1);	// might invalidate childrens ptrs, so refresh
		rebuild_child_rows();
	}

	ImGui::SameLine();

	if (!header || header->has_delete_all()) {
		ImGui::BeginDisabled(child_rows.empty());
		if (ImGui::ImageButton(ImTextureID(trashimg->gl_id), ImVec2(16, 16))) {

			clear_children();
			prop->list_ptr->resize(list_instance_ptr, 0);
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
	}

	set_next_state = next_state::keep;

	if (are_any_open) {
		if (ImGui::ImageButton(ImTextureID(visible_icon->gl_id), ImVec2(16, 16))) {
			set_next_state = next_state::hidden;
		}
	}
	else {
		if (ImGui::ImageButton(ImTextureID(hidden_icon->gl_id), ImVec2(16, 16))) {
			set_next_state = next_state::visible;
		}
	}

	ImGui::PopStyleColor(3);
}

 void ArrayRow::internal_update() 
{

	 uint8_t* list_instance_ptr = prop->get_ptr(instance);
	 {
		 int count = get_size();

		 ImGui::TextColored(ImVec4(0.5,0.5,0.5,1.0),"elements: %d", count);

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

		 }break;

		 case Movedown: {
			 int size = get_size();

			 int index = commands[i].index;
			 if (index < size - 1) {
				 prop->list_ptr->swap_elements(list_instance_ptr, index, index + 1);
			 }

		 }break;

		 case Moveup: {
			 int index = commands[i].index;
			 if (index > 0) {
				 prop->list_ptr->swap_elements(list_instance_ptr, index - 1, index);
			 }

		 }break;

		 }
	 }

	 if (!commands.empty()) {
		 clear_children();
		 rebuild_child_rows();
		 commands.clear();
	 }
}

 void ArrayRow::draw_header(float header_ofs)
 {
	 ImGui::Dummy(ImVec2(header_ofs, 0));
	 ImGui::SameLine();

	 ImGui::PushStyleColor(ImGuiCol_Header, 0);
	 ImGui::PushStyleColor(ImGuiCol_HeaderActive, 0);
	 ImGui::PushStyleColor(ImGuiCol_HeaderHovered, 0);

	 const char* name = prop->name;
	 if (!name_override.empty()) name = name_override.c_str();

	 expanded = ImGui::TreeNodeEx(name, ImGuiTreeNodeFlags_DefaultOpen);
	 if(expanded)
		 ImGui::TreePop();
	 ImGui::PopStyleColor(3);
 }

 void ArrayRow::rebuild_child_rows()
 {
	
	 ASSERT(child_rows.size() == 0);	// clearing done before
	 ASSERT(prop->type == core_type_id::List);

	 IListCallback* list = prop->list_ptr;
	 int count = list->get_size(prop->get_ptr(instance));
	 PropertyInfoList* struct_ = list->props_in_list;



	 for (int i = 0; i < count; i++) {
		 child_rows.push_back(std::unique_ptr<IGridRow>(new GroupRow(this, list->get_index(prop->get_ptr(instance), i), struct_, i)));
	 }
 }

 void PropertyRow::draw_header(float ofs)
 {
	 ImGui::Dummy(ImVec2(ofs, 0));
	 ImGui::SameLine();

	 ImGui::Text(prop->name);

 }

 void PropertyRow::internal_update()
 {
	 prop_editor->update();
 }


 GroupRow::GroupRow(IGridRow* parent, void* instance, PropertyInfoList* list, 
	 int row_idx) 
	 : IGridRow(parent, row_idx), proplist(list), 
	 inst(instance)
 {
	
	 for (int i = 0; i < list->count; i++) {
		 auto& prop = list->list[i];
		 if (!prop.can_edit())
			 continue;

		 auto row = create_row(this, &prop, inst, -1);
		 if (row)
			 child_rows.push_back(std::unique_ptr<IGridRow>(row));
	 }

	 name = list->type_name;

	 if (row_idx != -1) {
		 name = string_format("[ %d ]", row_idx);
	 }
 }

 bool GroupRow::draw_children()
 {
	 return !passthrough_to_child();
 }

 void GroupRow::draw_row_controls()
 {
	 ASSERT(parent);
	 ASSERT(row_index != -1);

	 ArrayRow* array_ = (ArrayRow*)parent;

	 if (array_->header && !array_->header->can_edit_array())
		 return;

	 bool canmoveup = row_index > 0;
	 bool canmovedown = (row_index != array_->get_size() - 1);

	 auto moveup = mats.find_texture("icon/moveup.png");
	 auto movedown = mats.find_texture("icon/movedown.png");
	 auto trash1 = mats.find_texture("icon/trash1.png");


	 ImGui::PushStyleColor(ImGuiCol_Button, 0);
	 ImGui::PushStyleColor(ImGuiCol_ButtonHovered, color32_to_uint({ 245, 242, 242, 55 }));
	 ImGui::PushStyleColor(ImGuiCol_ButtonActive, 0);

	 ImGui::BeginDisabled(!canmoveup);
	 if (ImGui::ImageButton((ImTextureID)moveup->gl_id, ImVec2(16, 16))) {
		 array_->moveup_index(row_index);
	 }
	 ImGui::EndDisabled();
	 ImGui::SameLine();
	 ImGui::BeginDisabled(!canmovedown);
	 if (ImGui::ImageButton((ImTextureID)movedown->gl_id, ImVec2(16, 16))) {
		 array_->movedown_index(row_index);
	 }
	 ImGui::EndDisabled();
	 ImGui::SameLine();
	 if (ImGui::ImageButton((ImTextureID)trash1->gl_id, ImVec2(16, 16))) {
		 array_->delete_index(row_index);
	 }

	 ImGui::PopStyleColor(3);
 }


 void GroupRow::draw_header(float ofs)
 {
	 ImGui::Dummy(ImVec2(ofs, 0));
	 ImGui::SameLine();

	 ImGui::PushStyleColor(ImGuiCol_Header, 0);
	 ImGui::PushStyleColor(ImGuiCol_HeaderActive, 0);
	 ImGui::PushStyleColor(ImGuiCol_HeaderHovered, 0);

	 bool is_array = row_index != -1;
	 bool has_drawn = false;
	 if (is_array) {
		 ArrayRow* array_ = (ArrayRow*)parent;

		 array_->hook_update_pre_tree_node();

		 if (array_->header) {
			 expanded = array_->header->imgui_draw_header(row_index);
			 has_drawn = true;
		 }
	 }
	 if(!has_drawn) {

		 uint32_t flags = (row_index == -1) ? ImGuiTreeNodeFlags_DefaultOpen : 0;
		 expanded = ImGui::TreeNodeEx(name.c_str(), flags);
		if(expanded)
			 ImGui::TreePop();

	 }
	ImGui::PopStyleColor(3);
 }

 void GroupRow::internal_update() {

	 if (passthrough_to_child())
	 {
		 auto row = (IGridRow*)child_rows[0].get();

		 row->internal_update();

	 }

	 bool is_array = row_index != -1;
	 if (!expanded && is_array) {
		 ArrayRow* array_ = (ArrayRow*)parent;
		 if (array_->header) {
			 array_->header->imgui_draw_closed_body(row_index);
		 }
	 }

 }