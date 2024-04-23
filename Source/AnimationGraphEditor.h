#pragma once
#include "Types.h"
#include "DrawPublic.h"
#include "Util.h"
#include <vector>
#include <array>
#include <string>

#include "AnimationGraphEditorPublic.h"

#include "RenderObj.h"
#include "Animation.h"

#include <SDL2/SDL.h>

#include "AnimationTreeLocal.h"

#include "imnodes.h"

#include <memory>
#include <functional>

#include "PropertyEd.h"

class Editor_Graph_Node;

struct editor_layer {
	ImNodesEditorContext* context = nullptr;
	uint32_t id = 1;

};

struct More_Node_Property
{
	const char* name = "";
	Property_Type type{};

	std::string str_type;
	int i_type = 0;
	float f_type = 0.f;
	// string completion/combo box selection
	find_completion_strs_callback fcsc = nullptr;
	void* fcsc_user_data = nullptr;
	bool treat_completion_as_combo = false;
	std::function<void(More_Node_Property&)> on_edit_callback;

	uint32_t loc1 = 0;
	uint32_t loc2 = 0;
};

struct Table_Row
{
	Table_Row() {}

	Table_Row& push_prop(More_Node_Property prop) {
		props.push_back(prop);
		return *this;
	}

	std::vector<More_Node_Property> props;
	uint32_t row_id = 0;
};

struct Table_Col_Def
{
	std::string name;
	bool fixed_width = false;
	float start_width = 0.0;
};

class Table
{
public:
	Table(std::string name, std::vector<Table_Col_Def> col_names, Table_Row te) : table_name(name), col_names(col_names){
		template_row = te;
	}

	void imgui_draw();

	Table& push_row(Table_Row row) {
		ASSERT(row.props.size() == col_names.size() && "mismatched column sizes");
		row.row_id = row_id_start++;
		rows.push_back(row);
		return *this;
	}
	Table_Row& find_row(uint32_t id) {
		for (int i = 0; i < rows.size(); i++) {
			if (rows[i].row_id == id) return rows[i];
		}
		ASSERT(0);
	}

	uint32_t selected_row = -1;
	bool allow_selecting = true;
	std::string table_name;
	int num_cols = 0;
	std::vector<Table_Col_Def> col_names;
	std::vector<Table_Row> rows;
	Table_Row template_row;
	uint32_t row_id_start = 0;
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

	enum class Hardcoded_Props {
		name = 0,
	};

	std::string& get_title() {
		ASSERT(properties.size() > (int)Hardcoded_Props::name);
		return properties.at((int)Hardcoded_Props::name).str_type;
	}

	uint32_t id = 0;
	Color32 node_color = { 23, 82, 12 };

	uint32_t num_inputs = 1;
	uint32_t graph_layer = 0;

	std::array<std::string, MAX_INPUTS> input_pin_names;
	std::array<Editor_Graph_Node*, MAX_INPUTS> inputs;
	
	// since its logically more sense for transitions to be thought of as ouputs and not inputs
	bool treat_inputs_as_outputs() {
		return type == animnode_type::state;
	}

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

	bool add_input(AnimationGraphEditor* ed, Editor_Graph_Node* input, uint32_t slot) {

		if (type == animnode_type::state) {
			for (int i = 0; i < inputs.size(); i++) {
				if (inputs[i] == input) {
					return true;
				}
			}
		}

		inputs[slot] = input;

		if (grow_pin_count_on_new_pin()) {
			if (num_inputs > 0 && inputs[num_inputs - 1])
				num_inputs++;
		}

		return false;
		//on_state_change(ed);
	}

