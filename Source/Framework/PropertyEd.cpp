#ifdef EDITOR_BUILD
#include "PropertyEd.h"
#include "imgui.h"
#include "Assets/AssetDatabase.h"

static uint32_t color32_to_uint(Color32 color) {
	return *(uint32_t*)&color;
}

IGridRow* create_row(IGridRow* parent, PropertyInfo* prop, void* inst, int row_idx, uint32_t property_flag_mask);

class UniquePtrRow : public IGridRow
{
public:
	UniquePtrRow(IGridRow* parent, void* instance, PropertyInfo* info, int row_idx, uint32_t prop_flag_mask) : IGridRow(parent, row_idx) {
		assert(info->type == core_type_id::StdUniquePtr);
		flagmask = prop_flag_mask;
		ClassBase** uniquePtr = (ClassBase**)info->get_ptr(instance);

		add_children(*uniquePtr);
		type_of_base = ClassBase::find_class(info->range_hint);
		this->info = info;
		this->inst = instance;

	}
	bool internal_update() override {
		auto classroot = info->range_hint;

		if (!type_of_base) {
			ImGui::Text("Couldnt find base class: %s\n", info->range_hint);
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

				if (subclasses.get_type()->allocate) {
					if (ImGui::Selectable(subclasses.get_type()->classname,
						subclasses.get_type() == thisType
					)) {
						thisType = subclasses.get_type();
						has_update = true;
					}
				}

			}
			ImGui::EndCombo();
		}

		if (has_update) {
			delete* uniquePtr;
			*uniquePtr = nullptr;
			if (thisType) {
				assert(thisType->allocate);
				*uniquePtr = thisType->allocate();
			}
			
			add_children(*uniquePtr);
		}

		return has_update;

	}
	void draw_header(float header_ofs) override {
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

			auto row = create_row(this, &prop, b, -1, flagmask);
			if (row)
				child_rows.push_back(std::unique_ptr<IGridRow>(row));
		}
	}
	uint32_t flagmask = 0;
	void* inst = nullptr;
	PropertyInfo* info = nullptr;
	const ClassTypeInfo* type_of_base = nullptr;
};



static IGridRow* create_row(IGridRow* parent, PropertyInfo* prop, void* inst, int row_idx, uint32_t property_flag_mask)
{
	if (prop->type == core_type_id::List) {
		ArrayRow* array_ = new ArrayRow(nullptr, inst, prop, row_idx, property_flag_mask);
		return array_;
	}
	else if (prop->type == core_type_id::StdUniquePtr) {
		auto row = new UniquePtrRow(nullptr, inst, prop, row_idx, property_flag_mask);
		return row;
	}
	else {
		PropertyRow* prop_ = new PropertyRow(nullptr, inst, prop, row_idx);

		if (prop_->prop_editor)
			return prop_;
		delete prop_;
		return nullptr;
	}
}

void PropertyGrid::add_property_list_to_grid(const PropertyInfoList* list, void* inst, uint32_t flags, uint32_t property_flag_mask)
{
	IGridRow* row = nullptr;

	if (flags & PG_LIST_PASSTHROUGH && list->count == 1 && list->list[0].type == core_type_id::List) {
		row = create_row(nullptr, list->list, inst, -1, property_flag_mask);
		row->set_name_override(list->type_name);
		ASSERT(row);
	}
	else {
		row = new GroupRow(nullptr, inst, list, -1, property_flag_mask);
	}

	rows.push_back(std::unique_ptr<IGridRow>(row));

}

void PropertyGrid::add_class_to_grid(ClassBase* classinst)
{
	auto ti = &classinst->get_type();
	while (ti) {
		if (ti->props)
			add_property_list_to_grid(ti->props, classinst);
		ti = ti->super_typeinfo;
	}
}

void PropertyGrid::update()
{
	//if (rows_had_changes)
	//	sys_print(Debug, "cleared change flag\n");
	rows_had_changes = false;

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
				rows[i]->update(this, 0.0);
			}

			ImGui::EndTable();
		}

		ImGui::EndDisabled();
