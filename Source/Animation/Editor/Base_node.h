#pragma once
#ifdef EDITOR_BUILD
#include <cstdint>
#include <array>
#include <vector>
#include <string>
#include "imgui.h"

#include "../Runtime/AnimationTreeLocal.h"
#include "Framework/Factory.h"
#include "Framework/ReflectionMacros.h"
#include "Framework/ReflectionProp.h"

#include "Framework/ClassBase.h"
#include "Framework/Reflection2.h"
#include "Framework/StructReflection.h"
#include "Optional.h"
#include "Framework/MapUtil.h"

struct ImNodesEditorContext;
struct editor_layer {
	ImNodesEditorContext* context = nullptr;
	int id = 1;
};

const int MAX_INPUTS = 32;
const int MAX_NODES_IN_GRAPH = (1 << 14);
const int INPUT_START = MAX_NODES_IN_GRAPH;
const int OUTPUT_START = INPUT_START + MAX_INPUTS * MAX_NODES_IN_GRAPH;
const int LINK_START = OUTPUT_START + MAX_INPUTS * MAX_NODES_IN_GRAPH;
const int MAX_STATIC_ATRS = 8;
const int STATIC_ATR_START = LINK_START + MAX_INPUTS * MAX_NODES_IN_GRAPH;

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
	return GraphPinType(anim_graph_value::float_t);
}

struct GraphNodeHandle {
	STRUCT_BODY();
	GraphNodeHandle(int id) : id(id) {}
	GraphNodeHandle() = default;
	REF int id = 0;
	bool is_valid() const {
		return id != 0;
	}

	bool operator==(const GraphNodeHandle& other) const { return id == other.id; }
};
struct GraphPortHandle {
	STRUCT_BODY();
	GraphPortHandle(int id) : id(id) {}
	GraphPortHandle() = default;
	REF int id = 0;
	GraphNodeHandle get_node()const;
	int get_index() const;
	bool is_output() const;
	bool operator==(const GraphPortHandle& other) const { return id == other.id; }

	static GraphPortHandle make(GraphNodeHandle node, int index, bool is_output);

	string to_string() {
		return "port(" +std::to_string(get_node().id) + ":" + std::to_string(get_index()) + ")";
	}
};
struct GraphLayerHandle {
	STRUCT_BODY();
	GraphLayerHandle(int id) : id(id) {}
	GraphLayerHandle() = default;
	REF int id = 0;
	bool operator==(const GraphLayerHandle& other) const { return id == other.id; }
	bool operator!=(const GraphLayerHandle& other) const { return id != other.id; }
	bool is_valid() const {
		return id != 0;
	}

};

struct GraphPort
{
	int index = 0;
	bool output_port = false;
	string name = "";
	GraphPinType type;

	GraphPortHandle get_handle(GraphNodeHandle node) const;
	bool is_output() const {
		return output_port;
	}
	bool is_input() const {
		return !is_output();
	}
	int get_idx() const {
		return index;
	}

};

struct GraphLink {
	STRUCT_BODY();
	GraphLink() = default;
	GraphLink(GraphPortHandle input, GraphPortHandle output) {
		assert(!input.is_output());
		assert(output.is_output());
		this->input = input;
		this->output = output;
	}

	REF GraphPortHandle input;
	REF GraphPortHandle output;

	string to_string() {
		return "link(" + input.to_string() + "," + output.to_string() + ")";
	}

	bool operator==(const GraphLink& other) {
		return input == other.input && output == other.output;
	}

	int get_link_id() const {
		return input.id;// reuse input, its unique
	}
	int get_input_node_id() const {
		return input.get_node().id;
	}
	int get_output_node_id() const {
		return output.get_node().id;
	}

	GraphNodeHandle get_other_node(GraphNodeHandle self) {
		auto n1 = input.get_node();
		auto n2 = output.get_node();
		assert(n1 == self || n2 == self);
		if (n1 == self)
			return n2;
		return n1;
	}
};

struct GraphLinkWithNode
{
	STRUCT_BODY();

	GraphLinkWithNode() = default;
	GraphLinkWithNode(GraphLink l) : link(l) {}
	REF GraphLink link;
	REF GraphNodeHandle opt_link_node;
};

struct CompilationError
{
	enum ErrorType {
		ERROR,
		WARN,
		INFO,
	}type = ERROR;
	GraphNodeHandle node;
	string message;
};

