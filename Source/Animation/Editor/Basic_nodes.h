#pragma once
#include "AnimationGraphEditor.h"
#include "Base_node.h"

#include "../Runtime/AnimationTreeLocal.h"




template<typename T>
inline bool util_create_or_ensure(T*& node)
{
	if (node) {
		ASSERT(node->get_type() == T::StaticType);
		return false;
	}
	else {
		ed.util_create_node(node);
		return true;
	}
}

inline void util_default_init(Base_EdNode* ednode, BaseAGNode* node, bool is_first)
{
	// init nodes by inspecting reflected node properties
	ednode->init_graph_nodes_from_node();

	// find corresponding node in runtime graph
	for (int i = 0; i < ednode->inputs.size(); i++) {
		if (!ednode->inputs[i].prop_link)
			continue;

		BaseAGNode** node_prop = (BaseAGNode**)ednode->inputs[i].prop_link->get_ptr(node);

		if (!*node_prop)
			continue;
		
		Base_EdNode* ed_node = ed.editor_node_for_cfg_node(*node_prop);

		if (!ed_node) {
			printf("!!! couldn't find editor node for cfg !!! (data read wrong from disk or out of date?)\n");

			ASSERT(0);
			// TODO: create the new editor node
		}

		ednode->inputs[i].node = ed_node;
	}
}


inline bool util_compile_default(Base_EdNode* node, const AgSerializeContext* ctx)
{
	auto cfg = node->get_graph_node();

	auto props = cfg->get_type().props;

	int input_index = 0;
	bool missing_inputs = false;
	for (int i = 0; i < node->inputs.size(); i++) {
		if (!node->inputs[i].prop_link)
			continue;
		auto& input = node->inputs[i];
		auto& prop = *input.prop_link;

		Base_EdNode* other_ed_node = input.node;

		BaseAGNode* other = (other_ed_node) ? other_ed_node->get_graph_node() : nullptr;

		BaseAGNode** ptr_to_ptr = (BaseAGNode**)prop.get_ptr(cfg);
		if (other_ed_node && !(other_ed_node->get_output_pin_type() == input.type)) {
			node->append_fail_msg("[ERROR] node input is wrong type (this should have not errored)");
			*ptr_to_ptr = ptr_to_serialized_nodecfg_ptr(nullptr, ctx);
		}
		else {
			*ptr_to_ptr = ptr_to_serialized_nodecfg_ptr(other, ctx);
		}

		if (!other)
			missing_inputs = true;

		input_index++;
	}
	if (missing_inputs)
		node->append_fail_msg("[ERROR] node is missing inputs");

	return node->has_errors();
}

#define MAKE_STANDARD_FUNCTIONS(name, color, desc) \
	std::string get_name() const override { return name;} \
	Color32 get_node_color() const override { return color; } \
	std::string get_tooltip() const override { return desc; }

#define MAKE_OUTPUT_TYPE(pin_type) \
	virtual GraphPinType get_output_pin_type() const override { return GraphPinType(GraphPinType::pin_type); }

#define MAKE_STANARD_SERIALIZE(type_name) \
	static PropertyInfoList* get_props() { \
		START_PROPS(type_name) \
			REG_STRUCT_CUSTOM_TYPE(node, PROP_SERIALIZE, "SerializeNodeCFGRef") \
		END_PROPS(type_name) \
	}


template<typename T>
class BaseNodeUtil_EdNode : public Base_EdNode
{
public:
	bool compile_my_data(const AgSerializeContext* ctx) override {
		return util_compile_default(this, ctx);
	}
	void init() override {
		util_create_or_ensure<T>(node);
		util_default_init(this, node, is_this_node_created());
		clear_newly_created();
	}
	GraphPinType get_output_pin_type() const override { return GraphPinType(GraphPinType::localspace_pose); }
	BaseAGNode* get_graph_node() override { return node; }

	T* node = nullptr;
};

class Clip_EdNode : public BaseNodeUtil_EdNode<Clip_Node_CFG>
{
public:
	CLASS_HEADER();

	MAKE_STANDARD_FUNCTIONS(
		"Clip", 
		SOURCE_COLOR, 
		"placeholder", 
	);
	MAKE_STANARD_SERIALIZE(Clip_EdNode);
	
	bool compile_my_data(const AgSerializeContext* ctx) override {
		if (node->clip_name.empty())
			append_fail_msg("[ERROR] clip name is empty\n");

		return has_errors();
	}
	
