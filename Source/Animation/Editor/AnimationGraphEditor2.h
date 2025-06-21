#pragma once
#include "Optional.h"
#include "Framework/FnFactory.h"
#include "Framework/Hashmap.h"
#include "Framework/MapUtil.h"
#include "Framework/MulticastDelegate.h"
#include "Framework/StringUtils.h"
#include "Framework/Hashmap.h"
#include "Framework/ConsoleCmdGroup.h"
#include "LevelEditor/CommandMgr.h"
#include "EditorTool3d.h"
#include "Base_node.h"
#include "imnodes.h"
#include "Framework/Log.h"
#include "GraphUtil.h"
#include "AnimCommands.h"

class Base_EdNode;
class AnimationGraphEditorNew;
using std::string;
using std::unordered_set;
using std::unordered_map;
using std::function;

class EditorNodeGraph;
class SerializeGraphContainer;
class NodeGraphLayer : public ClassBase
{
public:
	CLASS_BODY(NodeGraphLayer);
	NodeGraphLayer() {
		context = ImNodes::EditorContextCreate();
	}
	~NodeGraphLayer() {
		ImNodes::EditorContextFree(context);
	}

	virtual void draw(EditorNodeGraph& graph);
	struct TabNameAndBackground {
		string name;
		Color32 color;
	};

	TabNameAndBackground get_tab_name(EditorNodeGraph& graph);
	void serialize(Serializer& s) override {}
	Base_EdNode* get_owning_node();
	ImNodesEditorContext* get_context() { return context; }
	
	void set_id(GraphLayerHandle handle) {
		self = handle;
	}
	GraphLayerHandle get_id() const {
		return self;
	}
	void add_node_to_layer(Base_EdNode& node) {
		SetUtil::insert_test_exists(layer_nodes, node.self.id);
	}
	void remove_node(Base_EdNode& node) {
		assert(layer_nodes.find(node.self.id)!=layer_nodes.end());
		layer_nodes.erase(node.self.id);
	}
	void set_owner_node(GraphNodeHandle p) {
		owner = p;
	}
	GraphNodeHandle get_owner_node() const {
		return owner;
	}
	const unordered_set<int>& get_nodes() const {
		return layer_nodes;
	}
	enum Type {
		BlendTree,
		Statemachine,
		Transition
	};
	void set_layer_type(Type t) {
		type = t;
	}
	Type get_layer_type() const {
		return type;
	}
	bool contains(GraphNodeHandle h) const {
		return layer_nodes.find(h.id) != layer_nodes.end();
	}
protected:
	Type type = BlendTree;
	GraphNodeHandle selected;

	void handle_drag_drop();
	unordered_set<int> layer_nodes;
	REF GraphNodeHandle owner;
	REF GraphLayerHandle self;
	ImNodesEditorContext* context = nullptr;
	bool wants_reset_view = true;
};


class EditorNodeGraph : public ClassBase
{
public:
	Base_EdNode* get_node(GraphNodeHandle handle) const {
		return nodes.find(handle.id);
	}
	void remove_node(GraphNodeHandle handle);

	NodeGraphLayer* get_layer(GraphLayerHandle handle) const {
		return layers.find(handle.id);
	}
	NodeGraphLayer* get_root() {
		return root_layer;
	}
	NodeGraphLayer* create_layer() {
		NodeGraphLayer* l = new NodeGraphLayer;
		GraphLayerHandle handle(get_next_id());
		layers.insert(handle.id, l);
		l->set_id(handle);
		assert(get_layer(handle) == l);
		return l;
	}
	void remove_layer(NodeGraphLayer* layer);

	void set_root(NodeGraphLayer* l) {
		assert(!root_layer);
		this->root_layer = l;
	}
	void insert_new_node(Base_EdNode& node, GraphLayerHandle layer, opt<glm::vec2> pos);
	void insert_nodes(SerializeGraphContainer& container);
	void insert_nodes_with_new_id(SerializeGraphContainer& container);

	AnimationGraphEditorNew* editor = nullptr;

	const hash_map<Base_EdNode*>& get_nodes() {
		return nodes;
	}

	void validate_nodes();
private:

	int get_next_id() {
		return ++id_start;
	}
	NodeGraphLayer* root_layer = nullptr;
	hash_map<Base_EdNode*> nodes;
	hash_map<NodeGraphLayer*> layers;
	int id_start = 0;
};

