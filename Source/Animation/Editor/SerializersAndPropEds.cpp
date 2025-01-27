#ifdef EDITOR_BUILD
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_internal.h"

#include "Framework/ReflectionProp.h"
#include "Framework/PropertyEd.h"
#include "Animation/Editor/AnimationGraphEditor.h"
#include "Framework/MyImguiLib.h"
class SerializeImNodeState : public IPropertySerializer
{
	// Inherited via IPropertySerializer
	virtual std::string serialize(DictWriter& out, const PropertyInfo& info, const void* inst, ClassBase* userptr) override;
	virtual void unserialize(DictParser& in, const PropertyInfo& info, void* inst, StringView token, ClassBase* userptr) override;
};
class SerializeNodeCFGRef : public IPropertySerializer
{
	// Inherited via IPropertySerializer
	virtual std::string serialize(DictWriter& out, const PropertyInfo& info, const void* inst, ClassBase* userptr) override;
	virtual void unserialize(DictParser& in, const PropertyInfo& info, void* inst, StringView token, ClassBase* userptr) override;
};

std::string SerializeNodeCFGRef::serialize(DictWriter& out, const PropertyInfo& info, const void* inst, ClassBase* userptr)
{
	ASSERT(userptr);
	AgSerializeContext* context = userptr->cast_to<AgSerializeContext>();
	ASSERT(context);

	auto node = *(BaseAGNode**)info.get_ptr(inst);
	ASSERT(context->ptr_to_index.find(node) != context->ptr_to_index.end());

	return std::to_string(context->ptr_to_index.find(node)->second);
}

void SerializeNodeCFGRef::unserialize(DictParser& in, const PropertyInfo& info, void* inst, StringView token, ClassBase* userptr)
{
	ASSERT(userptr);
	AgSerializeContext* context = userptr->cast_to<AgSerializeContext>();
	ASSERT(context);

	auto node_ptr = (BaseAGNode**)info.get_ptr(inst);
	int index = atoi(token.to_stack_string().c_str());
	ASSERT(index >= 0 && index < context->tree->all_nodes.size());
	*node_ptr = context->tree->all_nodes.at(index);
}


std::string SerializeImNodeState::serialize(DictWriter& out, const PropertyInfo& info, const void* inst, ClassBase* userptr)
{
	auto context = *(ImNodesEditorContext**)info.get_ptr(inst);
	if (!context)
		return "";

	return ImNodes::SaveEditorStateToIniString(context);
}

void SerializeImNodeState::unserialize(DictParser& in, const PropertyInfo& info, void* inst, StringView token, ClassBase* userptr)
{
	std::string inistring(token.str_start, token.str_len);
	auto context = (ImNodesEditorContext**)info.get_ptr(inst);
	if (inistring.empty())
		*context = nullptr;
	else {
		*context = ImNodes::EditorContextCreate();
		ImNodes::EditorContextSet(*context);
		ImNodes::LoadEditorStateFromIniString(*context, inistring.c_str(), inistring.size());
	}
}



template<typename FUNCTOR>
static bool drag_drop_property_ed_func(std::string* str, Color32 color, FUNCTOR&& callback, const char* targetname, const char* tooltip)
{

	//ImGui::PushStyleColor(Imguicolba)

//	ImGui::InputText("##inp", (char*)str->c_str(), str->size(), ImGuiInputTextFlags_ReadOnly);

	//ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0)); // Ensure no clip rect so mouse hover can reach FramePadding edges
	auto drawlist = ImGui::GetWindowDrawList();
	auto& style = ImGui::GetStyle();
	auto min = ImGui::GetCursorScreenPos();
	auto sz = ImGui::CalcTextSize(str->c_str());
	float width = ImGui::CalcItemWidth();
	drawlist->AddRectFilled(ImVec2(min.x - style.FramePadding.x * 0.5f, min.y), ImVec2(min.x + width, min.y + sz.y + style.FramePadding.y * 2.0), color.to_uint());
	auto cursor = ImGui::GetCursorPos();
	ImGui::Text(str->c_str());
	ImGui::SetCursorPos(cursor);
	ImGui::InvisibleButton("##adfad", ImVec2(width, sz.y + style.FramePadding.y * 2.f));
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
		ImGui::BeginTooltip();
		ImGui::Text(string_format("Drag and drop %s asset here", tooltip));
		ImGui::EndTooltip();
	}
	bool return_val = false;
	if (ImGui::BeginDragDropTarget())
	{
		//const ImGuiPayload* payload = ImGui::GetDragDropPayload();
		//if (payload->IsDataType("AssetBrowserDragDrop"))
		//	sys_print("``` accepting\n");

		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(targetname))
		{
			return_val= callback(payload->Data);

		}
		ImGui::EndDragDropTarget();
	}
	return return_val;

}


#include "Assets/AssetRegistry.h"