//	}
	//ImGui::End();
}
#include "Render/Texture.h"
void IGridRow::clear_children()
{
	child_rows.clear();	// unique_ptr handles destruction
}
void IGridRow::update(PropertyGrid* parentGrid,float header_ofs)
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
		
		const bool had_changes = internal_update();
		if (had_changes)
			parentGrid->set_rows_had_changes();


		if (has_row_controls())
		{
			ImGui::TableNextColumn();
			const bool changes = draw_row_controls();
			if (changes)
				parentGrid->set_rows_had_changes();
		}

		ImGui::TableNextColumn();
		if (has_reset_button())
		{
			auto reset_img = g_assets.find_global_sync<Texture>("icon/undo.png");
			if (ImGui::ImageButton(ImTextureID(uint64_t(reset_img->gl_id)), ImVec2(14, 14))) {
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

bool IPropertyEditor::update()
{
	ASSERT(prop && instance);
	ImGui::PushID(this);
	bool ret= internal_update();
	ImGui::PopID();
	return ret;
}

bool StringEditor::internal_update()
{
	ASSERT(prop->type == core_type_id::StdString);

	auto str = (std::string*)((char*)instance + prop->offset);

	ImguiInputTextCallbackUserStruct user;
	user.string = str;
	if (ImGui::InputText("##input_text", (char*)str->data(), str->size() + 1/* null terminator byte */, 
		ImGuiInputTextFlags_CallbackResize, imgui_input_text_callback_function, &user)) {
		str->resize(strlen(str->c_str()));	// imgui messes with buffer size
		return true;
	}
	return false;

}

bool StringEditor::can_reset()
{
	ASSERT(prop->type == core_type_id::StdString);

	auto str = (std::string*)((char*)instance + prop->offset);
	return *str != prop->range_hint;
}

void StringEditor::reset_value()
{
	ASSERT(prop->type == core_type_id::StdString);

	auto str = (std::string*)((char*)instance + prop->offset);
	*str = prop->range_hint;
}

bool FloatEditor::internal_update()
{
	ASSERT(prop->type == core_type_id::Float);

	float* ptr = (float*)((char*)instance + prop->offset);

	if (ImGui::InputFloat("##input_float", ptr, 0.05))
		return true;
	return false;
}

bool EnumEditor::internal_update()
{
	ASSERT(prop->type == core_type_id::Enum8 || prop->type == core_type_id::Enum16 || prop->type == core_type_id::Enum32);
	ASSERT(prop->enum_type && prop->enum_type->str_count > 0);
	int64_t myval = prop->get_int(instance);

	auto myenumint = prop->enum_type->find_for_value(myval);
	if (!myenumint) {
		myval = prop->enum_type->strs[0].value;
		prop->set_int(instance,myval);
		myenumint = &prop->enum_type->strs[0];
	}

	bool ret = false;
	if (ImGui::BeginCombo("##type", myenumint->name)) {
		for (auto& enumiterator : *prop->enum_type) {
			bool selected = enumiterator.value == myval;
			if (ImGui::Selectable(enumiterator.name, &selected)) {
				myval = enumiterator.value;
				prop->set_int(instance, myval);
				ret = true;
			}
		}
		ImGui::EndCombo();
	}
	return ret;
}

bool BooleanEditor::internal_update()
{
	ASSERT(prop->type == core_type_id::Bool);

	bool b = prop->get_int(instance);
	bool ret = ImGui::Checkbox("##checkbox", &b);

	prop->set_int(instance, b);
	return ret;
}

bool IntegerEditor::internal_update()
{
	ASSERT(prop->type == core_type_id::Int8 || prop->type == core_type_id::Int16 || prop->type == core_type_id::Int32 || prop->type == core_type_id::Int64);

	int val = prop->get_int(instance);

	bool ret = ImGui::InputInt("##input_int", &val);

	prop->set_int(instance, val);
	return ret;
}

class VectorEditor : public IPropertyEditor
{
public:
	VectorEditor(void* ins, PropertyInfo* inf) {
		prop = inf; instance = ins;
	}
	virtual bool internal_update() {
		glm::vec3* v = (glm::vec3*)prop->get_ptr(instance);
		bool ret = false;
		if (ImGui::DragFloat3("##vec", (float*)v, 0.05))
			ret = true;
		return ret;
	}
};
#include "glm/gtx/euler_angles.hpp"
class RotationEditor : public IPropertyEditor
{
public:
	RotationEditor(void* ins, PropertyInfo* inf)  {
		prop = inf; instance = ins;
	}

	virtual bool internal_update() {
		glm::quat* v = (glm::quat*)prop->get_ptr(instance);

		glm::vec3 eul = glm::eulerAngles(*v);
		eul *= 180.f / PI;
		if (ImGui::DragFloat3("##eul", &eul.x, 1.0)) {
			eul *= PI / 180.f;
			*v = glm::normalize(glm::quat(eul));

			return true;
		}
		return false;
	}
};

static IPropertyEditor* create_ipropertyed(PropertyInfo* prop, void* instance, IGridRow* parent) {

	IPropertyEditor* out = nullptr;
	out = IPropertyEditor::get_factory().createObject(prop->custom_type_str);
	if (out) {
		out->post_construct_for_custom_type(instance, prop,parent);
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
	case core_type_id::Vec3:
		return new VectorEditor(instance, prop);
	case core_type_id::Quat:
		return new RotationEditor(instance, prop);
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
		sys_print(Info, "No match for \"%.*s\"!\n", (int)(word_end - word_start), word_start);
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
		sys_print(Info, "Possible matches:\n");
		for (int i = 0; i < candidates.size(); i++)
			sys_print(Info, "- %s\n", candidates[i]);
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

ArrayRow::ArrayRow(IGridRow* parent, void* instance, PropertyInfo* prop, int row_idx, uint32_t property_flag_mask) 
	: IGridRow(parent, row_idx), instance(instance), prop(prop), property_flag_mask(property_flag_mask)
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
	prop_editor = std::unique_ptr<IPropertyEditor>(create_ipropertyed(prop,instance, this));
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

bool ArrayRow::draw_row_controls()
{
	if (header && !header->can_edit_array())
		return false;
	bool ret = false;
	auto trashimg = g_assets.find_global_sync<Texture>("icon/trash.png");
	auto addimg = g_assets.find_global_sync<Texture>("icon/plus.png");

	auto visible_icon = g_assets.find_global_sync<Texture>("icon/visible.png");
	auto hidden_icon = g_assets.find_global_sync<Texture>("icon/hidden.png");

	bool are_any_open = are_any_nodes_open();

	uint8_t* list_instance_ptr = prop->get_ptr(instance);

	ImGui::PushStyleColor(ImGuiCol_Button, 0);
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, color32_to_uint({ 245, 242, 242, 55 }));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, 0);

#pragma warning(disable : 4312)
	if (ImGui::ImageButton(ImTextureID(addimg->gl_id), ImVec2(16, 16))) {
		ret = true;
		clear_children();
		prop->list_ptr->resize(list_instance_ptr, prop->list_ptr->get_size(list_instance_ptr) + 1);	// might invalidate childrens ptrs, so refresh
		rebuild_child_rows();
	}

	ImGui::SameLine();

	if (!header || header->has_delete_all()) {
		ImGui::BeginDisabled(child_rows.empty());
		if (ImGui::ImageButton(ImTextureID(trashimg->gl_id), ImVec2(16, 16))) {
			ret = true;
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
#pragma warning(default : 4312)

	ImGui::PopStyleColor(3);

	return ret;
}

 bool ArrayRow::internal_update() 
{
	 bool ret = false;


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
		 ret = true;
		 clear_children();
		 rebuild_child_rows();
		 commands.clear();
	 }
	 return ret;
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
	 const PropertyInfoList* struct_ = list->props_in_list;



	 for (int i = 0; i < count; i++) {
		 child_rows.push_back(std::unique_ptr<IGridRow>(new GroupRow(this, list->get_index(prop->get_ptr(instance), i), struct_, i, property_flag_mask)));
	 }
 }

 void PropertyRow::draw_header(float ofs)
 {
	 ImGui::Dummy(ImVec2(ofs, 0));
	 ImGui::SameLine();
	 if (name_override.empty())
		 ImGui::Text(prop->name);
	 else
		 ImGui::Text(name_override.c_str());

 }

 bool PropertyRow::internal_update()
 {
	 return prop_editor->update();
 }


 GroupRow::GroupRow(IGridRow* parent, void* instance, const PropertyInfoList* list, 
	 int row_idx, uint32_t property_flag_mask) 
	 : IGridRow(parent, row_idx), proplist(list), 
	 inst(instance)
 {
	
	 for (int i = 0; i < list->count; i++) {
		 auto& prop = list->list[i];
		 if (!prop.can_edit())
			 continue;
		 bool passed_mask_check = (prop.flags & property_flag_mask)!= 0;
		 if (!passed_mask_check)
			 continue;

		 auto row = create_row(this, &prop, inst, -1, property_flag_mask);
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

 bool GroupRow::draw_row_controls()
 {
	 ASSERT(parent);
	 ASSERT(row_index != -1);

	 ArrayRow* array_ = (ArrayRow*)parent;
	 if (!array_)
		 return false;
	 if (array_->header && !array_->header->can_edit_array())
		 return false;

	 bool canmoveup = row_index > 0;
	 bool canmovedown = (row_index != array_->get_size() - 1);

	 auto moveup = g_assets.find_global_sync<Texture>("icon/moveup.png");
	 auto movedown = g_assets.find_global_sync<Texture>("icon/movedown.png");
	 auto trash1 = g_assets.find_global_sync<Texture>("icon/trash1.png");


	 ImGui::PushStyleColor(ImGuiCol_Button, 0);
	 ImGui::PushStyleColor(ImGuiCol_ButtonHovered, color32_to_uint({ 245, 242, 242, 55 }));
	 ImGui::PushStyleColor(ImGuiCol_ButtonActive, 0);

#pragma warning(disable : 4312)
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
#pragma warning(default : 4312)

	 return false;	// array will update the flag itself
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

 bool GroupRow::internal_update() {

	 bool ret = false;
	 if (passthrough_to_child())
	 {
		 auto row = (IGridRow*)child_rows[0].get();

		ret =  row->internal_update();

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