struct PropertyInfo;
class Base_EdNode;
struct GraphNodeInput
{
	string name;
	GraphPinType type;
	int output_id = 0;
	int input_id = 0;

	Base_EdNode* node = nullptr;

	const PropertyInfo* prop_link = nullptr;

	bool is_node_required = true;	// set to false to not emit errors when node is missing

	bool is_attached_to_node() const { return node; }
};

class AnimationGraphEditorNew;

class EditorNodeGraph;
class Base_EdNode : public ClassBase
{
public:
	CLASS_BODY(Base_EdNode);
	AnimationGraphEditorNew* editor = nullptr;

	virtual ~Base_EdNode() {}

	virtual void draw_imnode();

	// called after either creation or load from file
	const std::string& get_name() const {
		return name;
	}
	//virtual bool compile_my_data(const AgSerializeContext* ctx) = 0;

	// used to specify menu to draw the creation for this in
	//virtual std::string get_menu_category() const { return ""; }
	//virtual bool allow_creation_from_menu() const { return true; }

	// get title to use for node in graph
	virtual std::string get_title() const { return get_name(); }
	virtual std::string get_tooltip() const { return ""; }
	virtual void draw_node_topbar() { }

	virtual bool has_pin_colors() const { return false; }
	virtual ImVec4 get_pin_colors() const { return ImVec4(1, 1, 1, 1); }
	//virtual BaseAGNode* get_graph_node() { return nullptr; }

	virtual bool dont_call_compile() const { return false; }
	//virtual bool traverse_and_find_errors();
	//virtual void remove_reference(Base_EdNode* node);
	
	// for statemachine and state nodes
	//virtual const editor_layer* get_layer() const { return nullptr; }
	virtual std::string get_layer_tab_title() const { return ""; }

	//virtual bool add_input(AnimationGraphEditor* ed, Base_EdNode* input, uint32_t slot) {
	//	inputs[slot].node = input;
	//	return false;
	//}


	// animation graph specific stuff
	//virtual bool draw_flat_links() const { return false; }
	//virtual bool push_imnode_link_colors(int index) { return false; }
	//virtual bool is_statemachine() const { return false; }
	//virtual bool is_state_node() const { return false; }

	virtual void on_post_edit() {}
	virtual void draw_node_top_bar() {}
	virtual void post_prop_edit_update() {}
	virtual void draw_node_bottom_bar() {}

	virtual Color32 get_node_color() const { return { 23, 82, 12 }; }
	virtual bool has_output_pin() const { return true; }
	virtual bool can_delete() const { return true; }

	//virtual std::string get_input_pin_name(int index) const { return inputs[index].name; }
	//virtual std::string get_output_pin_name() const { return  "out"; }

	//virtual GraphPinType get_output_type_general() const = 0;
	//virtual bool can_output_to_type(GraphPinType input_pin) const {
	//	return get_output_type_general() == input_pin;
	//}

	GraphPortHandle getinput_id(int inputslot) const {
		return inputslot + self.id * MAX_INPUTS + INPUT_START;
	}
	GraphPortHandle getoutput_id(int outputslot) const {
		return outputslot + self.id * MAX_INPUTS + OUTPUT_START;
	}
	int getlink_id(int link_idx) const {
		return link_idx + self.id * MAX_INPUTS + LINK_START;
	}

	static int get_slot_from_id(int id) {
		return id % MAX_INPUTS;
	}

	static int get_nodeid_from_output_id(int outputid) {
		return (outputid - OUTPUT_START) / MAX_INPUTS;
	}
	static int get_nodeid_from_input_id(int inputid) {
		return (inputid - INPUT_START) / MAX_INPUTS;
	}
	static int get_nodeid_from_link_id(int linkid) {
		return (linkid - LINK_START) / MAX_INPUTS;
	}
	static int get_nodeid_from_static_atr_id(int staticid) {
		return (staticid - STATIC_ATR_START) / MAX_STATIC_ATRS;
	}
	GraphPort& add_in_port(int index, string name) {
		GraphPort p;
		p.index = index;
		p.output_port = false;
		p.name = name;
		ports.push_back(p);
		return ports.back();
	}
	GraphPort& add_out_port(int index, string name) {
		GraphPort p;
		p.output_port = true;
		p.index = index;
		p.name = name;
		ports.push_back(p);
		return ports.back();
	}