class FindAnimGraphVariableProp : public IPropertyEditor
{
	virtual bool internal_update() override
	{
		auto str = (VariableNameAndType*)prop->get_ptr(instance);
		return drag_drop_property_ed_func(&str->str, Color32{ 62, 27, 82 }, [&](void* payload) {

			VariableNameAndType* resource = *(VariableNameAndType**)payload;
			*str = *resource;

			return true;

			}, "AnimGraphVariableDrag", "variable");

	}
};

class FindAnimationClipPropertyEditor : public IPropertyEditor
{
public:

	// Inherited via IPropertyEditor
	virtual bool internal_update() override
	{
		ASSERT(prop->type == core_type_id::StdString);

		auto str = (std::string*)prop->get_ptr(instance);
		return drag_drop_property_ed_func(str, Color32{ 39, 56, 35 }, [&](void* payload) {

			std::string* resource = *(std::string**)payload;
			*str = *resource;

			return true;

			}, "AnimationItemAnimGraphEd", "animation");

	}
};




class AgLispCodeEditorProperty : public IPropertyEditor
{
public:
	using IPropertyEditor::IPropertyEditor;

	// Inherited via IPropertyEditor
	virtual bool internal_update() override
	{
		ASSERT(prop->type == core_type_id::StdString);

		auto script = (std::string*)prop->get_ptr(instance);

		ImguiInputTextCallbackUserStruct user;
		user.string = script;
		if (ImGui::InputTextMultiline("##source",
			(char*)script->data(),
			script->size() + 1,
			ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 4),
			ImGuiInputTextFlags_CallbackResize,
			imgui_input_text_callback_function,
			&user)) {
			script->resize(strlen(script->data()));

			return true;
		}
		return false;
	}

	bool initial = true;
	int index = 0;

};


class AgEnumFinder : public IPropertyEditor
{
public:
	using IPropertyEditor::IPropertyEditor;

	// Inherited via IPropertyEditor
	virtual bool internal_update() override
	{
		ASSERT(prop->type == core_type_id::Int32);

		ImGui::Text("Hello world\n");

		return false;
	}

};

class AgBoneFinder : public IPropertyEditor
{
public:
	AgBoneFinder()
	{
		auto model = anim_graph_ed.out.get_model();
		if (!model || !model->get_skel()) {
			no_model = true; 
			return; 
		}
		for (int i = 0; i < model->get_skel()->bone_dat.size(); i++)
			bones.push_back(model->get_skel()->bone_dat[i].strname);

	}

	// Inherited via IPropertyEditor
	virtual bool internal_update() override
	{
		ASSERT(prop->type == core_type_id::StdString);
		std::string* str = (std::string*)prop->get_ptr(instance);
		bool ret = false;
		if (no_model)
			ImGui::TextColored(ImVec4(0.5, 0.5, 0.5, 1.0), "no model set");
		else if(bones.empty())
			ImGui::TextColored(ImVec4(0.5, 0.5, 0.5, 1.0), "model has no bones");
		else {

			if (ImGui::BeginCombo("##bones", str->c_str())) {
				for (int i = 0; i < bones.size(); i++) {
					bool is_selected = *str == bones[i];
					if (ImGui::Selectable(bones[i].c_str(), &is_selected)) {
						*str = bones[i];
						ret = true;
					}
				}
				ImGui::EndCombo();
			}

		}
		return ret;
	}
	bool no_model = false;
	// copy in as std strings, could be c_strs but that opens up more room for bugs
	std::vector<std::string> bones;
};




class AgEditor_BlendSpaceArrayHead : public IArrayHeader
{
	using IArrayHeader::IArrayHeader;
	// Inherited via IArrayHeader
	virtual bool imgui_draw_header(int index)
	{
		return false;
	}
	virtual void imgui_draw_closed_body(int index)
	{
	}
};

