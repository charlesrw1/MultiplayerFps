#ifdef EDITOR_BUILD
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

#include "../Runtime/Animation.h"
#include "../Runtime/AnimationTreeLocal.h"

#include "Framework/PropertyEd.h"
#include "Framework/ReflectionProp.h"

#include "Base_node.h"

#include "Framework/MulticastDelegate.h"
#include "EditorTool3d.h"
#if 0
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
		if (active_tab != -1)
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

	int get_current_layer_from_tab() {
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
		for (int i = (int)tabs.size() - 1; i >= 0; i--) {
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

class Entity;
class MeshComponent;
class GraphOutput
{
public:
	void init();
	void show(bool is_playing);
	AnimatorInstance* get_animator();
	void set_model(Model* model);
	Model* get_model();
	void start();
	void end();
private:
	Entity* ent = nullptr;
	MeshComponent* mc = nullptr;
};

struct VariableNameAndType
{
	string str;
	anim_graph_value type = anim_graph_value::float_t;
};

class ControlParamArrayHeader;
class ControlParamsWindow
{
public:
	ControlParamsWindow();

	void imgui_draw();
	void refresh_props();
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
};




class Texture;
class AG_GuiLayout;
class AnimationGraphEditor : public EditorTool3d
{
public:

	AnimationGraphEditor();

	virtual void init() override;

	virtual void close_internal() override;
	virtual void tick(float dt) override;
	void hook_menu_bar() override;

	bool can_save_document() override;
	virtual void imgui_draw() override;


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
	void nuke_layer(int layer_id);
	void remove_node_from_index(int index, bool force);
	void remove_node_from_id(int index);
	int find_for_id(int id);
	Base_EdNode* find_node_from_id(int id) {
		return nodes.at(find_for_id(id));
	}

	template<typename T>
	T* find_first_node_in_layer(int layer) {
		for (int i = 0; i < nodes.size(); i++) {
			if (nodes[i]->graph_layer == layer && nodes[i]->get_type() == T::StaticType) {
				return nodes[i]->cast_to<T>();
			}
		}
		return nullptr;
	}
	Base_EdNode* get_owning_node_for_layer(int layer) {
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


	AG_GuiLayout* gui = nullptr;
	void on_key_down(const SDL_KeyboardEvent& key);
	void on_wheel(const SDL_MouseWheelEvent& wheel);


	std::unique_ptr<ControlParamsWindow> control_params;
	std::unique_ptr<PropertyGrid> node_props;
	std::unique_ptr<TabState> graph_tabs;
	std::unique_ptr<AnimGraphClipboard> clipboard;

	Animation_Tree_CFG* get_tree() {
		return editing_tree.get();
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

	Base_EdNode* user_create_new_graphnode(const char* typename_, int layer);

	void add_root_node_to_layer(Base_EdNode* parent, int layer, bool is_statemachine) {
		user_create_new_graphnode((is_statemachine) ? "StateStart_EdNode" : "Root_EdNode", layer);
	}

	void draw_node_creation_menu(bool is_state_mode, ImVec2 mouse_click_pos);

	void draw_popups();
	void draw_prop_editor();
	void handle_imnode_creations(bool* open_popup_menu_from_drop);
	void draw_graph_layer(int layer);

	void signal_nessecary_prop_ed_reset() {
		reset_prop_editor_next_tick = true;
	}


	void try_load_preview_models();

	static const PropertyInfoList* get_props() {
		using MyClassType = AnimationGraphEditor;
		START_PROPS(AnimationGraphEditor)
			REG_INT(current_id, PROP_SERIALIZE, ""),
			REG_INT(current_layer, PROP_SERIALIZE, ""),
			REG_STRUCT_CUSTOM_TYPE(default_editor, PROP_SERIALIZE, "SerializeImNodeState"),
			// settings
			REG_BOOL(opt.open_graph, PROP_SERIALIZE, ""),
			REG_BOOL(opt.open_control_params, PROP_SERIALIZE, ""),
			REG_BOOL(opt.open_prop_editor, PROP_SERIALIZE, ""),
			REG_BOOL(opt.statemachine_passthrough, PROP_SERIALIZE, ""),

			REG_CLASSTYPE_PTR(anim_class_type, PROP_EDITABLE),
			REG_ASSET_PTR(output_model, PROP_DEFAULT)
			END_PROPS(AnimationGraphEditor)
	}

	bool is_modifier_pressed = false;
	bool is_focused = false;
	void* imgui_node_context = nullptr;
	std::vector<const Base_EdNode*> template_creation_nodes;

	graph_playback_state playback = graph_playback_state::stopped;

	struct settings {
		bool open_graph = true;
		bool open_control_params = true;
		bool open_viewport = true;
		bool open_prop_editor = true;
		bool statemachine_passthrough = false;
	}opt;

	const ClassTypeInfo* get_animator_class() const {
		return anim_class_type.ptr;
	}

	ClassTypePtr<AnimatorInstance> anim_class_type;
	AssetPtr<Model> output_model;


	bool reset_prop_editor_next_tick = false;

	struct selection_state
	{
		int node_last_frame = -1;
		int link_last_frame = -1;
	}sel;

	struct create_from_drop_state {
		Base_EdNode* from = nullptr;
		bool from_is_input = false;
		int slot = 0;
	}drop_state;

	ImNodesEditorContext* default_editor = nullptr;
	std::vector<Base_EdNode*> nodes;

	std::unique_ptr<Animation_Tree_CFG> editing_tree;

	GraphOutput out;
	int current_id = 0;
	int current_layer = 1;	// layer 0 is root

	PropertyGrid self_grid;

	void add_node_to_tree_manual(BaseAGNode* n) {
		editing_tree->all_nodes.push_back(n);
	}

	void post_map_load_callback();
	const ClassTypeInfo& get_asset_type_info() const override {
		return Animation_Tree_CFG::StaticType;
	}
	const char* get_save_file_extension() const override {
		return "ag";
	}

	FnFactory<IPropertyEditor> grid_factory;
};

extern AnimationGraphEditor anim_graph_ed;
#endif
#endif