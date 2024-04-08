#pragma once

#include "Util.h"
#include <vector>
#include <array>
#include <string>

#include "RenderObj.h"
#include "Animation.h"

#include <SDL2/SDL.h>

#include "AnimationTreeLocal.h"

#include "imnodes.h"

#include <memory>

class Editor_Graph_Node;

struct editor_layer {
	ImNodesEditorContext* context = nullptr;
	uint32_t id = 1;
};

const uint32_t MAX_INPUTS = 16;
const uint32_t MAX_NODES_IN_GRAPH = (1 << 12);
const uint32_t INPUT_START = MAX_NODES_IN_GRAPH;
const uint32_t OUTPUT_START = INPUT_START + MAX_INPUTS * MAX_NODES_IN_GRAPH;
const uint32_t LINK_START = OUTPUT_START + MAX_INPUTS * MAX_NODES_IN_GRAPH;
const uint32_t MAX_STATIC_ATRS = 8;
const uint32_t STATIC_ATR_START = LINK_START + MAX_INPUTS * MAX_NODES_IN_GRAPH;
class AnimationGraphEditor;
class Editor_Graph_Node
{
public:
	Editor_Graph_Node() {
		memset(inputs.data(), 0, sizeof(Editor_Graph_Node*) * inputs.size());
	}

	~Editor_Graph_Node() {
		if (sublayer.context) {
			ImNodes::EditorContextFree(sublayer.context);
		}
	}


	std::string title = "Empty";
	uint32_t id = 0;
	Color32 node_color = { 23, 82, 12 };

	uint32_t num_inputs = 1;
	uint32_t graph_layer = 0;

	std::array<std::string, MAX_INPUTS> input_pin_names;
	std::array<Editor_Graph_Node*, MAX_INPUTS> inputs;

	uint32_t num_params = 1;
	struct static_atr {
		const char* param_name = "";
		float float_param = 0.0;
		int int_param = 0;
		int min_int = 0;
		int max_int = 1;
		float min_float = 0.0;
		float max_float = 1.0;
		bool is_int_param = false;
	};
	std::array<static_atr, MAX_STATIC_ATRS> attributes;

	void add_input(Editor_Graph_Node* input, uint32_t slot) {
		inputs[slot] = input;
	}

	void remove_reference(Editor_Graph_Node* node);

	uint32_t getinput_id(uint32_t inputslot) {
		return inputslot + id * MAX_INPUTS + INPUT_START;
	}
	uint32_t getoutput_id(uint32_t outputslot) {
		return outputslot + id * MAX_INPUTS + OUTPUT_START;
	}
	uint32_t getlink_id(uint32_t link_idx) {
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

	void set_node_title(const std::string& name) {
		title = name;
	}

	// animation graph specific stuff
	void on_state_change(AnimationGraphEditor* ed);
	bool is_node_valid();

	bool draw_flat_links() {
		return type == animnode_type::state;
	}
	bool can_user_delete() {
		return type != animnode_type::root;
	}
	bool has_output_pins() {
		return type != animnode_type::root;
	}
	bool is_statemachine() {
		return type == animnode_type::statemachine;
	}

	animnode_type type = animnode_type::source;
	Node_CFG* node = nullptr;

	// used by statemachine and state nodes
	editor_layer sublayer;

	// state machine node stuff
	struct statemachine_data {
		std::vector<Editor_Graph_Node*> states;
	}sm;

	// state node data
	struct state_data {
		Editor_Graph_Node* parent_statemachine = nullptr;
		
		struct transition {
			std::string code = "";	// lisp expression
			Editor_Graph_Node* transition_node = nullptr;
		};

		State* state_ptr = nullptr;
	}state;
};
class AnimationGraphEditor
{
public:

	void init();
	void close();

	void tick(float dt);

	void begin_draw();

	void draw_graph_layer(uint32_t layer);

	void handle_event(const SDL_Event& event);

