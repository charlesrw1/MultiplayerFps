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


struct AutoStruct_asdf {
	AutoStruct_asdf() {
		auto& pfac = IPropertyEditor::get_factory();

		pfac.registerClass<FindAnimationClipPropertyEditor>("AG_CLIP_TYPE");
		pfac.registerClass<FindAnimGraphVariableProp>("FindAnimGraphVariableProp");

		pfac.registerClass<FindModelForEdAnimG>("FindModelForEdAnimG");


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


