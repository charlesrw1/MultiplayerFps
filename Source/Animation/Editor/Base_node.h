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

#include <variant>
using std::variant;

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
class EditorNodeGraph;


struct GraphPinType {
	enum Enum {
		Any,

		Boolean,
		Integer,
		Float,
		Vec3,
		Quat,
		EnumType,

		StringName,
		ClassInfoType,

		LocalSpacePose,
		MeshSpacePose,
		StateType
	};
	Enum type = Enum::Any;
	variant<const EnumTypeInfo*, const ClassTypeInfo*, std::monostate> data;

	GraphPinType(Enum type) : type(type) {}
	GraphPinType() = default;

	bool operator!=(const GraphPinType& other) const {
		return !(*this == other);
	}
	bool operator==(const GraphPinType& other) const { 
		assert(0);
		return other.type == type;
	}
	bool is_any() const {
		return type == Enum::Any;
	}
};

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
class Base_EdNode;
struct GraphPort;
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

	struct NodeIndexOutput {
		GraphNodeHandle h;
		int index = 0;
		bool is_output = false;
	};
	NodeIndexOutput break_to_values() const {
		return { get_node(),get_index(),is_output() };
	}

	GraphPort* get_port_ptr(EditorNodeGraph& graph);
	Base_EdNode* get_node_ptr(EditorNodeGraph& graph);

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
	GraphPort() = default;

	GraphPinType type;
	string name = "";
	int index = 0;
	bool output_port = false;

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

	bool self_is_input(GraphNodeHandle self) {
		auto n1 = input.get_node();
		auto n2 = output.get_node();
		assert(n1 == self || n2 == self);
		if (n1 == self)
			return true;
		return false;
	}
	GraphPortHandle get_other_port(GraphNodeHandle self) {
		return self_is_input(self) ? output : input;
	}
	GraphPortHandle get_self_port(GraphNodeHandle self) {
		return self_is_input(self) ? input : output;
	}
	GraphNodeHandle get_other_node(GraphNodeHandle self) {
		return get_other_port(self).get_node();
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

enum class EdNodeCategory
{
	None,
	Math,
	Function,

	AnimSource,
	AnimBlend,
	AnimBoneModify,
	AnimStatemachine,
};
class AnimationGraphEditorNew;
class Base_EdNode : public ClassBase
{
public:
	CLASS_BODY(Base_EdNode);
	AnimationGraphEditorNew* editor = nullptr;
	virtual ~Base_EdNode() {}
	virtual void draw_imnode();
	virtual void initialize() {	// called after editor ptr is set
	}

	// called after either creation or load from file
	const std::string& get_name() const {
		return name;
	}
	// get title to use for node in graph
	virtual std::string get_title() const { return get_name(); }
	virtual std::string get_tooltip() const { return ""; }
	virtual void draw_node_topbar() { }

	virtual std::string get_layer_tab_title() const { return ""; }
	virtual void on_post_edit() {}
	virtual Color32 get_node_color() const { return { 23, 82, 12 }; }
	virtual GraphLayerHandle get_owning_sublayer() const { return GraphLayerHandle(); }
	virtual void set_owning_sublayer(GraphLayerHandle h) { }


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

	virtual void on_link_changes();

	GraphPort& add_in_port(int index, string name);
	GraphPort& add_out_port(int index, string name);
	opt<int> find_my_port_idx(int index, bool output);
	GraphPort* find_my_port(int index, bool output);
	opt<int> find_port_idx_from_handle(GraphPortHandle handle);
	GraphPort* find_port_from_handle(GraphPortHandle handle);
	opt<int> find_link_idx_from_port(GraphPortHandle port);
	opt<GraphLink> find_link_from_port(GraphPortHandle port);
	Base_EdNode* find_other_node_from_port(GraphPortHandle port);
	void get_ports(vector<int>& input, vector<int>& output);
	opt<int> get_link_index(GraphLink link);
	bool does_input_have_port_already(GraphPortHandle input);
	void add_link(GraphLink link);
	GraphNodeHandle remove_link_to_input(GraphPortHandle p);
	GraphNodeHandle remove_link(GraphLink link);
	GraphNodeHandle remove_link_from_idx(int index);
	void remove_node_from_other_ports();
	bool vaildate_links();
	const GraphPort* get_other_nodes_port(GraphLink whatlink);
	const GraphPort* get_other_nodes_port_from_myport(GraphPortHandle handle);


	string name;
	vector<GraphPort> ports;

	REFLECT(hide);
	vector<GraphLinkWithNode> links;
	REFLECT(hide);
	GraphNodeHandle self;
	REFLECT(hide);
	GraphLayerHandle layer;
	REFLECT(hide);
	bool hidden_node = false;
	REFLECT(hide);
	float nodex = 0.0;
	REFLECT(hide);
	float nodey = 0.0;
};
#endif