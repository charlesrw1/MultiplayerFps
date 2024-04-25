#include "PropertyEd.h"
#include "imgui.h"
#include "GlobalEnumMgr.h"

static IGridRow* create_row(IGridRow* parent, PropertyInfo* prop, void* inst)
{
	if (prop->type == core_type_id::List) {
		ArrayRow* array_ = new ArrayRow(nullptr, inst, prop);
		return array_;
	}
	else {
		PropertyRow* prop_ = new PropertyRow(nullptr, inst, prop);

		if (prop_->prop_editor)
			return prop_;
		delete prop_;
		return nullptr;
	}
}

void PropertyGrid::add_property_list_to_grid(PropertyInfoList* list, void* inst)
{

	for (int i = 0; i < list->count; i++) {
		auto& prop = list->list[i];
		if (!prop.can_edit())
			continue;

		auto row = create_row(nullptr, &prop, inst);
		if (row)
			rows.push_back(std::unique_ptr<IGridRow>(row));
	}


}

void PropertyGrid::update()
{
	if (rows.empty()) {
		ImGui::Text("nothing to edit");
		return;
	}

	ImGui::BeginDisabled(read_only);

	ImGuiTableFlags const flags = ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_NoBordersInBodyUntilResize | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY;
	//if (ImGui::Begin("PropEdit")) {
		if(ImGui::BeginTable("Table", 2, flags) ){

			ImGui::TableSetupColumn("##Header", ImGuiTableColumnFlags_WidthFixed, 200);
			ImGui::TableSetupColumn("##Editor", ImGuiTableColumnFlags_WidthStretch);

			for (int i = 0; i < rows.size(); i++) {
				rows[i]->update();
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
void IGridRow::update()
{
	ImGui::PushID(this);

	ImGui::TableNextRow();
	ImGui::TableNextColumn();
	ImGui::AlignTextToFramePadding();


	draw_header();

	ImGui::TableNextColumn();
	ImGuiTableFlags const flags = ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_SizingFixedFit;
	if (ImGui::BeginTable("GridTable", 2, flags))
	{
		ImGui::TableSetupColumn("##Editor", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("##Reset", ImGuiTableColumnFlags_WidthFixed, 100.0);

		ImGui::TableNextRow();

		ImGui::TableNextColumn();
		ImGui::AlignTextToFramePadding();
		
		internal_update();

		//if (HasExtraControls())
		//{
		//	ImGui::TableNextColumn();
		//	DrawExtraControlsSection();
		//}

		ImGui::TableNextColumn();
		//if (HasResetSection())
		{
			//DrawResetSection();

			auto reset_img = mats.find_texture("icon/undo.png");

			ImGui::ImageButton(ImTextureID(reset_img->gl_id), ImVec2(16, 16));
		}

		ImGui::EndTable();
	}

	ImGui::PopID();

	for (int i = 0; i < child_rows.size(); i++)
		child_rows[i]->update();
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

	auto str = *(std::string*)((char*)instance + prop->offset);

	ImguiInputTextCallbackUserStruct user;
	user.string = &str;
	ImGui::InputText("##input_text", (char*)str.data(), str.size() + 1, ImGuiInputTextFlags_CallbackResize, imgui_input_text_callback_function, &user);
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
	auto& enum_def = GlobalEnumDefMgr::get().get_enum_def(prop->enum_type_id);
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
	ASSERT(prop->type == core_type_id::Int8 || prop->type == core_type_id::Int16 || prop->type == core_type_id::Int32);

	int val = prop->get_int(instance);

	ImGui::InputInt("##input_int", &val);

	prop->set_int(instance, val);
}

IPropertyEditorFactory* IPropertyEditorFactory::first = nullptr;

IPropertyEditorFactory::IPropertyEditorFactory()
{
	next = first;
	first = this;
}

IPropertyEditor* IPropertyEditorFactory::create(PropertyInfo* prop, void* instance)
{
	IPropertyEditorFactory* factory = first;
	while (factory) {
		
		IPropertyEditor* out = factory->try_create(prop, instance);
		if (out)
			return out;
		factory = factory->next;
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

ArrayRow::ArrayRow(IGridRow* parent, void* instance, PropertyInfo* prop) : IGridRow(parent), instance(instance), prop(prop)
{
	rebuild_child_rows();
}

PropertyRow::PropertyRow(IGridRow* parent, void* instance, PropertyInfo* prop) : IGridRow(parent), instance(instance), prop(prop)
{
	prop_editor = std::unique_ptr<IPropertyEditor>(IPropertyEditorFactory::create(prop, instance));
}

 void ArrayRow::internal_update() 
{
	 auto trashimg = mats.find_texture("icon/trash.png");
	 auto addimg = mats.find_texture("icon/plus.png");

	 if (ImGui::ImageButton(ImTextureID(addimg->gl_id), ImVec2(16, 16))) {

		 clear_children();
		 prop->list_ptr->resize(instance, prop->list_ptr->get_size(instance) + 1);
		 rebuild_child_rows();
	 }

	 ImGui::SameLine();

	 if (ImGui::ImageButton(ImTextureID(trashimg->gl_id), ImVec2(16, 16))) {

		 clear_children();
		 prop->list_ptr->resize(instance, 0);
	 }
}

 void ArrayRow::draw_header()
 {
	 ImGui::PushStyleColor(ImGuiCol_Header, 0);
	 ImGui::PushStyleColor(ImGuiCol_HeaderActive, 0);
	 ImGui::PushStyleColor(ImGuiCol_HeaderHovered, 0);
	 if(ImGui::TreeNode(prop->name))
		 ImGui::TreePop();
	 ImGui::PopStyleColor(3);
 }

 void ArrayRow::rebuild_child_rows()
 {
	 clear_children();

	 ASSERT(prop->type == core_type_id::List);

	 IListCallback* list = prop->list_ptr;
	 int count = list->get_size(instance);
	 PropertyInfoList* struct_ = list->props_in_list;
	 for (int i = 0; i < count; i++) {

		 ASSERT(struct_->count >= 1);
		 // FIXME:
		 IGridRow* child = create_row(this, &struct_->list[0], list->get_index(instance, i));

		 ASSERT(child);

		 child_rows.push_back(std::unique_ptr<IGridRow>(child));
	 }
 }

 void PropertyRow::draw_header()
 {
	 ImGui::Text(prop->name);
 }

 void PropertyRow::internal_update()
 {
	 prop_editor->update();
 }