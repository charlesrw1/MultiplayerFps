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

#include "Base_node.h"

class SerializeImNodeState : public IPropertySerializer
{
	// Inherited via IPropertySerializer
	virtual std::string serialize(DictWriter& out, const PropertyInfo& info, void* inst, TypedVoidPtr userptr) override;
	virtual void unserialize(DictParser& in, const PropertyInfo& info, void* inst, StringView token, TypedVoidPtr userptr) override;
};
class SerializeNodeCFGRef : public IPropertySerializer
{
	// Inherited via IPropertySerializer
	virtual std::string serialize(DictWriter& out, const PropertyInfo& info, void* inst, TypedVoidPtr userptr) override;
	virtual void unserialize(DictParser& in, const PropertyInfo& info, void* inst, StringView token, TypedVoidPtr userptr) override;
};

struct GraphTab {
	const editor_layer* layer = nullptr;
	Base_EdNode* owner_node = nullptr;
	bool open = true;
	glm::vec2 pan = glm::vec2(0.f, 0.f);
	std::string tabname;
	bool reset_pan_to_middle_next_draw = false;

	bool is_statemachine_tab() {
		return owner_node && owner_node->is_statemachine();
	}

	void update_tab_name() {
		if (!owner_node) tabname = "ROOT";
		else owner_node->get_layer_tab_title();
	}
};

class AnimationGraphEditor;
class TabState
{
public:
	AnimationGraphEditor* parent;
	TabState(AnimationGraphEditor* parent) : parent(parent) {}


