#pragma once
#include <vector>
#include <array>
#include <string>
#include <memory>
#include <SDL2/SDL.h>
#include <functional>
#include <unordered_set>
#include "ImSequencer.h"
#include "imnodes.h"

#include "Types.h"
#include "DrawPublic.h"
#include "Util.h"
#include "AnimationGraphEditorPublic.h"
#include "RenderObj.h"
#include "Animation.h"
#include "AnimationTreeLocal.h"
#include "PropertyEd.h"
#include "ReflectionProp.h"

class IAgEditorNode;

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

struct InputPinNode
{
	std::string pin_name;
	IAgEditorNode* other_node = nullptr;
};

class AnimationGraphEditor;
class IAgEditorNode
{
public:
	IAgEditorNode() {	
	}
	virtual ~IAgEditorNode() {
	}
	static PropertyInfoList* get_prop_list();

	// called after either creation or load from file
	virtual void init();

	virtual std::string get_default_name();
	virtual Node_CFG* get_graph_node() { return nullptr; }
	virtual void get_props(std::vector<PropertyListInstancePair>& props) {
		props.push_back({ get_prop_list(), this });
	}
	virtual void get_link_props(std::vector<PropertyListInstancePair>& props, int slot) {}
	virtual bool dont_call_compile() { return false; }
	virtual bool traverse_and_find_errors();
	virtual void on_remove_pin(int slot, bool force = false) {
		inputs[slot].other_node = nullptr;
	}
	virtual void remove_reference(IAgEditorNode* node);
	virtual editor_layer* get_layer() { return nullptr;  }
	virtual bool add_input(AnimationGraphEditor* ed, IAgEditorNode* input, uint32_t slot) {
		inputs[slot].other_node = input;
		if (grow_pin_count_on_new_pin()) {
			if (num_inputs > 0 && inputs[num_inputs - 1].other_node)
				num_inputs++;
		}
		return false;
	}
	// animation graph specific stuff
	virtual bool compile_my_data();
	virtual bool draw_flat_links() { return false; }
	virtual bool is_statemachine() {
		return false;
	}
	virtual bool is_state_node() {
		return false;
	}

	virtual void on_post_edit() {
	}
	virtual void draw_node_top_bar() {}
	virtual void post_prop_edit_update() {}
	virtual void draw_node_bottom_bar() {}

	const std::string& get_title() {
		return title;
	}
	void set_node_title(const std::string& name) {
		title = name;
	}
	bool name_is_default() { return title == "Unnamed"; }

	std::string title = "Unnamed";
	uint32_t id = 0;
	Color32 node_color = { 23, 82, 12 };
	uint32_t num_inputs = 1;
	uint32_t graph_layer = 0;
	animnode_type type = animnode_type::source;
	std::array<InputPinNode, MAX_INPUTS> inputs;
	

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

	bool can_user_delete() {
		return type != animnode_type::root;
	}
	bool has_output_pins() {
		return type != animnode_type::root;
	}

	bool grow_pin_count_on_new_pin() {
		return type == animnode_type::state;
	}

	Node_CFG* get_nodecfg_for_slot(uint32_t slot) {
		if (inputs[slot].other_node)
			return inputs[slot].other_node->get_graph_node();
		return nullptr;
	}

	void append_fail_msg(const char* msg) {
		compile_error_string += msg;
	}
	void append_info_msg(const char* msg) {
		compile_info_string += msg;
	}

	bool children_have_errors = false;
	std::string compile_error_string;
	std::string compile_info_string;
};

class AgEditor_BaseNode : public IAgEditorNode
{
public:
	AgEditor_BaseNode(Node_CFG* node) : node(node) {}

	virtual void init();
	virtual bool compile_my_data() override;
	virtual Node_CFG* get_graph_node() override { return node; }
	virtual void get_props(std::vector<PropertyListInstancePair>& props) override;
	virtual void draw_node_top_bar() override;
	virtual void on_post_edit() override {}

	Node_CFG* node = nullptr;
};

extern AutoEnumDef BlendSpace2dTopology_def;
enum class BlendSpace2dTopology
{
	FiveVert,
	NineVert,
	FifteenVert,
};

class AgEditor_BlendspaceNode : public AgEditor_BaseNode
{
public:
	using AgEditor_BaseNode::AgEditor_BaseNode;

