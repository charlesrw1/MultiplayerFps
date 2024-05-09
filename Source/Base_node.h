#pragma once

#include <cstdint>
#include <array>
#include "ReflectionProp.h"
#include "AnimationTreeLocal.h"
#include "imgui.h"
#include "Factory.h"
#include "TypeInfo.h"
#include "ReflectionRegisterDefines.h"

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

extern ImVec4 scriptparamtype_to_color(control_param_type type);

#define EDNODE_HEADER(type_name)  virtual const TypeInfo& get_typeinfo() const override; \
	void add_props(std::vector<PropertyListInstancePair>& props) override { \
Base_EdNode::add_props(props); \
props.push_back({ type_name::get_props(), this }); \
}

class AnimationGraphEditor;
class Base_EdNode
{
public:

	Base_EdNode() {
		for (int i = 0; i < inputs.size(); i++) 
			inputs[i] = nullptr;
	}
	virtual ~Base_EdNode() {}

	static PropertyInfoList* get_props() {
		START_PROPS(Base_EdNode)
			REG_INT(id, PROP_SERIALIZE, ""),
			REG_INT(graph_layer, PROP_SERIALIZE, ""),
		END_PROPS(Base_EdNode)
	}

	void post_construct(uint32_t id, uint32_t graph_layer);

	// called after either creation or load from file
	virtual void init() = 0;
	virtual std::string get_name() const = 0;
	virtual bool compile_my_data() = 0;
	virtual const TypeInfo& get_typeinfo() const = 0;

	// used to specify menu to draw the creation for this in
	virtual std::string get_menu_category() const { return ""; }
	virtual bool allow_creation_from_menu() const { return true; }

	// get title to use for node in graph
	virtual std::string get_title() const { return get_name(); }
	virtual std::string get_tooltip() const { return ""; }
	virtual void draw_node_topbar() { }

	virtual bool has_pin_colors() const { return false; }
	virtual ImVec4 get_pin_colors() const { return ImVec4(1, 1, 1, 1); }
	virtual Node_CFG* get_graph_node() { return nullptr; }
	virtual void add_props(std::vector<PropertyListInstancePair>& props) {
		props.push_back({ get_props(), this });
	}
	virtual void get_link_props(std::vector<PropertyListInstancePair>& props, int slot) {}
	virtual bool dont_call_compile() const { return false; }
	virtual bool traverse_and_find_errors();
	virtual void on_remove_pin(int slot, bool force = false) {
		inputs[slot] = nullptr;
	}
	virtual void on_post_remove_pins() {}
	virtual void remove_reference(Base_EdNode* node);
	
	// for statemachine and state nodes
	virtual const editor_layer* get_layer() const { return nullptr; }
	virtual std::string get_layer_tab_title() const { return ""; }

	virtual bool add_input(AnimationGraphEditor* ed, Base_EdNode* input, uint32_t slot) {
		inputs[slot] = input;
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

	virtual int get_num_inputs() const { return 0; }
	virtual Color32 get_node_color() const { return { 23, 82, 12 }; }
	virtual bool has_output_pin() const { return true; }
	virtual bool can_delete() const { return true; }
	virtual std::string get_input_pin_name(int index) const { return "in"; }
	virtual std::string get_output_pin_name() const { return  "out"; }

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
	std::array<Base_EdNode*, MAX_INPUTS> inputs;


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

private:
	bool is_newly_created = false;

};


extern Factory<std::string, Base_EdNode>& get_tool_node_factory();