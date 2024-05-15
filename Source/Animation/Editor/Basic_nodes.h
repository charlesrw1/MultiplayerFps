#pragma once
#include "AnimationGraphEditor.h"
#include "Base_node.h"

#include "../Runtime/AnimationTreeLocal.h"

template<typename T>
inline void util_create_node(T*& node)
{
	node = new T();
	ed.editing_tree->all_nodes.push_back(node);
}

template<typename T>
inline void util_ensure_type(T* saved)
{
	Node_CFG* cfg = saved;
	ASSERT(strcmp(cfg->get_typeinfo().name, T().get_typeinfo().name) == 0);
}


inline void util_draw_param_for_topbar(ControlParamHandle handle)
{
	auto param = ed.control_params.get_parameter_for_ed_id(handle.id);
	if (param) 
		ImGui::TextColored(scriptparamtype_to_color(param->type), string_format(":= %s", param->name.c_str()));
}

template<typename T>
inline bool util_create_or_ensure(T*& node)
{
	if (node) {
		util_ensure_type(node);
		return false;
	}
	else {
		util_create_node(node);
		return true;
	}
}

inline void util_default_init(Base_EdNode* ednode, Node_CFG* node, bool is_first)
{
	for (int i = 0; i < node->input.size(); i++) {
		Node_CFG* node_cfg = node->input[i];
		if (!node_cfg)
			continue;
		Base_EdNode* ed_node = ed.editor_node_for_cfg_node(node_cfg);

		if (!ed_node) {
			printf("!!! couldn't find editor node for cfg !!! (data read wrong from disk or out of date?)\n");

			ASSERT(0);
			// TODO: create the new editor node
		}

		ednode->add_input(&ed, ed_node, i);
	}

}

inline ControlParamHandle util_get_real_param(ControlParamHandle oldhandle) {
	if (!oldhandle.is_valid()) return oldhandle;
	int paramid = ed.control_params.get_index_of_prop_for_compiling(oldhandle.id).id;
	return { paramid };
}

inline control_param_type util_get_param_type(ControlParamHandle handle) {
	ASSERT(handle.is_valid());
	return ed.editing_tree->params->get_type(handle);
}

inline bool util_compile_default(ControlParamHandle* param, bool param_is_boolfloat, int num_inputs, Base_EdNode* node, const AgSerializeContext* ctx)
{
	auto cfg = node->get_graph_node();

	if (param) {
		*param = util_get_real_param(*param);

		if (param->is_valid()) {
			auto type = util_get_param_type(*param);

			if (param_is_boolfloat) {
				if (type != control_param_type::bool_t && type != control_param_type::float_t)
					node->append_fail_msg("[ERROR] param must be bool or float type");
			}
			else {
				if (type != control_param_type::int_t && type != control_param_type::enum_t)
					node->append_fail_msg("[ERROR] param must be int or enum type");
			}
		}
		else {
			node->append_fail_msg("[ERROR] needs param");
		}
	}

	cfg->input.resize(num_inputs);

	bool missing_inputs = false;
	for (int i = 0; i < num_inputs; i++) {
		Node_CFG* other_ptr = (node->inputs[i]) ? node->inputs[i]->get_graph_node() : nullptr;

		cfg->input[i] = ptr_to_serialized_nodecfg_ptr(other_ptr, ctx);
		missing_inputs |= node->inputs[i] == nullptr;
	}
	if (missing_inputs)
		node->append_fail_msg("[ERROR] missing required inputs");

	return node->has_errors();

}

#define MAKE_STANDARD_FUNCTIONS(name, color, desc, num_inputs) \
	std::string get_name() const override { return name;} \
	Color32 get_node_color() const override { return color; } \
	int get_num_inputs() const override { return num_inputs; } \
	std::string get_tooltip() const override { return desc; } \
	Node_CFG* get_graph_node() override { return node; }