	virtual void init();
	virtual void get_props(std::vector<PropertyListInstancePair>& props) override;
	virtual bool compile_my_data() override;
	static PropertyInfoList* get_props_list();

	struct Blendspace_Input {
		static PropertyInfoList* get_props();
		float x=0.0;
		float y=0.0;
		std::string clip_name;
		Clip_Node_CFG* clip_node = nullptr;
	};
	char parameterization = 0;	// empty value
	BlendSpace2dTopology topology_2d = BlendSpace2dTopology::FiveVert;
	std::vector<Blendspace_Input> blend_space_inputs;
};

class AgEditor_StateNode;
class AgEditor_StateMachineNode : public IAgEditorNode
{
public:
	AgEditor_StateMachineNode(Statemachine_Node_CFG* node) : node(node) {}

	virtual void init();
	virtual std::string get_default_name() override;
	virtual bool traverse_and_find_errors();
	virtual void remove_reference(IAgEditorNode* node) override;
	virtual void get_props(std::vector<PropertyListInstancePair>& props) override;
	virtual bool compile_my_data() override;
	virtual bool is_statemachine() override { return true; }
	virtual Node_CFG* get_graph_node() override { return node; }
	virtual editor_layer* get_layer() { return (sublayer.context )
		?  &sublayer : nullptr; }

	~AgEditor_StateMachineNode() {
		if (sublayer.context) {
			ImNodes::EditorContextFree(sublayer.context);
		}
	}

	handle<State> add_new_state();
	bool set_editor_node_for_handle(handle<State> handle, AgEditor_StateNode* node) {
		ASSERT(handle.is_valid());
		if (handle.id >= 0 && handle.id < states.size() && states[handle.id] == nullptr) {
			states[handle.id] = node;
			return true;
		}
		printf("!!! set_editor_node_for_handle failed !!! (something was read from disk wrong or out of date)\n ");
		return false;
	}

	State* get_state(handle<State> state);

	std::vector<AgEditor_StateNode*> states;
	editor_layer sublayer;

	Statemachine_Node_CFG* node = nullptr;
};


class AgEditor_StateNode : public IAgEditorNode
{
public:
	AgEditor_StateNode(AgEditor_StateMachineNode* parent, handle<State> state_handle) 
		: parent_statemachine(parent), state_handle(state_handle) {}

	virtual void init();

	~AgEditor_StateNode() {
		if (sublayer.context) {
			ImNodes::EditorContextFree(sublayer.context);
		}
	}

	virtual bool dont_call_compile() { return true; }
	virtual bool traverse_and_find_errors();
	virtual std::string get_default_name() override;
	virtual void on_remove_pin(int slot, bool force) override;
	virtual bool is_state_node() override { return true; }
	virtual void remove_reference(IAgEditorNode* node) override;
	virtual bool compile_my_data() override;
	virtual void get_props(std::vector<PropertyListInstancePair>& props) override;
	virtual bool draw_flat_links() override { return true; }
	virtual editor_layer* get_layer() {
		return (sublayer.context)
			? &sublayer : nullptr;
	}
	virtual bool add_input(AnimationGraphEditor* ed, IAgEditorNode* input, uint32_t slot) override;
	virtual void get_link_props(std::vector<PropertyListInstancePair>& props, int slot) override;

	void on_output_create(AgEditor_StateNode* node_to_output);
	void remove_output_to(AgEditor_StateNode* node);

	void get_transition_props(AgEditor_StateNode* to, std::vector<PropertyListInstancePair>& props);
	bool compile_data_for_statemachine();

	struct output_transition_info {
		AgEditor_StateNode* output_to = nullptr;
	};

	// maps 1-to-1 with state transitions
	std::vector<output_transition_info> output;

	editor_layer sublayer;
	AgEditor_StateMachineNode* parent_statemachine = nullptr;

	handle<State> state_handle;
	int selected_transition_for_prop_ed = 0;
};

struct GraphTab {
	editor_layer* layer = nullptr;
	IAgEditorNode* owner_node = nullptr;
	bool open = true;
	glm::vec2 pan = glm::vec2(0.f, 0.f);
	std::string tabname;
	bool reset_pan_to_middle_next_draw = false;

	bool is_statemachine_tab() {
		return owner_node && owner_node->is_statemachine();
	}

