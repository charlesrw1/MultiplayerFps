#pragma once

#include <cstdint>
#include <array>
#include <vector>
#include <string>
#include "imgui.h"

#include "../Runtime/AnimationTreeLocal.h"
#include "Framework/Factory.h"
#include "Framework/TypeInfo.h"
#include "Framework/ReflectionMacros.h"
#include "Framework/ReflectionProp.h"

#include "Framework/ClassBase.h"

struct ImNodesEditorContext;
struct editor_layer {
	ImNodesEditorContext* context = nullptr;
	uint32_t id = 1;
};

const uint32_t MAX_INPUTS = 32;
const uint32_t MAX_NODES_IN_GRAPH = (1 << 14);
const uint32_t INPUT_START = MAX_NODES_IN_GRAPH;
const uint32_t OUTPUT_START = INPUT_START + MAX_INPUTS * MAX_NODES_IN_GRAPH;
const uint32_t LINK_START = OUTPUT_START + MAX_INPUTS * MAX_NODES_IN_GRAPH;
const uint32_t MAX_STATIC_ATRS = 8;
const uint32_t STATIC_ATR_START = LINK_START + MAX_INPUTS * MAX_NODES_IN_GRAPH;

const Color32 ROOT_COLOR = { 94, 2, 2 };
const Color32 SM_COLOR = { 82, 2, 94 };
const Color32 STATE_COLOR = { 15, 61, 16 };
const Color32 SOURCE_COLOR = { 1, 0, 74 };
const Color32 BLEND_COLOR = { 26, 75, 79 };
const Color32 ADD_COLOR = { 44, 57, 71 };
const Color32 MISC_COLOR = { 13, 82, 44 };
const Color32 VALUE_COLOR = { 93, 97, 15 };
const Color32 IK_COLOR = MISC_COLOR;
const Color32 CACHE_COLOR = { 5,5,5 };
const Color32 DIRPLAY_COLOR = { 112, 112, 112 };

extern ImVec4 scriptparamtype_to_color(anim_graph_value type);

struct GraphPinType
{
	GraphPinType() = default;

	enum type_ {
		value_t,
		localspace_pose,
		meshspace_pose,
		state_t,
	}type=localspace_pose;
	anim_graph_value value_type = anim_graph_value::bool_t;
	GraphPinType(anim_graph_value agv) : type(value_t), value_type(agv) {}
	GraphPinType(type_ agv) : type(agv) {}

	bool operator==(const GraphPinType& other) const { return other.type == type && value_type == other.value_type; }
};

inline GraphPinType hint_str_to_GraphPinType(const char* str)
{
	if (strcmp(str, "float") == 0) return GraphPinType(anim_graph_value::float_t);
	if (strcmp(str, "int") == 0) return GraphPinType(anim_graph_value::int_t);
	if (strcmp(str, "bool") == 0) return GraphPinType(anim_graph_value::bool_t);
	if (strcmp(str, "local") == 0) return GraphPinType(GraphPinType::localspace_pose);
	if (strcmp(str, "mesh") == 0) return GraphPinType(GraphPinType::meshspace_pose);
	if (strcmp(str, "vec3") == 0) return GraphPinType(anim_graph_value::vec3_t);
	if (strcmp(str, "quat") == 0) return GraphPinType(anim_graph_value::quat_t);
	ASSERT(0);
}


struct PropertyInfo;
struct Base_EdNode;
struct GraphNodeInput
{
	std::string name;
	GraphPinType type;
	Base_EdNode* node = nullptr;
	const PropertyInfo* prop_link = nullptr;

	bool is_node_required = true;	// set to false to not emit errors when node is missing

	bool is_attached_to_node() const { return node; }
};

