#include "PropertyEd.h"
#include "imgui.h"
#include "GlobalEnumMgr.h"


void PropertyGrid::add_property_list_to_grid(PropertyInfoList* list, void* inst)
{

	for (int i = 0; i < list->count; i++) {
		auto& prop = list->list[i];
		if (!prop.can_edit())
			continue;

		if (prop.type == core_type_id::StdVector) {
			ArrayRow* array_ = new ArrayRow(nullptr, inst, &prop);
			rows.push_back(std::unique_ptr<IGridRow>(array_));
		}
		else {
			PropertyRow* prop_ = new PropertyRow(nullptr, inst, &prop);

			if(prop_->prop_editor)
				rows.push_back(std::unique_ptr<IGridRow>(prop_));
		}
	}


}

void PropertyGrid::update()
{
	if (rows.empty()) {
		ImGui::Text("nothing to edit");
		return;
	}
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
//	}
	//ImGui::End();
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
	//if (ImGui::BeginTable("GridTable", 2, flags))
	//{
		//ImGui::TableSetupColumn("##Editor", ImGuiTableColumnFlags_WidthStretch);
		//ImGui::TableSetupColumn("##Reset", ImGuiTableColumnFlags_WidthFixed, 100.0);

		//ImGui::TableNextRow();

		//ImGui::TableNextColumn();
		//ImGui::AlignTextToFramePadding();
		
		internal_update();

		//if (HasExtraControls())
		//{
		//	ImGui::TableNextColumn();
		//	DrawExtraControlsSection();
		//}

		ImGui::TableNextColumn();
		//if (HasResetSection())
		//{
		//	DrawResetSection();
		//}

		//ImGui::EndTable();
	//}

	ImGui::PopID();
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
	ASSERT(0);
}

PropertyRow::PropertyRow(IGridRow* parent, void* instance, PropertyInfo* prop) : IGridRow(parent), instance(instance), prop(prop)
{
	prop_editor = std::unique_ptr<IPropertyEditor>(IPropertyEditorFactory::create(prop, instance));
}

 void ArrayRow::internal_update() 
{

}

 void ArrayRow::draw_header()
 {
	 ImGui::CollapsingHeader(prop->name);
 }

 void PropertyRow::draw_header()
 {
	 ImGui::Text(prop->name);
 }

 void PropertyRow::internal_update()
 {
	 prop_editor->update();
 }