	void delete_selected();
	bool has_selected() { return selected.size() > 0; }
	Editor_Graph_Node* add_node();
	void remove_node_from_index(int index);
	void remove_node_from_id(uint32_t index);
	int find_for_id(uint32_t id);
	Editor_Graph_Node* find_node_from_id(uint32_t id) {
		return nodes.at(find_for_id(id));
	}
	void save_graph(const std::string& name);
	void open_graph(const std::string& name);

	Editor_Graph_Node* find_first_node_in_layer(uint32_t layer, animnode_type type) {
		for (int i = 0; i < nodes.size(); i++) {
			if (nodes[i]->type == type && nodes[i]->graph_layer == layer) {
				return nodes[i];
			}
		}
		return nullptr;
	}

	void* imgui_node_context = nullptr;

	ImNodesEditorContext* default_editor = nullptr;

	std::vector<Editor_Graph_Node*> selected;
	std::vector<Editor_Graph_Node*> nodes;

	void draw_node_creation_menu(bool is_state_mode);
	Editor_Graph_Node* create_graph_node_from_type(animnode_type type);


	std::string name = "";

	Animation_Tree_CFG* editing_tree = nullptr;

	struct create_from_drop_state {
		Editor_Graph_Node* from = nullptr;
		bool from_is_input = false;
		uint32_t slot = 0;
	}drop_state;

	struct output_data {
		Animator anim;
		handle<Render_Object> obj;
	}out;

	struct tab {
		editor_layer* layer = nullptr;
		Editor_Graph_Node* owner_node = nullptr;
		bool open = true;
		glm::vec2 pan = glm::vec2(0.f, 0.f);
		bool mark_for_selection = false;
		std::string tabname;

		bool is_statemachine_tab() {
			return owner_node && owner_node->is_statemachine();
		}

		void update_tab_name() {
			if (!owner_node) tabname =  "ROOT";
			else if (owner_node->type == animnode_type::statemachine) tabname =  "statemachine: " + owner_node->title;
			else if (owner_node->type == animnode_type::state) tabname = "state: " + owner_node->title;
			else ASSERT(!"tab with bad type");
		}
	};

	editor_layer create_new_layer(bool is_statemachine) {
		editor_layer layer;
		layer.id = current_layer++;
		layer.context = ImNodes::EditorContextCreate();

		add_root_node_to_layer(layer.id, is_statemachine);

		return layer;
	}

	uint32_t get_current_layer_from_tab() {
		return tabs[active_tab_index].layer ? tabs[active_tab_index].layer->id : 0;
	}

	tab* find_tab(Editor_Graph_Node* owner_node) {
		for (int i = 0; i < tabs.size(); i++) {
			if (tabs[i].owner_node == owner_node)
				return &tabs[i];
		}
		return nullptr;
	}
	int find_tab_index(Editor_Graph_Node* owner_node) {
		for (int i = 0; i < tabs.size(); i++) {
			if (tabs[i].owner_node == owner_node)
				return i;
		}
		return -1;
	}

	void add_root_node_to_layer(uint32_t layer, bool is_statemachine) {
		auto a = create_graph_node_from_type(is_statemachine?animnode_type::start_statemachine : animnode_type::root);
		a->graph_layer = layer;
	}


	void update_tab_names() {
		std::unordered_map<std::string, int> name_to_count;

		for (int i = 0; i < tabs.size(); i++) {
			tabs[i].update_tab_name();

			auto find = name_to_count.find(tabs[i].tabname);
			if (find == name_to_count.end()) name_to_count[tabs[i].tabname] = 1;
			else name_to_count[tabs[i].tabname]++;
		}
		for (int i = tabs.size()-1; i>=0; i--) {
			auto find = name_to_count.find(tabs[i].tabname);
			if (find->second > 1) {
				tabs[i].tabname += "_";
				tabs[i].tabname += std::to_string(find->second-1);
				name_to_count[find->first]--;
			}
		}
	}

	std::vector<tab> tabs;
	uint32_t active_tab_index = 0;

	bool is_modifier_pressed = false;
	bool is_focused = false;

	uint32_t current_id = 0;
	uint32_t current_layer = 1;	// layer 0 is root
};

extern AnimationGraphEditor* g_agraph;