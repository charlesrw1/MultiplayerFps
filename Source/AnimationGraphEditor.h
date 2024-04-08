#pragma once

#include "Util.h"
#include <vector>
#include <array>
#include <string>

#include <SDL2/SDL.h>

const uint32_t MAX_INPUTS = 16;
const uint32_t MAX_NODES_IN_GRAPH = (1 << 12);
const uint32_t INPUT_START = MAX_NODES_IN_GRAPH;
const uint32_t OUTPUT_START = INPUT_START + MAX_INPUTS * MAX_NODES_IN_GRAPH;
const uint32_t LINK_START = OUTPUT_START + MAX_INPUTS * MAX_NODES_IN_GRAPH;
const uint32_t MAX_STATIC_ATRS = 8;
const uint32_t STATIC_ATR_START = LINK_START + MAX_INPUTS * MAX_NODES_IN_GRAPH;
class Editor_Graph_Node
{
public:
	Editor_Graph_Node() {
		memset(inputs.data(), 0, sizeof(Editor_Graph_Node*) * inputs.size());
	}


	std::string title = "Empty";
	uint32_t id = 0;
	Color32 node_color = { 23, 82, 12 };
	bool persistent = false;	// dont delete from graph

	uint32_t num_inputs = 4;

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
};

enum class animnode_type
{
	source,
	statemachine,
	selector,

	mask,
	
	blend,
	blend2d,
	add,
	subtract,
	aimoffset,

	mirror,
	speed,

	state,

	root,

	COUNT
};

// animation editor node specific stuff
class Animation_Ed_Graph_Node : public Editor_Graph_Node
{
public:
	animnode_type type = animnode_type::source;
	void on_state_change();
};

class AnimationGraphEditor
{
public:

	void init();
	void close();

	void begin_draw();

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

	void* imgui_node_context = nullptr;
	std::vector<Editor_Graph_Node*> selected;
	std::vector<Editor_Graph_Node*> nodes;

	void draw_node_creation_menu(bool is_state_mode);
	Animation_Ed_Graph_Node* create_graph_node_from_type(animnode_type type);

	struct create_from_drop_state {
		Editor_Graph_Node* from = nullptr;
		bool from_is_input = false;
		uint32_t slot = 0;
	}drop_state;

	bool is_modifier_pressed = false;
	bool is_focused = false;

	uint32_t current_id = 0;
};

extern AnimationGraphEditor* g_agraph;