#define MAKE_STANARD_SERIALIZE(type_name) \
	static PropertyInfoList* get_props() { \
		START_PROPS(type_name) \
			REG_STRUCT_CUSTOM_TYPE(node, PROP_SERIALIZE, "SerializeNodeCFGRef") \
		END_PROPS(type_name) \
	}

#define MAKE_STANDARD_INIT() \
	void init() override { \
		util_create_or_ensure(node);\
		util_default_init(this, node, is_this_node_created()); \
		clear_newly_created(); \
	}

#define MAKE_STANDARD_ADD_PROPS(type_name) \
	void add_props(std::vector<PropertyListInstancePair>& props) override { \
		Base_EdNode::add_props(props); \
		props.push_back({ type_name::get_props(), this }); \
	} \
	void add_props_for_editable_element(std::vector<PropertyListInstancePair>& props) override { \
		props.push_back({node->get_props(), node } );\
	} 

class Clip_EdNode : public Base_EdNode
{
public:
	EDNODE_HEADER(Clip_EdNode);
	MAKE_STANARD_SERIALIZE(Clip_EdNode);
	MAKE_STANDARD_FUNCTIONS(
		"Clip", 
		SOURCE_COLOR, 
		"placeholder", 
		0
	);
	MAKE_STANDARD_INIT();
	MAKE_STANDARD_ADD_PROPS(Clip_EdNode);
	
	std::string get_title() const override {
		if (node->clip_name.empty()) return get_name();
		return node->clip_name;
	}
	
	bool compile_my_data(const AgSerializeContext* ctx) override {
		if (node->clip_name.empty())
			append_fail_msg("[ERROR] clip name is empty\n");

		return has_errors();
	}

	Clip_Node_CFG* node = nullptr;
};

class Blend_EdNode : public Base_EdNode
{
	EDNODE_HEADER(Blend_EdNode);
	MAKE_STANARD_SERIALIZE(Blend_EdNode);
	MAKE_STANDARD_FUNCTIONS(
		"Blend",
		BLEND_COLOR,
		"placeholder",
		2
	);

	MAKE_STANDARD_INIT();
	MAKE_STANDARD_ADD_PROPS(Blend_EdNode);

	void draw_node_topbar() override {
		util_draw_param_for_topbar(node->param);
	}

	bool compile_my_data(const AgSerializeContext* ctx) override {
		return util_compile_default(&node->param, true, 2, this, ctx);
	}

	Blend_Node_CFG* node = nullptr;
};

class Blend_int_EdNode : public Base_EdNode
{
	EDNODE_HEADER(Blend_int_EdNode);
	MAKE_STANARD_SERIALIZE(Blend_int_EdNode);
	MAKE_STANDARD_INIT();
	MAKE_STANDARD_ADD_PROPS(Blend_int_EdNode);

	Node_CFG* get_graph_node() override { return node; }
	std::string get_name() const override { return "Blend by int"; }
	Color32 get_node_color() const override { return BLEND_COLOR; }
	int get_num_inputs() const override { return num_int_inputs; }
	std::string get_input_pin_name(int index) const override {
		ASSERT(index < num_int_inputs);
		return std::to_string(index);
	}
	void draw_node_topbar() override {
		util_draw_param_for_topbar(node->param);

		auto param = ed.control_params.get_parameter_for_ed_id(node->param.id);
		if (!node->param.is_valid() || (param && param->type == control_param_type::int_t)) {

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
		return util_compile_default(&node->param, false, num_int_inputs, this, ctx);
	}

	int num_int_inputs = 0;
	Blend_Int_Node_CFG* node = nullptr;
};

class Additive_EdNode : public Base_EdNode
{
	EDNODE_HEADER(Additive_EdNode);
	MAKE_STANARD_SERIALIZE(Additive_EdNode);
	MAKE_STANDARD_FUNCTIONS(
		"Additive",
		ADD_COLOR,
		"placeholder",
		2
	);
	MAKE_STANDARD_INIT();
	MAKE_STANDARD_ADD_PROPS(Additive_EdNode);

