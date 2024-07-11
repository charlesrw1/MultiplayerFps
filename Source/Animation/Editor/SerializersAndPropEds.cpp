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
	virtual std::string serialize(DictWriter& out, const PropertyInfo& info, void* inst, TypedVoidPtr userptr) override;
	virtual void unserialize(DictParser& in, const PropertyInfo& info, void* inst, StringView token, TypedVoidPtr userptr) override;
};
class SerializeNodeCFGRef : public IPropertySerializer
{
	// Inherited via IPropertySerializer
	virtual std::string serialize(DictWriter& out, const PropertyInfo& info, void* inst, TypedVoidPtr userptr) override;
	virtual void unserialize(DictParser& in, const PropertyInfo& info, void* inst, StringView token, TypedVoidPtr userptr) override;
};

std::string SerializeNodeCFGRef::serialize(DictWriter& out, const PropertyInfo& info, void* inst, TypedVoidPtr userptr)
{
	ASSERT(userptr.name == NAME("AgSerializeContext"));
	AgSerializeContext* context = (AgSerializeContext*)userptr.ptr;
	auto node = *(BaseAGNode**)info.get_ptr(inst);
	ASSERT(context->ptr_to_index.find(node) != context->ptr_to_index.end());

	return std::to_string(context->ptr_to_index.find(node)->second);
}

void SerializeNodeCFGRef::unserialize(DictParser& in, const PropertyInfo& info, void* inst, StringView token, TypedVoidPtr userptr)
{
	ASSERT(userptr.name == NAME("AgSerializeContext"));
	AgSerializeContext* context = (AgSerializeContext*)userptr.ptr;
	auto node_ptr = (BaseAGNode**)info.get_ptr(inst);
	int index = atoi(token.to_stack_string().c_str());
	ASSERT(index >= 0 && index < context->tree->all_nodes.size());
	*node_ptr = context->tree->all_nodes.at(index);
}


std::string SerializeImNodeState::serialize(DictWriter& out, const PropertyInfo& info, void* inst, TypedVoidPtr userptr)
{
	auto context = *(ImNodesEditorContext**)info.get_ptr(inst);
	if (!context)
		return "";

	return ImNodes::SaveEditorStateToIniString(context);
}

void SerializeImNodeState::unserialize(DictParser& in, const PropertyInfo& info, void* inst, StringView token, TypedVoidPtr userptr)
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


class AnimatorInstanceParentEditor : public IPropertyEditor
{
public:
	// Inherited via IPropertyEditor
	virtual void internal_update() override
	{
		ASSERT(prop->type == core_type_id::StdString);

		auto str = (std::string*)prop->get_ptr(instance);

		if (initial) {
			auto iter = ClassBase::get_subclasses<AnimatorInstance>();
			for (; !iter.is_end(); iter.next()) {
				if (iter.get_type()->allocate)
					options.push_back(iter.get_type()->classname);
			}

			index = 0;
			for (int i = 0; i < options.size(); i++) {
				if (*str == options.at(i)) {
					index = i;
					break;
				}
			}
			initial = false;
		}
		int prev_index = index;
		if (ImGui::Combo("##combo", &index, options.data(), options.size())) {
			if (index != prev_index && index >= 0) {
				*str = options[index];

				ed.set_animator_instance_from_string(*str);
			}
		}
	}

	std::vector<const char*> options;
	bool initial = true;
	int index = 0;
};

template<typename FUNCTOR>
static void drag_drop_property_ed_func(std::string* str, Color32 color, FUNCTOR&& callback, const char* targetname, const char* tooltip)
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
	if (ImGui::BeginDragDropTarget())
	{
		//const ImGuiPayload* payload = ImGui::GetDragDropPayload();
		//if (payload->IsDataType("AssetBrowserDragDrop"))
		//	sys_print("``` accepting\n");

		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(targetname))
		{
			callback(payload->Data);

		}
		ImGui::EndDragDropTarget();
	}

}


#include "AssetRegistry.h"
class FindModelForEdAnimG : public IPropertyEditor
{
public:
	using IPropertyEditor::IPropertyEditor;


	// Inherited via IPropertyEditor
	virtual void internal_update() override
	{
		ASSERT(prop->type == core_type_id::StdString);

		auto str = (std::string*)prop->get_ptr(instance);
		drag_drop_property_ed_func(str, Color32{ 11, 50, 94 }, [&](void* payload) {

			AssetOnDisk* resource = *(AssetOnDisk**)payload;
			if (resource->type->get_type_name() == "Model") {
				*str = resource->filename;
				ed.set_model_from_str(*str);
			}


			}, "AssetBrowserDragDrop", "model");

	}
};

