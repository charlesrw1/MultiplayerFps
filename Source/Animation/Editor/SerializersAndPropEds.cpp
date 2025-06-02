#ifdef EDITOR_BUILD
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_internal.h"

#include "Framework/ReflectionProp.h"
#include "Framework/PropertyEd.h"
#include "Animation/Editor/AnimationGraphEditor.h"
#include "Framework/MyImguiLib.h"
#include "Assets/AssetDatabase.h"

class SerializeImNodeState : public IPropertySerializer
{
	// Inherited via IPropertySerializer
	virtual std::string serialize(DictWriter& out, const PropertyInfo& info, const void* inst, ClassBase* userptr) override;
	virtual void unserialize(DictParser& in, const PropertyInfo& info, void* inst, StringView token, ClassBase* userptr, IAssetLoadingInterface*) override;
};
class SerializeNodeCFGRef : public IPropertySerializer
{
	// Inherited via IPropertySerializer
	virtual std::string serialize(DictWriter& out, const PropertyInfo& info, const void* inst, ClassBase* userptr) override;
	virtual void unserialize(DictParser& in, const PropertyInfo& info, void* inst, StringView token, ClassBase* userptr, IAssetLoadingInterface*) override;
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

void SerializeNodeCFGRef::unserialize(DictParser& in, const PropertyInfo& info, void* inst, StringView token, ClassBase* userptr, IAssetLoadingInterface*)
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

void SerializeImNodeState::unserialize(DictParser& in, const PropertyInfo& info, void* inst, StringView token, ClassBase* userptr, IAssetLoadingInterface*)
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





#include "Assets/AssetRegistry.h"

struct AutoStruct_asdf {
	AutoStruct_asdf() {
		
		auto& afac = IArrayHeader::get_factory();



		auto& sfac = IPropertySerializer::get_factory();

		sfac.registerClass<SerializeImNodeState>("SerializeImNodeState");
		sfac.registerClass<SerializeNodeCFGRef>("SerializeNodeCFGRef");

	}
};

static AutoStruct_asdf add_to_factories_asdf;
#endif