	void update_tab_name() {
		if (!owner_node) tabname = "ROOT";
		else if (owner_node->type == animnode_type::statemachine) tabname = "statemachine: " + owner_node->get_title();
		else if (owner_node->type == animnode_type::state) tabname = "state: " + owner_node->get_title();
		else ASSERT(!"tab with bad type");
	}
};

class AnimationGraphEditor;
class TabState
{
public:
	AnimationGraphEditor* parent;
	TabState(AnimationGraphEditor* parent) : parent(parent) {}


	void add_tab(editor_layer* layer, IAgEditorNode* node, glm::vec2 pan, bool mark_for_selection) {
		GraphTab tab;
		tab.layer = layer;
		tab.owner_node = node;
		tab.pan = pan;
		tab.reset_pan_to_middle_next_draw = true;
		tab.open = true;
		tabs.push_back(tab);

		push_tab_to_view(tabs.size() - 1);
	}

	void imgui_draw();

	GraphTab* get_active_tab() {
		if (tabs.empty()) return nullptr;

		return &tabs[active_tab];
	}

	void remove_nodes_tab(IAgEditorNode* node) {
		int tab = find_tab_index(node);
		if (tab != -1) {
			tabs.erase(tabs.begin() + tab);
		}

	}

	void push_tab_to_view(int index) {
		if (index == active_tab)
			return;

		forward_tab_stack.clear();
		if(active_tab!=-1)
			active_tab_hist.push_back(active_tab);
		active_tab = index;
		active_tab_dirty = true;
	}

	GraphTab* find_tab(IAgEditorNode* owner_node) {
		for (int i = 0; i < tabs.size(); i++) {
			if (tabs[i].owner_node == owner_node)
				return &tabs[i];
		}
		return nullptr;
	}
	int find_tab_index(IAgEditorNode* owner_node) {
		for (int i = 0; i < tabs.size(); i++) {
			if (tabs[i].owner_node == owner_node)
				return i;
		}
		return -1;
	}

	uint32_t get_current_layer_from_tab() {
		// 0 = root layer

		int active = active_tab;

		return tabs[active].layer ? tabs[active].layer->id : 0;
	}

	void update_tab_names() {
		std::unordered_map<std::string, int> name_to_count;

		for (int i = 0; i < tabs.size(); i++) {
			tabs[i].update_tab_name();

			auto find = name_to_count.find(tabs[i].tabname);
			if (find == name_to_count.end()) name_to_count[tabs[i].tabname] = 1;
			else name_to_count[tabs[i].tabname]++;
		}
		for (int i = tabs.size() - 1; i >= 0; i--) {
			auto find = name_to_count.find(tabs[i].tabname);
			if (find->second > 1) {
				tabs[i].tabname += "_";
				tabs[i].tabname += std::to_string(find->second - 1);
				name_to_count[find->first]--;
			}
		}
	}

private:
	int active_tab = -1;
	bool active_tab_dirty = false;

	std::vector<int> forward_tab_stack;
	std::vector<int> active_tab_hist;
	std::vector<GraphTab> tabs;
};


enum class sequence_type
{
	clip,
	transition,
	state
};
struct MySequence : public ImSequencer::SequenceInterface
{
	// interface with sequencer

	virtual int GetFrameMin() const {
		return mFrameMin;
	}
	virtual int GetFrameMax() const {
		return mFrameMax;
	}
	virtual int GetItemCount() const { return (int)items.size(); }

	virtual void Add(int type) {  };
	virtual void Del(int index) { }
	virtual void Duplicate(int index) { }

	virtual size_t GetCustomHeight(int index) { return 0; }

	void add_manual_track(std::string str, int start, int end) {

		uint32_t mask = 0;

		for (int i = 0; i < items.size(); i++) {
			const auto& item = items[i].back();
			if (item.start < end && item.end > start) {
				mask |= (1ull << i);
			}
		}

		auto save = current_item_bitmask;
		current_item_bitmask = mask;

		int index = start_track(str, sequence_type::clip);
		items[index].back().start = start;
		items[index].back().end = end;
		end_track(index);

		current_item_bitmask = save;
	}