	//void append_fail_msg(const char* msg) {
	//	compile_error_string += msg;
	//}
	//void append_info_msg(const char* msg) {
	//	compile_info_string += msg;
	//}

	//bool has_errors() const {
	//	return !compile_error_string.empty();
	//}

	//void clear_info_and_fail_msgs() {
	//	compile_error_string.clear();
	//	compile_info_string.clear();
	//}

	//bool children_have_errors = false;
	//string compile_error_string;
	//string compile_info_string;

	//bool is_this_node_created() const {
	//	return is_newly_created;
	//}
	//void clear_newly_created() {
	//	is_newly_created = false;
	//}

	//int get_input_size() const { return (int)inputs.size(); }
	//std::vector<GraphNodeInput> inputs;

	opt<int> find_link_idx_from_port(GraphPortHandle port) {

		for (int i = 0; i < links.size(); i++) {
			const GraphLink& l = links.at(i).link;
			if (l.input == port || l.output == port)
				return i;
		}
		return std::nullopt;
	}
	opt<GraphLink> find_link_from_port(GraphPortHandle port) {
		opt<int> idx = find_link_idx_from_port(port);
		if (!idx.has_value())
			return std::nullopt;
		return links.at(idx.value()).link;
	}
	void get_ports(vector<int>& input, vector<int>& output) {
		for (int i = 0; i < ports.size(); i++) {
			if (ports.at(i).is_input())
				input.push_back(i);
			else
				output.push_back(i);
		}
	}
	opt<int> get_link_index(GraphLink link) {
		for (int i = 0; i < links.size();i++) {
			auto& l = links.at(i);
			if (l.link == link)
				return i;
		}
		return std::nullopt;
	}
	bool does_input_have_port_already(GraphPortHandle input) {
		assert(input.get_node() == self);
		assert(!input.is_output());
		for (GraphLinkWithNode& link : links) {
			if (link.link.input == input)
				return true;
		}
		return false;
	}
	void add_link(GraphLink link) {
		assert(link.input.get_node() == self || link.output.get_node() == self);
		opt<int> index = get_link_index(link);
		if (index.has_value()) {
			sys_print(Warning, "link already exists\n");
			return;
		}
		links.push_back(GraphLinkWithNode(link));
	}
	GraphNodeHandle remove_link_to_input(GraphPortHandle p) {
		opt<int> index = find_link_idx_from_port(p);
		if (index.has_value()) {
			return remove_link(links.at(index.value()).link);
		}
		else {
			printf("couldnt find link to input\n");
			return GraphNodeHandle();
		}
	}

	GraphNodeHandle remove_link(GraphLink link) {
		opt<int> index = get_link_index(link);
		if (index.has_value()) {
			GraphNodeHandle link_opt_node = links.at(index.value()).opt_link_node;
			links.erase(links.begin() + index.value());
			return link_opt_node;
		}
		else {
			sys_print(Warning, "cant remove link, not found\n");
			return GraphNodeHandle();
		}
	}
	void remove_node_from_other_ports();

	bool vaildate_links();

	string name;
	vector<GraphPort> ports;
	REF vector<GraphLinkWithNode> links;
	REF GraphNodeHandle self;
	REF GraphLayerHandle layer;
	REF bool hidden_node = false;

	REFLECT(hide);
	float nodex = 0.0;
	REFLECT(hide);
	float nodey = 0.0;

	//void init_graph_node_input(const char* name, GraphPinType type, const PropertyInfo* pi) {
	//	GraphNodeInput i;
	//	i.name = name;
	//	i.type = type;
	//	i.prop_link = pi;
	//	inputs.push_back(i);
	//}
	//void init_graph_nodes_from_node() {
	//	auto node = get_graph_node();
	//	if (!node||!node->get_type().props)
	//		return;
	//	auto props = node->get_type().props;
	//	for (int i = 0; i < props->count; i++) {
	//		auto& prop = props->list[i];
	//		// value/pose node type
	//		if (strcmp(prop.custom_type_str, "AgSerializeNodeCfg") == 0) {
	//			auto type = hint_str_to_GraphPinType(prop.range_hint);
	//			init_graph_node_input(prop.name, type, &props->list[i]);
	//		}
	//	}
	//}
	//
private:
	//bool is_newly_created = false;
};
#endif