class AnimNodeGraphSettings : public ClassBase
{
public:
	CLASS_BODY(AnimNodeGraphSettings);
	REF AssetPtr<Model> outputModel;
	REF ClassTypePtr<AnimatorInstance> anim_class_type;
};

enum class GraphPlayState {
	Stopped,
	Running,
	Paused,
};

class PlaybackManager
{
public:
	PlaybackManager(AnimationGraphEditorNew& editor) : editor(editor) {}
	~PlaybackManager() {}

	bool is_playing() const { return get_state() != GraphPlayState::Stopped; }
	GraphPlayState get_state() const { return state; }
	void draw() {}
private:
	AnimationGraphEditorNew& editor;
	Entity* output_entity = nullptr;
	GraphPlayState state = GraphPlayState::Stopped;
};

class GraphTabManager
{
public:
	GraphTabManager(AnimationGraphEditorNew& editor) : editor(editor) {}
	void draw();
	void open_tab(GraphLayerHandle handle, bool set_active);
	void go_down_layer();
	void go_up_layer();
	void close_tab(GraphLayerHandle handle);
	opt<GraphLayerHandle> get_active_tab() const {
		if (!active_tab.has_value())
			return std::nullopt;
		int i = active_tab.value();
		assert(i >= 0 && i < tabs.size());
		return tabs.at(i);
	}
private:
	void handle_link_changes();
	opt<int> find_tab(GraphLayerHandle handle) {
		for (int i = 0; i < tabs.size(); i++) {
			if (tabs[i] == handle) return i;
		}
		return std::nullopt;
	}
	void draw_popup_menu();

	struct FromDropState {
		GraphPortHandle link_node_to_this;
	};

	opt<int> active_tab;
	bool active_tab_dirty = false;
	vector<GraphLayerHandle> tabs;
	AnimationGraphEditorNew& editor;
	glm::vec2 mouse_click_pos{};
	opt<FromDropState> from_drop;
	string node_menu_filter_buf;
	bool set_keyboard_focus = false;

	vector<GraphLayerHandle> history;
};

class GraphPropertyWindow
{
public:
	GraphPropertyWindow(AnimationGraphEditorNew& editor);
	void draw();
private:
	void update_property_window();
	PropertyGrid grid;
	AnimationGraphEditorNew& ed;
};
#include "Base_node.h"
class ControlParamsWindowNew
{
public:
	ControlParamsWindowNew(AnimationGraphEditorNew& ed);
	void imgui_draw();
	opt<GraphPinType> find_value_type(const string& name) const {
		for (auto& p : props)
			if (p.nativepi->name == name)
				return p.type;
		return std::nullopt;
	}
private:
	void refresh_props();
	void on_close() {
		props.clear();
	}
	struct VariableParam {
		GraphPinType type{};
		const PropertyInfo* nativepi = nullptr;
	};
	std::vector<VariableParam> props;
	AnimationGraphEditorNew& ed;
};

class NodeSearcher
{
public:
	
private:
};

class NodePrototypes
{
public:
	struct Creation {
		Creation(function<Base_EdNode* ()> func) {
			this->creation = std::move(func);
			template_node.reset(this->creation());
		}
		function<Base_EdNode*()> creation;
		uptr<Base_EdNode> template_node;
	};
	Base_EdNode* create(const string& name) const {
		auto func = MapUtil::get_opt(creations, name);
		if (!func) 
			return nullptr;
		Base_EdNode* node =  (func->creation)();
		if (node)
			node->name = name;
		return node;
	}
	void add(string name, function<Base_EdNode* ()> creation) {
		assert(!MapUtil::contains(creations, name));
		creations.insert({ name,Creation(creation) });
	}
	unordered_map<string, Creation> creations;
};

class ImNodesInterface
{
public:
	void set_node_position(GraphNodeHandle self, glm::vec2 pos);
	void clear_selection() {
		ImNodes::ClearNodeSelection();
		ImNodes::ClearLinkSelection();
	}
};