	std::string get_input_pin_name(int index) const override {
		if (index == 0) return "Delta";
		if (index == 1) return "Base";
		ASSERT(0);
	}
	void draw_node_topbar() override {
		util_draw_param_for_topbar(node->param);
	}
	bool compile_my_data(const AgSerializeContext* ctx) override {
		return util_compile_default(&node->param, true, 2, this, ctx);
	}

	Add_Node_CFG* node = nullptr;
};

class Subtract_EdNode : public Base_EdNode
{
	EDNODE_HEADER(Subtract_EdNode);
	MAKE_STANARD_SERIALIZE(Subtract_EdNode);
	MAKE_STANDARD_FUNCTIONS(
		"Subtract",
		ADD_COLOR,
		"placeholder",
		2
	);

	MAKE_STANDARD_INIT();
	MAKE_STANDARD_ADD_PROPS(Subtract_EdNode);


	std::string get_input_pin_name(int index) const override {
		if (index == 0) return "Reference";
		if (index == 1) return "Source";
		ASSERT(0);
	}

	bool compile_my_data(const AgSerializeContext* ctx) override {
		return util_compile_default(nullptr, true, 2, this, ctx);
	}

	Subtract_Node_CFG* node = nullptr;
};

class Mirror_EdNode : public Base_EdNode
{
	EDNODE_HEADER(Mirror_EdNode);
	MAKE_STANARD_SERIALIZE(Mirror_EdNode);
	MAKE_STANDARD_FUNCTIONS(
		"Mirror",
		MISC_COLOR,
		"placeholder",
		1
	);
	MAKE_STANDARD_INIT();
	MAKE_STANDARD_ADD_PROPS(Mirror_EdNode);


	void draw_node_topbar() override {
		util_draw_param_for_topbar(node->param);
	}

	bool compile_my_data(const AgSerializeContext* ctx) override {
		return util_compile_default(&node->param, true, 1, this, ctx);
	}

	Mirror_Node_CFG* node = nullptr;
};

class Blend_Layered_EdNode : public Base_EdNode
{
	EDNODE_HEADER(Blend_Layered_EdNode);
	MAKE_STANDARD_FUNCTIONS(
		"Blend Layered",
		BLEND_COLOR,
		"placeholder",
		2
	);
	MAKE_STANDARD_INIT();
	MAKE_STANDARD_ADD_PROPS(Blend_Layered_EdNode);


	void draw_node_topbar() override {
		ImGui::TextColored(ImVec4(0.7, 0.7, 0.7, 1.0), maskname.c_str());
		util_draw_param_for_topbar(node->param);
	}

	bool compile_my_data(const AgSerializeContext* ctx) override {
		node->maskname = StringName(maskname.c_str());
		return util_compile_default(&node->param, true, 2, this, ctx);
	}

	static PropertyInfoList* get_props() {
		START_PROPS(Blend_Layered_EdNode)
			REG_STDSTRING(maskname,PROP_DEFAULT),
			REG_STRUCT_CUSTOM_TYPE(node, PROP_SERIALIZE, "SerializeNodeCFGRef"),
		END_PROPS(Blend_Layered_EdNode)
	}

	std::string maskname = "";
	Blend_Masked_CFG* node = nullptr;
};

class Sync_EdNode : public Base_EdNode
{
	EDNODE_HEADER(Sync_EdNode);
	MAKE_STANARD_SERIALIZE(Sync_EdNode);
	MAKE_STANDARD_FUNCTIONS(
		"Sync",
		MISC_COLOR,
		"placeholder",
		1
	);
	MAKE_STANDARD_INIT();
	MAKE_STANDARD_ADD_PROPS(Sync_EdNode);

	bool compile_my_data(const AgSerializeContext* ctx) override {
		return util_compile_default(nullptr, true, 1, this, ctx);
	}
	
	Sync_Node_CFG* node = nullptr;
};