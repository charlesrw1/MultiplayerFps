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
#include "Render/DrawPublic.h"
#include "Framework/Util.h"
#include "Render/RenderObj.h"

#include "AnimationGraphEditorPublic.h"

#include "../Runtime/Animation.h"
#include "../Runtime/AnimationTreeLocal.h"

#include "Framework/PropertyEd.h"
#include "Framework/ReflectionProp.h"

#include "Base_node.h"

#include "Framework/MulticastDelegate.h"


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
		if (!owner_node) 
			tabname = "ROOT";
		else 
			tabname = owner_node->get_layer_tab_title();
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
	bool is_valid_for_preview() { return model && anim && model->get_skel(); }
	
	void hide();
	void show(bool is_playing);
	
	View_Setup vs;
	User_Camera camera;

	AnimatorInstance* get_animator();
	Model* get_model() { return model; }

	void set_model(Model* model) {
		this->model = model;
		idraw->get_scene()->remove_obj(obj);
	}

	void set_animator_instance(AnimatorInstance* inst);
	void clear();

	void initialize_animator(Animation_Tree_CFG* tree) {
		if (anim && model)
			anim->initialize_animator(model, tree, nullptr);
	}
private:
	handle<Render_Object> obj;
	Model* model = nullptr;
	std::unique_ptr<AnimatorInstance> anim = nullptr;

};

struct VariableNameAndType
{
	std::string str;
	anim_graph_value type = anim_graph_value::float_t;
};

class ControlParamArrayHeader;
class ControlParamsWindow
{
public:
	ControlParamsWindow();

	void imgui_draw();
	void refresh_props();

	unique_ptr<Script> add_parameters_to_tree();
	
	AnimGraphVariable get_index_of_prop_for_compiling(std::string str) {
		for (int i = 0; i < props.size(); i++)
			if (props[i].str == str)
				return { i };
		return { -1 };
	}
	anim_graph_value get_type(AnimGraphVariable var) {
		assert(var.is_valid());
		return props[var.id].type;
	}

private:
	void on_set_animator_instance(AnimatorInstance* a) {
		refresh_props();
	}
	void on_close() {
		props.clear();
	}

	struct VariableParam
	{
		std::string str;
		anim_graph_value type = anim_graph_value::float_t;
		const PropertyInfo* nativepi = nullptr;
	};

	std::vector<VariableParam> props;
	
	VariableNameAndType dragdrop;
	//PropertyGrid control_params;
};

class ListAnimationDataInModel
{
public:
	ListAnimationDataInModel();
	void imgui_draw();

private:
	void on_close();

	void set_model(const Model* model);
	const Model* model = nullptr;
	char name_filter[256];
	std::vector<std::string> vec;
	std::string drag_drop_name;
	std::string selected_name;
};

class AnimGraphClipboard
{
public:
	AnimGraphClipboard();
	void remove_references(Base_EdNode* node);
private:
	void on_close();
	void on_key_down(const SDL_KeyboardEvent& k);
	void paste_selected();
	std::vector<Base_EdNode*> clipboard;
};

class Texture;
class AG_GuiLayout;
class AnimationGraphEditor : public IEditorTool
{
public:

	AnimationGraphEditor();

	virtual void init() override;
	virtual void open_document_internal(const char* name) override;
	virtual void close_internal() override;
	virtual void tick(float dt) override;

	virtual void overlay_draw() override;
	virtual const View_Setup& get_vs() override{
		return out.vs;
	}
	bool can_save_document() override;
	virtual void imgui_draw() override;
	virtual void on_change_focus(editor_focus_state newstate) override;

	std::string get_save_root_dir()  override {
		return "./Data/Graphs/";
	}
	enum class graph_playback_state {
		stopped,
		running,
		paused,
	};

	graph_playback_state get_playback_state() { return playback; }
	bool graph_is_read_only() { return playback != graph_playback_state::stopped; }
	void pause_playback();
	void start_or_resume_playback();
	void stop_playback();

	void save_command();
	bool save_document_internal() override;
	void save_editor_nodes(DictWriter& writer);
	void create_new_document();
	void compile_and_run();
	bool compile_graph_for_playing();
	bool load_editor_nodes(DictParser& parser);