class FindAnimGraphVariableProp : public IPropertyEditor
{
	virtual void internal_update() override
	{
		auto str = (VariableNameAndType*)prop->get_ptr(instance);
		drag_drop_property_ed_func(&str->str, Color32{ 62, 27, 82 }, [&](void* payload) {

			VariableNameAndType* resource = *(VariableNameAndType**)payload;
			*str = *resource;

			}, "AnimGraphVariableDrag", "variable");

	}
};

class FindAnimationClipPropertyEditor : public IPropertyEditor
{
public:

	// Inherited via IPropertyEditor
	virtual void internal_update() override
	{
		ASSERT(prop->type == core_type_id::StdString);

		auto str = (std::string*)prop->get_ptr(instance);
		drag_drop_property_ed_func(str, Color32{ 39, 56, 35 }, [&](void* payload) {

			std::string* resource = *(std::string**)payload;
			*str = *resource;

			}, "AnimationItemAnimGraphEd", "animation");

	}
};




class AgLispCodeEditorProperty : public IPropertyEditor
{
public:
	using IPropertyEditor::IPropertyEditor;

	// Inherited via IPropertyEditor
	virtual void internal_update() override
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
		}

	}

	bool initial = true;
	int index = 0;

};


class AgEnumFinder : public IPropertyEditor
{
public:
	using IPropertyEditor::IPropertyEditor;

	// Inherited via IPropertyEditor
	virtual void internal_update() override
	{
		ASSERT(prop->type == core_type_id::Int32);

		ImGui::Text("Hello world\n");
	}

};

class AgBoneFinder : public IPropertyEditor
{
public:
	AgBoneFinder()
	{
		auto model = ed.out.get_model();
		if (!model || !model->get_skel()) {
			no_model = true; 
			return; 
		}
		for (int i = 0; i < model->get_skel()->bone_dat.size(); i++)
			bones.push_back(model->get_skel()->bone_dat[i].strname);

	}

	// Inherited via IPropertyEditor
	virtual void internal_update() override
	{
		ASSERT(prop->type == core_type_id::StdString);
		std::string* str = (std::string*)prop->get_ptr(instance);
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
					}
				}
				ImGui::EndCombo();
			}

		}
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
	virtual void internal_update() override
	{

		std::vector<ImVec2> verts;
		std::vector<const char*> names;

		std::vector<int> indicies;

		verts.push_back(ImVec2(0.5, 0.5));
		names.push_back("[0]");

		verts.push_back(ImVec2(0, 0));
		names.push_back("[1]");

		verts.push_back(ImVec2(0, 1));
		names.push_back("[2]");

		verts.push_back(ImVec2(1, 1));
		names.push_back("[3]");

		verts.push_back(ImVec2(1, 0));
		names.push_back("[4]");

		indicies.push_back(0);
		indicies.push_back(1);
		indicies.push_back(2);

		indicies.push_back(0);
		indicies.push_back(1);
		indicies.push_back(4);



		//MyImDrawBlendSpace("##label", verts, indicies, names, ImVec2(0, 0), ImVec2(1, 1), nullptr);

	}
};
#include "Blendspace_nodes.h"
class BlendspaceGridEd : public IPropertyEditor
{
	virtual void internal_update() override
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

	}
};

struct AutoStruct_asdf {
	AutoStruct_asdf() {
		auto& pfac = IPropertyEditor::get_factory();

		pfac.registerClass<FindAnimationClipPropertyEditor>("AG_CLIP_TYPE");
		pfac.registerClass<FindAnimGraphVariableProp>("FindAnimGraphVariableProp");
		pfac.registerClass<AgBoneFinder>("AgBoneFinder");
		pfac.registerClass<FindModelForEdAnimG>("FindModelForEdAnimG");
		pfac.registerClass<BlendspaceGridEd>("BlendspaceGridEd");

		pfac.registerClass<AgLispCodeEditorProperty>("AG_LISP_CODE");
		pfac.registerClass<AgEnumFinder>("AG_ENUM_TYPE_FINDER");

		pfac.registerClass<AgEdtior_BlendSpaceParameteriation>("AG_EDITOR_BLEND_SPACE_PARAMETERIZATION");
		
		pfac.registerClass<AnimatorInstanceParentEditor>("AnimatorInstanceParentEditor");

		auto& afac = IArrayHeader::get_factory();

		afac.registerClass<AgEditor_BlendSpaceArrayHead>("AG_EDITOR_BLEND_SPACE");


		auto& sfac = IPropertySerializer::get_factory();

		sfac.registerClass<SerializeImNodeState>("SerializeImNodeState");
		sfac.registerClass<SerializeNodeCFGRef>("SerializeNodeCFGRef");

	}
};

static AutoStruct_asdf add_to_factories_asdf;