	int start_track(const std::string& str, sequence_type type) {
		for (int i = 0; i < 64; i++) {
			bool active = current_item_bitmask & (1ull << (uint64_t)i);
			if (active)
				continue;

			ImSequencer::Item item;
			item.start = active_frame;
			item.end = active_frame + 1;
			item.text = get_cstr(str);

			if (i >= items.size()) {
				items.push_back({});
				ASSERT(i < items.size());
			}
			items[i].push_back(item);
			current_item_bitmask |= (1ull << (uint64_t)i);

			return i;
		}
		printf("Start_track full!\n");
		ASSERT(0);
		return 0;
	}
	void end_track(int index) {
		current_item_bitmask = current_item_bitmask & ~(1ull << index);
	}

	void continue_tracks() {
		for (int i = 0; i < 64; i++) {
			if (current_item_bitmask & (1ull << i)) {
				ASSERT(items.size() >= i);
				ASSERT(!items[i].empty());
				items[i].back().end = active_frame;
			}
		}
	}

	const char* get_cstr(std::string s) {
		if (interned_strings.find(s) != interned_strings.end())
			return interned_strings.find(s)->c_str();
		interned_strings.insert(s);
		return interned_strings.find(s)->c_str();
	}

	std::unordered_set<std::string> interned_strings;
	uint64_t current_item_bitmask = 0;
	std::vector<std::vector<ImSequencer::Item>> items;

	// my datas
	MySequence() : mFrameMin(0), mFrameMax(0) {}
	int mFrameMin, mFrameMax;
	int active_frame = 0;

	virtual void DoubleClick(int index) {

	}
	// Inherited via SequenceInterface
	virtual int GetItems(int index, ImSequencer::Item* item, int start = 0) override
	{
		if (start >= items[index].size()) return -1;

		*item = items[index][start];
		return ++start;
	}
};

class Timeline
{
public:
	AnimationGraphEditor* owner;
	Timeline(AnimationGraphEditor* o) : owner(o) {}
	void draw_imgui();

	void play() {}
	void pause() {}
	void stop() {}
	void save() {}

	bool needs_compile = false;
	bool is_reset = true;
	bool is_playing = false;
	float play_speed = 1.f;

	bool expaned = true;
	int first_frame = 0;

	int current_tick = 0;

	MySequence seq;
};

class GraphOutput
{
public:
	Animator anim;
	handle<Render_Object> obj;
	User_Camera camera;
	View_Setup vs;

	Model* model = nullptr;
	const Animation_Set_New* set = nullptr;
};

class ControlParamArrayHeader;
class ControlParamsWindow
{
public:
	void imgui_draw();
	void refresh_props() {
		control_params.clear_all();
		control_params.add_property_list_to_grid(get_props(), this, PG_LIST_PASSTHROUGH);
	}
	void set_read_only(bool readonly) { control_params.set_read_only(readonly); }
	struct tuple {
		const char* str = "";
		int index = 0;
		script_parameter_type type{};
	};

	std::vector<tuple> get_param_names_for_lookup() {
		std::vector<tuple> out;
		for (int i = 0; i < props.size(); i++) {
			// c_str shouldnt become invalid
			out.push_back({ props[i].name.c_str(), props[i].imgui_id, props[i].type });
		}
		return out;
	}

	
	struct ControlParamProp {
		ControlParamProp() {
			static int unique_id = 0;
			imgui_id = unique_id++;
		}
		int indirect_index = 0;
		std::string name = "Unnamed";
		script_parameter_type type = script_parameter_type::int_t;
		int enum_type = 0;
		int imgui_id = 0;

		bool is_virtual_param = false;
		ScriptExpression script;

		static PropertyInfoList* get_props();
	};

	const ControlParamProp* get_parameter(int id) {
		for (int i = 0; i < props.size(); i++)
			if (props[i].imgui_id == id) 
				return &props[i];

		return nullptr;
	}
private:
	static PropertyInfoList* get_props();

	friend class ControlParamArrayHeader;

	std::vector<ControlParamProp> props;
	PropertyGrid control_params;
};

class BoneWeightListWindow
{
	struct BoneWeightListProp {
		BoneWeightListProp() {
			static int unique_id = 0;
			imgui_id = unique_id++;
		}
		std::string name = "Unnamed";

		struct BoneWeightProp {
			float weight = 0.0;
			int bone_index = 0;
		};

		std::vector<BoneWeightProp> weights;