struct NodeMenuItem;
class NodeMenu
{
public:
	~NodeMenu();
	vector<NodeMenuItem> menus;
	NodeMenu& add(string s, opt<Color32> color = std::nullopt);
	NodeMenu& add_submenu(string name, NodeMenu& m);
	opt<int> find_item(string name) const;
};
struct NodeMenuItem {
	string name;
	opt<NodeMenu> menu;
	opt<Color32> color;
};
inline NodeMenu& NodeMenu::add(string s, opt<Color32> color) {
	menus.push_back({ s, std::nullopt, color });
	return *this;
}
inline NodeMenu& NodeMenu::add_submenu(string name, NodeMenu& m) {
	menus.push_back({ name,std::move(m) });
	return *this;
}
inline opt<int> NodeMenu::find_item(string name) const {
	for (int i = 0; i < menus.size(); i++)
		if (menus[i].name == name)
			return i;
	return std::nullopt;
}
inline NodeMenu::~NodeMenu() {}

class AnimationGraphEditorNew : public EditorTool3d
{
public:
	EditorNodeGraph& get_graph() {
		assert(graph);
		return *graph.get();
	}
	const AnimNodeGraphSettings& get_options() const {
		assert(settings);
		return *settings.get();
	}
	AnimNodeGraphSettings* get_options_ptr() {
		assert(settings);
		return settings.get();
	}
	MulticastDelegate<> on_changed_graph_classes;
	MulticastDelegate<> on_node_changes;
	MulticastDelegate<> on_selection_change;


	void add_command(Command* command);
	bool is_node_selected(Base_EdNode& node) const {
		return SetUtil::contains(multiple_selected_last_frame, node.self.id);
	}
	const NodePrototypes& get_prototypes() {
		return prototypes;
	}
	const FnFactory<IPropertyEditor>& get_factory() {
		return grid_factory;
	}
	ImNodesInterface& get_imnodes() {
		return *imnodes.get();
	}
	Base_EdNode* get_selected_node();
	NodeMenu& get_menu() { return animGraphMenu; }
	const ControlParamsWindowNew& get_params() { return *params_window.get(); }
	NodePrototypes& get_var_prototypes() {
		return variablePrototypes;
	}
private:
	void init_node_factory();
	void delete_selected();
	void dup_selected();
	void resolve_any_types();
	void draw_layer_window();

	void post_map_load_callback() override{}
	void init() override;
	void close_internal() override;
	void tick(float dt) override;
	void hook_menu_bar() override;
	bool can_save_document() override;
	void imgui_draw() override;
	bool save_document_internal() override;
	const ClassTypeInfo& get_asset_type_info() const override;
	const char* get_save_file_extension() const override;

	Animation_Tree_CFG* tree = nullptr;
	uptr<ImNodesInterface> imnodes;
	uptr<EditorNodeGraph> graph;
	uptr<AnimNodeGraphSettings> settings;
	uptr<GraphPropertyWindow> property_window;
	uptr<GraphTabManager> tab_manager;
	uptr<PlaybackManager> playback;
	uptr<ControlParamsWindowNew> params_window;
	uptr<NodeSearcher> node_searcher;
	FnFactory<IPropertyEditor> grid_factory;
	NodePrototypes prototypes;
	NodePrototypes variablePrototypes;

	//uptr<CreateFromDropState> drop_state;
	UndoRedoSystem cmd_manager;
	uptr<ConsoleCmdGroup> concmds;
	void* imnodes_context = nullptr;

	GraphNodeHandle selected_last_frame;
	unordered_set<int> multiple_selected_last_frame;


	NodeMenu animGraphMenu;
	NodeMenu stateGraphMenu;

	GraphCommandUtil::Clipboard clipboard;
};

using std::unordered_map;

class SerializeGraphContainer : public ClassBase
{
public:
	CLASS_BODY(SerializeGraphContainer);
	void serialize(Serializer& s) final;
	unordered_set<Base_EdNode*> nodes;
	unordered_set<NodeGraphLayer*> layers;
};

class SerializeGraphUtils
{
public:
	static uptr<SerializeGraphContainer> unserialize(const string& text, const NodePrototypes& p);
	static string serialize_to_string(SerializeGraphContainer& container, EditorNodeGraph& graph, const NodePrototypes& p);
	static SerializeGraphContainer make_container_from_handles(vector<GraphNodeHandle> handles, EditorNodeGraph& graph);
	static SerializeGraphContainer make_container_from_nodeids(const vector<int>& nodes,const vector<int>& links, EditorNodeGraph& graph);

};