	Base_EdNode* editor_node_for_cfg_node(BaseAGNode* node) {
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

	template<typename T>
	T* find_first_node_in_layer(uint32_t layer) {
		for (int i = 0; i < nodes.size(); i++) {
			if (nodes[i]->graph_layer == layer && nodes[i]->get_type() == T::StaticType){
				return nodes[i]->cast_to<T>();
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

	MulticastDelegate<> on_open_new_doc;
	MulticastDelegate<> on_close;
	MulticastDelegate<const Model*> on_set_model;
	MulticastDelegate<AnimatorInstance*> on_set_animator_instance;


	std::unique_ptr<AG_GuiLayout> gui;
	void on_key_down(const SDL_KeyboardEvent& key);
	void on_wheel(const SDL_MouseWheelEvent& wheel);


	std::unique_ptr<ListAnimationDataInModel> animation_list;
	std::unique_ptr<ControlParamsWindow> control_params;
	std::unique_ptr<PropertyGrid> node_props;
	std::unique_ptr<TabState> graph_tabs;
	std::unique_ptr<AnimGraphClipboard> clipboard;

	Animation_Tree_CFG* get_tree() {
		return editing_tree;
	}
	template<typename T>
	void util_create_node(T*& node)
	{
		node = new T();
		editing_tree->all_nodes.push_back(node);
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

	void draw_node_creation_menu(bool is_state_mode, ImVec2 mouse_click_pos);
	void draw_menu_bar() override;
	void draw_popups();
	void draw_prop_editor();
	void handle_imnode_creations(bool* open_popup_menu_from_drop);
	void draw_graph_layer(uint32_t layer);

	void signal_nessecary_prop_ed_reset() {
		reset_prop_editor_next_tick = true;
	}

	bool has_document_open() const override {
		return editing_tree != nullptr;
	}

	void try_load_preview_models();

	static const PropertyInfoList* get_props() {
		START_PROPS(AnimationGraphEditor)
			REG_INT(current_id, PROP_SERIALIZE, ""),
			REG_INT(current_layer, PROP_SERIALIZE, ""),
			REG_STRUCT_CUSTOM_TYPE(default_editor, PROP_SERIALIZE, "SerializeImNodeState"),
			// settings
			REG_BOOL(opt.open_graph, PROP_SERIALIZE, ""),
			REG_BOOL(opt.open_control_params, PROP_SERIALIZE, ""),
			REG_BOOL(opt.open_prop_editor, PROP_SERIALIZE, ""),
			REG_BOOL(opt.statemachine_passthrough, PROP_SERIALIZE, ""),
			REG_FLOAT(out.camera.position.x,PROP_SERIALIZE,""),
			REG_FLOAT(out.camera.position.y, PROP_SERIALIZE, ""),
			REG_FLOAT(out.camera.position.z, PROP_SERIALIZE, ""),
			REG_FLOAT(out.camera.yaw, PROP_SERIALIZE, ""),
			REG_FLOAT(out.camera.pitch, PROP_SERIALIZE, ""),
			REG_FLOAT(out.camera.move_speed, PROP_SERIALIZE, ""),

			// Editable options
			REG_STDSTRING_CUSTOM_TYPE(opt.PreviewModel, PROP_DEFAULT,"FindModelForEdAnimG"),
			REG_STDSTRING_CUSTOM_TYPE(opt.AnimatorClass, PROP_DEFAULT, "AnimatorInstanceParentEditor")
		END_PROPS(AnimationGraphEditor)
	}

	bool is_modifier_pressed = false;
	bool is_focused = false;
	void* imgui_node_context = nullptr;
	std::vector<const Base_EdNode*> template_creation_nodes;

	graph_playback_state playback = graph_playback_state::stopped;

	void set_animator_instance_from_string(std::string str);
	void set_model_from_str(std::string str);

	struct settings {
		bool open_graph = true;
		bool open_control_params = true;
		bool open_viewport = true;
		bool open_prop_editor = true;
		bool statemachine_passthrough = false;

		// defaults
		std::string PreviewModel = "player_FINAL.cmdl";
		std::string AnimatorClass = "AnimatorInstance";
	}opt;

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

	ImNodesEditorContext* default_editor = nullptr;
	std::vector<Base_EdNode*> nodes;
	// if true, then deletes editing_tree on graph close
	// otherwise, the pointer is a reference to a loaded graph stored in anim_graph_man
	bool is_owning_editing_tree = false;
	Animation_Tree_CFG* editing_tree = nullptr;
	GraphOutput out;
	uint32_t current_id = 0;
	uint32_t current_layer = 1;	// layer 0 is root

	PropertyGrid self_grid;

	const char* get_editor_name() override { return "Animation Editor"; }

	void add_node_to_tree_manual(BaseAGNode* n) {
		editing_tree->all_nodes.push_back(n);
	}
};

extern AnimationGraphEditor ed;