		int imgui_id = 0;
		static PropertyInfoList* get_props();
	};
};

class IPopup
{
public:
	virtual bool draw() = 0;
};

class AreYouSurePopup : public IPopup
{
	virtual bool draw() override;
	std::function<void(int)> command;
};

class Texture;
class AnimationGraphEditor : public AnimationGraphEditorPublic
{
public:
	AnimationGraphEditor() : graph_tabs(this), timeline(this) {
	}
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
	virtual void begin_draw() override;

	enum class graph_playback_state {
		stopped,
		running,
		paused,
	}playback = graph_playback_state::stopped;

	graph_playback_state get_playback_state() { return playback; }
	bool graph_is_read_only() { return playback != graph_playback_state::stopped(); }
	void pause_playback();
	void start_or_resume_playback();
	void stop_playback();

	void save_command();
	void save_document();
	void remove_unused_nodes() {}
	void save_editor_nodes(DictWriter& writer) {}
	void create_new_document();
	void compile_and_run();
	bool compile_graph_for_playing();



	IAgEditorNode* editor_node_for_cfg_node(Node_CFG* node) {
		for (int i = 0; i < nodes.size(); i++) {
			ASSERT(nodes[i]);
			if (nodes[i]->get_graph_node() == node)
				return nodes[i];
		}
		return nullptr;
	}

	void delete_selected();
	void nuke_layer(uint32_t layer_id);
	void remove_node_from_index(int index, bool force);
	void remove_node_from_id(uint32_t index);
	int find_for_id(uint32_t id);
	IAgEditorNode* find_node_from_id(uint32_t id) {
		return nodes.at(find_for_id(id));
	}

	IAgEditorNode* find_first_node_in_layer(uint32_t layer, animnode_type type) {
		for (int i = 0; i < nodes.size(); i++) {
			if (nodes[i]->type == type && nodes[i]->graph_layer == layer) {
				return nodes[i];
			}
		}
		return nullptr;
	}
	IAgEditorNode* get_owning_node_for_layer(uint32_t layer) {
		for (int i = 0; i < nodes.size(); i++) {

			auto sublayer = nodes[i]->get_layer();
			if (sublayer && sublayer->id == layer && sublayer->context) {
				return nodes[i];
			}
		}
		return nullptr;
	}

	ImNodesEditorContext* get_default_node_context() {
		return default_editor;
	}

	ControlParamsWindow control_params;
	PropertyGrid node_props;

	TabState graph_tabs;
	Timeline timeline;

	std::string name = "";
	void* imgui_node_context = nullptr;
	ImNodesEditorContext* default_editor = nullptr;

	std::vector<IAgEditorNode*> nodes;
	Animation_Tree_CFG* editing_tree = nullptr;

	IAgEditorNode* create_graph_node_from_type(IAgEditorNode* parent, animnode_type type, uint32_t layer);

	GraphOutput out;

	struct create_from_drop_state {
		IAgEditorNode* from = nullptr;
		bool from_is_input = false;
		uint32_t slot = 0;
	}drop_state;



	editor_layer create_new_layer(bool is_statemachine) {
		editor_layer layer;
		layer.id = current_layer++;
		layer.context = ImNodes::EditorContextCreate();

		return layer;
	}

	void add_root_node_to_layer(IAgEditorNode* parent, uint32_t layer, bool is_statemachine) {
		auto a = create_graph_node_from_type(parent, is_statemachine?animnode_type::start_statemachine : animnode_type::root, layer);
	}

	void draw_node_creation_menu(bool is_state_mode);
	void draw_menu_bar();
	void draw_popups();
	void draw_prop_editor();
	void handle_imnode_creations(bool* open_popup_menu_from_drop);
	void draw_graph_layer(uint32_t layer);

	bool is_modifier_pressed = false;
	bool is_focused = false;

	bool open_open_dialouge = false;
	bool open_timeline = true;
	bool open_viewport = true;
	bool open_prop_editor = true;

	bool statemachine_passthrough = false;

	struct icons {
		Texture* error;
		Texture* back;
		Texture* forward;
		Texture* undo;
	}icon;

	uint32_t node_last_frame = -1;
	uint32_t link_last_frame = -1;

	uint32_t current_id = 0;
	uint32_t current_layer = 1;	// layer 0 is root
};