class AnimationGraphEditor;
CLASS_H(Base_EdNode, ClassBase)
	Base_EdNode() {
		for (int i = 0; i < inputs.size(); i++) 
			inputs[i].node = nullptr;
	}
	virtual ~Base_EdNode() {}

	static const PropertyInfoList* get_props() {
		START_PROPS(Base_EdNode)
			REG_INT(id, PROP_SERIALIZE, ""),
			REG_INT(graph_layer, PROP_SERIALIZE, ""),
		END_PROPS(Base_EdNode)
	}

	void post_construct(uint32_t id, uint32_t graph_layer);

	// called after either creation or load from file
	virtual void init() = 0;
	virtual std::string get_name() const = 0;
	virtual bool compile_my_data(const AgSerializeContext* ctx) = 0;

	// used to specify menu to draw the creation for this in
	virtual std::string get_menu_category() const { return ""; }
	virtual bool allow_creation_from_menu() const { return true; }

	// get title to use for node in graph
	virtual std::string get_title() const { return get_name(); }
	virtual std::string get_tooltip() const { return ""; }
	virtual void draw_node_topbar() { }

	virtual bool has_pin_colors() const { return false; }
	virtual ImVec4 get_pin_colors() const { return ImVec4(1, 1, 1, 1); }
	virtual BaseAGNode* get_graph_node() { return nullptr; }
	// used for blendspace nodes to add ref'd clip nodes
	virtual void get_any_extra_refed_graph_nodes(std::vector<BaseAGNode*>& refed_nodes) {}

	// this call adds elements that are being edited like Node_CFG, State
	virtual void add_props_for_extra_editable_element(std::vector<PropertyListInstancePair>& props) {
	}

	virtual void get_link_props(std::vector<PropertyListInstancePair>& props, int slot) {}

	virtual bool dont_call_compile() const { return false; }
	virtual bool traverse_and_find_errors();
	virtual void remove_reference(Base_EdNode* node);
	
	// for statemachine and state nodes
	virtual const editor_layer* get_layer() const { return nullptr; }
	virtual std::string get_layer_tab_title() const { return ""; }

	virtual void on_remove_pin(int slot, bool force = false) {
		inputs[slot].node = nullptr;
	}
	virtual void on_post_remove_pins() {}
	virtual bool add_input(AnimationGraphEditor* ed, Base_EdNode* input, uint32_t slot) {
		inputs[slot].node = input;
		return false;
	}


	// animation graph specific stuff
	virtual bool draw_flat_links() const { return false; }
	virtual bool push_imnode_link_colors(int index) { return false; }
	virtual bool is_statemachine() const { return false; }
	virtual bool is_state_node() const { return false; }

	virtual void on_post_edit() {}
	virtual void draw_node_top_bar() {}
	virtual void post_prop_edit_update() {}
	virtual void draw_node_bottom_bar() {}

	virtual Color32 get_node_color() const { return { 23, 82, 12 }; }
	virtual bool has_output_pin() const { return true; }
	virtual bool can_delete() const { return true; }

	virtual std::string get_input_pin_name(int index) const { return inputs[index].name; }
	virtual std::string get_output_pin_name() const { return  "out"; }

	virtual GraphPinType get_output_type_general() const = 0;
	virtual bool can_output_to_type(GraphPinType input_pin) const {
		return get_output_type_general() == input_pin;
	}

	uint32_t getinput_id(uint32_t inputslot) const {
		return inputslot + id * MAX_INPUTS + INPUT_START;
	}
	uint32_t getoutput_id(uint32_t outputslot) const {
		return outputslot + id * MAX_INPUTS + OUTPUT_START;
	}
	uint32_t getlink_id(uint32_t link_idx) const {
		return link_idx + id * MAX_INPUTS + LINK_START;
	}

	static uint32_t get_slot_from_id(uint32_t id) {
		return id % MAX_INPUTS;
	}

	static uint32_t get_nodeid_from_output_id(uint32_t outputid) {
		return (outputid - OUTPUT_START) / MAX_INPUTS;
	}
	static uint32_t get_nodeid_from_input_id(uint32_t inputid) {
		return (inputid - INPUT_START) / MAX_INPUTS;
	}
	static uint32_t get_nodeid_from_link_id(uint32_t linkid) {
		return (linkid - LINK_START) / MAX_INPUTS;
	}
	static uint32_t get_nodeid_from_static_atr_id(uint32_t staticid) {
		return (staticid - STATIC_ATR_START) / MAX_STATIC_ATRS;
	}

	void append_fail_msg(const char* msg) {
		compile_error_string += msg;
	}
	void append_info_msg(const char* msg) {
		compile_info_string += msg;
	}

	uint32_t id = 0;
	uint32_t graph_layer = 0;

	bool has_errors() const {
		return !compile_error_string.empty();
	}

	void clear_info_and_fail_msgs() {
		compile_error_string.clear();
		compile_info_string.clear();
	}

	bool children_have_errors = false;
	std::string compile_error_string;
	std::string compile_info_string;

	bool is_this_node_created() const {
		return is_newly_created;
	}
	void clear_newly_created() {
		is_newly_created = false;
	}

	uint32_t get_input_size() const { return inputs.size(); }

	std::vector<GraphNodeInput> inputs;

	void init_graph_node_input(const char* name, GraphPinType type, const PropertyInfo* pi) {
		GraphNodeInput i;
		i.name = name;
		i.type = type;
		i.prop_link = pi;
		inputs.push_back(i);
	}
	void init_graph_nodes_from_node() {
		auto node = get_graph_node();
		if (!node||!node->get_type().props)
			return;
		auto props = node->get_type().props;
		for (int i = 0; i < props->count; i++) {
			auto& prop = props->list[i];
			// value/pose node type
			if (strcmp(prop.custom_type_str, "AgSerializeNodeCfg") == 0) {
				auto type = hint_str_to_GraphPinType(prop.range_hint);
				init_graph_node_input(prop.name, type, &props->list[i]);
			}
		}
	}
private:
	bool is_newly_created = false;
};