	std::string get_title() const override {
		if (node->clip_name.empty()) return get_name();
		return node->clip_name;
	}
};

class Blend_EdNode : public BaseNodeUtil_EdNode<Blend_Node_CFG>
{
public:
	CLASS_HEADER();

	MAKE_STANDARD_FUNCTIONS(
		"Blend",
		BLEND_COLOR,
		"placeholder",
	);
	MAKE_STANARD_SERIALIZE(Blend_EdNode);
};

class Blend_int_EdNode : public BaseNodeUtil_EdNode<Blend_Int_Node_CFG>
{
public:
	CLASS_HEADER();

	MAKE_STANDARD_FUNCTIONS(
		"Blend By Int",
		BLEND_COLOR,
		"placeholder",
		);
	MAKE_STANARD_SERIALIZE(Blend_int_EdNode);

	void init() override {
		util_create_or_ensure(node);
		// creates int parameter
		util_default_init(this, node, is_this_node_created());
		// TODO: create array params

		clear_newly_created();
	}

	void draw_node_topbar() override {

		{

			ImGui::PushStyleColor(ImGuiCol_Button, Color32{ 0xff,0xff,0xff,50 }.to_uint() );
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Color32{ 0xff,0xff,0xff,128 }.to_uint() );
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, Color32{ 0xff,0xff,0xff,50 }.to_uint() );


			if (ImGui::SmallButton("Add") && num_int_inputs < MAX_INPUTS) {
				num_int_inputs += 1;
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("Remove") && num_int_inputs > 0) {
				on_remove_pin(num_int_inputs - 1, true);
				on_post_remove_pins();
				num_int_inputs -= 1;
			}

			ImGui::PopStyleColor(3);
		}
	}

	bool compile_my_data(const AgSerializeContext* ctx) override {
		// FIXME:
		return util_compile_default(this, ctx);

	}

	int num_int_inputs = 0;
};

class Additive_EdNode : public BaseNodeUtil_EdNode<Add_Node_CFG>
{
public:
	CLASS_HEADER();

	MAKE_STANARD_SERIALIZE(Additive_EdNode);
	MAKE_STANDARD_FUNCTIONS(
		"Additive",
		ADD_COLOR,
		"placeholder",
	);
};

class Subtract_EdNode : public BaseNodeUtil_EdNode<Subtract_Node_CFG>
{
public:
	CLASS_HEADER();

	MAKE_STANARD_SERIALIZE(Subtract_EdNode);
	MAKE_STANDARD_FUNCTIONS(
		"Subtract",
		ADD_COLOR,
		"placeholder",
	);
};

class Mirror_EdNode : public BaseNodeUtil_EdNode<Mirror_Node_CFG>
{
public:
	CLASS_HEADER();

	MAKE_STANARD_SERIALIZE(Mirror_EdNode);
	MAKE_STANDARD_FUNCTIONS(
		"Mirror",
		MISC_COLOR,
		"placeholder",
	);


};

class Blend_Layered_EdNode : public BaseNodeUtil_EdNode<Blend_Masked_CFG>
{
public:
	CLASS_HEADER();

	MAKE_STANDARD_FUNCTIONS(
		"Blend Layered",
		BLEND_COLOR,
		"placeholder",
	);

	void draw_node_topbar() override {
		ImGui::TextColored(ImVec4(0.7, 0.7, 0.7, 1.0), maskname.c_str());
	}

	//  custom compile
	bool compile_my_data(const AgSerializeContext* ctx) override {
		node->maskname = StringName(maskname.c_str());
		return util_compile_default(this, ctx);
	}

	// custom serialize
	static const PropertyInfoList* get_props() {
		START_PROPS(Blend_Layered_EdNode)
			REG_STDSTRING(maskname,PROP_DEFAULT),
			REG_STRUCT_CUSTOM_TYPE(node, PROP_SERIALIZE, "SerializeNodeCFGRef"),
		END_PROPS(Blend_Layered_EdNode)
	}

	std::string maskname = "";
};

class Sync_EdNode : public BaseNodeUtil_EdNode<Sync_Node_CFG>
{
public:
	CLASS_HEADER();
	
	MAKE_STANDARD_FUNCTIONS(
		"Sync",
		MISC_COLOR,
		"placeholder",
	);

	// serialize
	MAKE_STANARD_SERIALIZE(Sync_EdNode);
};