	void remove_reference(AnimationGraphEditor* ed, Editor_Graph_Node* node);

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
		get_title() = name;
	}

	void draw_property_editor(AnimationGraphEditor* ed);
	void draw_link_property_editor(AnimationGraphEditor* ed, uint32_t slot_thats_selected);

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
	bool is_state_node() {
		return type == animnode_type::state;
	}

	bool grow_pin_count_on_new_pin() {
		return type == animnode_type::state || type == animnode_type::selector;
	}

	Node_CFG* get_nodecfg_for_slot(uint32_t slot) {
		if (inputs[slot] && inputs[slot]->is_node_valid())
			return inputs[slot]->node;
		return nullptr;
	}

	animnode_type type = animnode_type::source;
	Node_CFG* node = nullptr;

	// used by statemachine and state nodes
	editor_layer sublayer;

	// state machine node stuff
	struct statemachine_data {
		std::vector<Editor_Graph_Node*> states;
	};

	std::unique_ptr<statemachine_data> sm;

	// state node data
	struct state_data {
		Editor_Graph_Node* parent_statemachine = nullptr;

		Statemachine_Node_CFG* sm_node_parent = nullptr;
		struct transition {
			More_Node_Property code_prop;
			More_Node_Property time_prop;

			std::string& get_code() {
				return code_prop.str_type;
			}
		};
		std::array<transition, MAX_INPUTS> transitions;
		handle<State> state_handle;
		int selected_transition_for_prop_ed = 0;

		State* get_state() {
			ASSERT(sm_node_parent);
			ASSERT(state_handle.id != -1);
			return &sm_node_parent->states.at(state_handle.id);
		}
	};

	std::unique_ptr<state_data> state;

	std::vector<More_Node_Property> properties;
};

struct Editor_Parameter_list
{
	struct ed_param {
		bool fake_entry = false;
		std::string s;
		script_parameter_type type = script_parameter_type::animfloat;
		uint32_t id = 0;
	};
	std::vector<ed_param> param;


	void update_real_param_list(ScriptVars_CFG* cfg);
};

class AnimationGraphEditor : public AnimationGraphEditorPublic
{
public:
	virtual void init() override;
	virtual void open(const char* name) override;
	virtual void close() override;
	virtual void tick(float dt) override;
	virtual void handle_event(const SDL_Event& event) override;
	virtual void overlay_draw() override;
	virtual const View_Setup& get_vs() override{
		return out.vs;
	}
	virtual const char* get_name() override {
		return name.c_str();
	}

	void begin_draw();

	void draw_graph_layer(uint32_t layer);

	void delete_selected();

	Editor_Graph_Node* add_node();
	void remove_node_from_index(int index);
	void remove_node_from_id(uint32_t index);
	int find_for_id(uint32_t id);
	Editor_Graph_Node* find_node_from_id(uint32_t id) {
		return nodes.at(find_for_id(id));
	}
	void save_graph(const std::string& name);

	Editor_Graph_Node* find_first_node_in_layer(uint32_t layer, animnode_type type) {
		for (int i = 0; i < nodes.size(); i++) {
			if (nodes[i]->type == type && nodes[i]->graph_layer == layer) {
				return nodes[i];
			}
		}
		return nullptr;
	}
	Editor_Graph_Node* get_owning_node_for_layer(uint32_t layer) {
		for (int i = 0; i < nodes.size(); i++) {
			if (nodes[i]->sublayer.id == layer && nodes[i]->sublayer.context) return nodes[i];
		}
		return nullptr;
	}

	std::string name = "";
	void* imgui_node_context = nullptr;
	ImNodesEditorContext* default_editor = nullptr;

	std::vector<Editor_Graph_Node*> nodes;
	Animation_Tree_CFG* editing_tree = nullptr;


	std::unique_ptr<Table> ed_params;

	void draw_node_creation_menu(bool is_state_mode);
	Editor_Graph_Node* create_graph_node_from_type(animnode_type type);



	struct create_from_drop_state {
		Editor_Graph_Node* from = nullptr;
		bool from_is_input = false;
		uint32_t slot = 0;
	}drop_state;

	struct output_data {
		Animator anim;
		handle<Render_Object> obj;
		User_Camera camera;
		View_Setup vs;

		Model* model = nullptr;
		Animation_Set* set = nullptr;

		Animation_Tree_RT tree_rt;
	}out;

	struct tab {
		editor_layer* layer = nullptr;
		Editor_Graph_Node* owner_node = nullptr;
		bool open = true;
		glm::vec2 pan = glm::vec2(0.f, 0.f);
		bool mark_for_selection = false;
		std::string tabname;
		bool reset_pan_to_middle_next_draw = false;

		bool is_statemachine_tab() {
			return owner_node && owner_node->is_statemachine();
		}

		void update_tab_name() {
			if (!owner_node) tabname =  "ROOT";
			else if (owner_node->type == animnode_type::statemachine) tabname =  "statemachine: " + owner_node->get_title();
			else if (owner_node->type == animnode_type::state) tabname = "state: " + owner_node->get_title();
			else ASSERT(!"tab with bad type");
		}
	};

	void compile_graph_for_playing();

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