	void add_tab(const editor_layer* layer, Base_EdNode* node, glm::vec2 pan, bool mark_for_selection) {
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

	void remove_nodes_tab(Base_EdNode* node) {
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

	GraphTab* find_tab(Base_EdNode* owner_node) {
		for (int i = 0; i < tabs.size(); i++) {
			if (tabs[i].owner_node == owner_node)
				return &tabs[i];
		}
		return nullptr;
	}
	int find_tab_index(Base_EdNode* owner_node) {
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


class GraphOutput
{
public:
	bool is_valid_for_preview() { return model && set; }

	Animator anim;
	handle<Render_Object> obj;
	User_Camera camera;
	View_Setup vs;

	Model* model = nullptr;
	const Animation_Set_New* set = nullptr;
};

struct EditorControlParamProp {
	EditorControlParamProp() {
		current_id = unique_id_generator++;
	}
	std::string name = "Unnamed";
	control_param_type type = control_param_type::int_t;
	int16_t enum_type = -1;

	int current_id = 0;

	AG_ControlParam get_control_param() const {
		AG_ControlParam p;
		p.name = name;
		p.type = type;
		p.enum_idx = enum_type;
		p.reset_after_tick = false;
		return p;
	}
	EditorControlParamProp(const AG_ControlParam& param) {
		name = param.name;
		type = param.type;
		enum_type = param.enum_idx;
		current_id = unique_id_generator++;
	}

	static PropertyInfoList* get_props();
	static PropertyInfoList* get_ed_control_null_prop();
	static void reset_id_generator(int to) {
		unique_id_generator = to;
	}
private:
	static int unique_id_generator;
};

class ControlParamArrayHeader;
class ControlParamsWindow
{
public:
	void imgui_draw();
	void refresh_props();

	const std::vector<EditorControlParamProp>& get_control_params() { return props; }

	void add_parameters_to_tree(ControlParam_CFG* cfg) {
		cfg->clear_variables();
		for (int i = 0; i < props.size(); i++) {
			ControlParamHandle h = cfg->push_variable(props[i].get_control_param());
			ASSERT(h.id == i);
		}
	}
	void recalculate_control_prop_ids() {
		// all parameters are now referencing the 'index' of the property as its id,
		// so set it for future ones
		for (int i = 0; i < props.size(); i++)
			props[i].current_id = i;
		EditorControlParamProp::reset_id_generator( props.size() );	// reset to size() so new props get added correctly
	}
	
	ControlParamHandle get_index_of_prop_for_compiling(int id) {
		for (int i = 0; i < props.size(); i++)
			if (props[i].current_id == id)
				return { i };
		return { -1 };
	}

	const EditorControlParamProp* get_parameter_for_ed_id(int id) {
		for (int i = 0; i < props.size(); i++)
			if (props[i].current_id == id) 
				return &props[i];

		return nullptr;
	}

	void init_from_tree(ControlParam_CFG* cfg) {
		clear_all();
		for (int i = 0; i < cfg->types.size(); i++) {
			props.push_back(cfg->types[i]);
		}
		refresh_props();
	}

	void clear_all() {
		props.clear();
		EditorControlParamProp::reset_id_generator(0);
		control_params.clear_all();
	}

private:
	static PropertyInfoList* get_props();
	static PropertyInfoList* get_edit_value_props();

	friend class ControlParamArrayHeader;

	std::vector<EditorControlParamProp> props;
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
	AnimationGraphEditor() : graph_tabs(this) {
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
	};

	graph_playback_state get_playback_state() { return playback; }
	bool graph_is_read_only() { return playback != graph_playback_state::stopped(); }
	void pause_playback();
	void start_or_resume_playback();
	void stop_playback();

	void save_command();
	bool save_document();
	void save_editor_nodes(DictWriter& writer);
	void create_new_document();
	void compile_and_run();
	bool compile_graph_for_playing();
	bool load_editor_nodes(DictParser& parser);
	bool current_document_has_path() { return !name.empty(); }

	Base_EdNode* editor_node_for_cfg_node(Node_CFG* node) {
		if (node == nullptr)
			return nullptr;
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
	Base_EdNode* find_node_from_id(uint32_t id) {
		return nodes.at(find_for_id(id));
	}

	Base_EdNode* find_first_node_in_layer(uint32_t layer, const char* name) {
		for (int i = 0; i < nodes.size(); i++) {
			if (nodes[i]->graph_layer == layer && strcmp(nodes[i]->get_typeinfo().name, name) == 0 ){
				return nodes[i];
			}
		}
		return nullptr;
	}
	Base_EdNode* get_owning_node_for_layer(uint32_t layer) {
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

	Animation_Tree_CFG* get_tree() {
		return editing_tree;
	}

	Animation_Tree_RT* get_runtime_tree() {
		return &out.anim.runtime_dat;
	}

	editor_layer create_new_layer(bool is_statemachine) {
		editor_layer layer;
		layer.id = current_layer++;
		layer.context = ImNodes::EditorContextCreate();

		return layer;
	}

	Base_EdNode* user_create_new_graphnode(const char* typename_, uint32_t layer);

	void add_root_node_to_layer(Base_EdNode* parent, uint32_t layer, bool is_statemachine) {
		user_create_new_graphnode((is_statemachine) ? "StateStart_EdNode" : "Root_EdNode", layer);
	}

	void draw_node_creation_menu(bool is_state_mode);
	void draw_menu_bar();
	void draw_popups();
	void draw_prop_editor();
	void handle_imnode_creations(bool* open_popup_menu_from_drop);
	void draw_graph_layer(uint32_t layer);

	void signal_nessecary_prop_ed_reset() {
		reset_prop_editor_next_tick = true;
	}

	bool has_document_open() const {
		return editing_tree != nullptr;
	}

	void try_load_preview_models();

	static PropertyInfoList* get_props() {
		START_PROPS(AnimationGraphEditor)
			REG_INT(current_id, PROP_SERIALIZE, ""),
			REG_INT(current_layer, PROP_SERIALIZE, ""),
			REG_STRUCT_CUSTOM_TYPE(default_editor, PROP_SERIALIZE, "SerializeImNodeState"),
			// settings
			REG_BOOL(opt.open_graph, PROP_SERIALIZE, ""),
			REG_BOOL(opt.open_control_params, PROP_SERIALIZE, ""),
			REG_BOOL(opt.open_prop_editor, PROP_SERIALIZE, ""),
			REG_BOOL(opt.statemachine_passthrough, PROP_SERIALIZE, ""),
			REG_STDSTRING(opt.preview_model,PROP_EDITABLE),
			REG_STDSTRING(opt.preview_set, PROP_EDITABLE)
		END_PROPS(AnimationGraphEditor)
	}

	bool is_modifier_pressed = false;
	bool is_focused = false;
	bool is_initialized = false;
	void* imgui_node_context = nullptr;
	std::vector<const Base_EdNode*> template_creation_nodes;

	graph_playback_state playback = graph_playback_state::stopped;

	struct settings {
		bool open_graph = true;
		bool open_control_params = true;
		bool open_viewport = true;
		bool open_prop_editor = true;
		bool statemachine_passthrough = false;
		std::string preview_model = "player_FINAL.glb";
		std::string preview_set = "default.txt";
	}opt;

	bool open_open_popup = false;
	bool open_save_popup = false;
	bool reset_prop_editor_next_tick = false;

	struct selection_state
	{
		uint32_t node_last_frame = -1;
		uint32_t link_last_frame = -1;
	}sel;

	struct create_from_drop_state {
		Base_EdNode* from = nullptr;
		bool from_is_input = false;
		uint32_t slot = 0;
	}drop_state;

	std::string name = "";
	ImNodesEditorContext* default_editor = nullptr;
	std::vector<Base_EdNode*> nodes;
	// if true, then deletes editing_tree on graph close
	// otherwise, the pointer is a reference to a loaded graph stored in anim_graph_man
	bool is_owning_editing_tree = false;
	Animation_Tree_CFG* editing_tree = nullptr;
	GraphOutput out;
	uint32_t current_id = 0;
	uint32_t current_layer = 1;	// layer 0 is root
};

extern AnimationGraphEditor ed;