class AgEdtior_BlendSpaceParameteriation : public IPropertyEditor
{
	using IPropertyEditor::IPropertyEditor;
	// Inherited via IPropertyEditor
	virtual bool internal_update() override
	{

		return false;

	}
};
#include "Blendspace_nodes.h"
class BlendspaceGridEd : public IPropertyEditor
{
	virtual bool internal_update() override
	{
		//auto str = (VariableNameAndType*)prop->get_ptr(instance);
		//drag_drop_property_ed_func(&str->str, Color32{ 62, 27, 82 }, [&](void* payload) {
		//
		//	VariableNameAndType* resource = *(VariableNameAndType**)payload;
		//	*str = *resource;
		//
		//	}, "AnimGraphVariableDrag", "variable");
		Blendspace2d_EdNode* node = (Blendspace2d_EdNode*)instance;
		if (node->cols == 0 || node->rows == 0)	// only happens on startup
			node->resize_grid(1, 1);
				
		auto drawlist = ImGui::GetWindowDrawList();
		auto& style = ImGui::GetStyle();


		const Color32 unusedc = { 42, 48, 39 };
		const Color32 usedc = { 124, 150, 111 };
		const Color32 hoveredc = { 187, 196, 61 };
		const float square_width = 48;

		static int x_popup_idx = 0;
		static int y_popup_idx = 0;


		for (int y = 0; y < node->rows; y++) {
			for (int x = 0; x < node->cols; x++) {

				auto min = ImGui::GetCursorScreenPos();

				const int index = y * node->cols + x;
				auto& gridpoint = node->gridpoints[index];
				const bool hovered = ImRect(min, min + ImVec2(square_width, square_width)).Contains(ImGui::GetMousePos());
				Color32 color_to_use = unusedc;
				if (hovered) color_to_use = hoveredc;
				else if (!gridpoint.animation_name.empty()) color_to_use = usedc;

				drawlist->AddRectFilled(ImVec2(min.x, min.y), ImVec2(min.x + square_width, min.y + square_width), color_to_use.to_uint());
				
				ImGui::PushClipRect(min, min + ImVec2(square_width, square_width),false);
				if (!gridpoint.animation_name.empty())
					drawlist->AddText(NULL, 0.0f, min, COLOR_WHITE.to_uint(), gridpoint.animation_name.c_str(), nullptr, square_width);
				ImGui::PopClipRect();

				ImGui::InvisibleButton("##adfad", ImVec2(square_width,square_width));
				if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
					if (!gridpoint.animation_name.empty()) {
						ImGui::BeginTooltip();
							ImGui::Text(gridpoint.animation_name.c_str());
						ImGui::EndTooltip();
					}
					if (ImGui::GetIO().MouseClicked[1]) {
						ImGui::OpenPopup("blendspace_grid_popup");
						x_popup_idx = x;
						y_popup_idx = y;
					}
				}
				if (ImGui::BeginDragDropTarget())
				{
					//const ImGuiPayload* payload = ImGui::GetDragDropPayload();
					//if (payload->IsDataType("AssetBrowserDragDrop"))
					//	sys_print("``` accepting\n");

					if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("AnimationItemAnimGraphEd"))
					{
						std::string* p = *(std::string**)payload->Data;
						gridpoint.animation_name = *p;

					}
					ImGui::EndDragDropTarget();
				}

				if (x != node->cols-1)
					ImGui::SameLine();
			}
		}

		if (ImGui::BeginPopup("blendspace_grid_popup")) {

			bool close_popup = false;
			auto& gridpoint = node->gridpoints[y_popup_idx * node->cols + x_popup_idx];
			if (!gridpoint.animation_name.empty() && ImGui::SmallButton("remove animation")) {
				gridpoint.animation_name = "";
				close_popup = true;
			}
			ImGui::Separator();

			int gridpoints_before = node->gridpoints.size();

			if (ImGui::SmallButton("add row above"))
				node->append_row_to_grid(y_popup_idx);
			if (ImGui::SmallButton("add row below"))
				node->append_row_to_grid(y_popup_idx+1);
			if (node->rows > 1 && ImGui::SmallButton("remove this row"))
				node->remove_row_from_grid(y_popup_idx);
			
			ImGui::Separator();

			if (ImGui::SmallButton("add col left"))
				node->append_col_to_grid(x_popup_idx);
			if (ImGui::SmallButton("add col right"))
				node->append_col_to_grid(x_popup_idx + 1);
			if (node->cols > 1 && ImGui::SmallButton("remove this col"))
				node->remove_col_from_grid(x_popup_idx);


			int gridpoints_after = node->gridpoints.size();
			if (gridpoints_after != gridpoints_before || close_popup)
				ImGui::CloseCurrentPopup();

			ImGui::EndPopup();
		}


		return false;
	}
};

struct AutoStruct_asdf {
	AutoStruct_asdf() {
		auto& pfac = IPropertyEditor::get_factory();

		pfac.registerClass<FindAnimationClipPropertyEditor>("AG_CLIP_TYPE");
		pfac.registerClass<FindAnimGraphVariableProp>("FindAnimGraphVariableProp");
		pfac.registerClass<AgBoneFinder>("AgBoneFinder");

		pfac.registerClass<BlendspaceGridEd>("BlendspaceGridEd");

		pfac.registerClass<AgLispCodeEditorProperty>("AG_LISP_CODE");
		pfac.registerClass<AgEnumFinder>("AG_ENUM_TYPE_FINDER");

		pfac.registerClass<AgEdtior_BlendSpaceParameteriation>("AG_EDITOR_BLEND_SPACE_PARAMETERIZATION");
		

		auto& afac = IArrayHeader::get_factory();

		afac.registerClass<AgEditor_BlendSpaceArrayHead>("AG_EDITOR_BLEND_SPACE");


		auto& sfac = IPropertySerializer::get_factory();

		sfac.registerClass<SerializeImNodeState>("SerializeImNodeState");
		sfac.registerClass<SerializeNodeCFGRef>("SerializeNodeCFGRef");

	}
};

static AutoStruct_asdf add_to_factories_asdf;
#endif

