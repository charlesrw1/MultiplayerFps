#include "AnimationGraphEditor.h"
#include <cstdio>
#include "imnodes.h"
#include "glm/glm.hpp"

#include "Game_Engine.h"

AnimationGraphEditor ed;
AnimationGraphEditorPublic* g_anim_ed_graph = &ed;

void AnimationGraphEditor::init()
{
	imgui_node_context = ImNodes::CreateContext();

	ImNodes::GetIO().LinkDetachWithModifierClick.Modifier = &is_modifier_pressed;

	editing_tree = new Animation_Tree_CFG;
	editing_tree->arena.init("ATREE ARENA", 1'000'000);	// spam the memory for the editor

	add_root_node_to_layer(0, false);
	tabs.push_back(tab());
	default_editor = ImNodes::EditorContextCreate();
	ImNodes::EditorContextSet(default_editor);
}

void AnimationGraphEditor::close()
{
	idraw->remove_obj(out.obj);

	ImNodes::DestroyContext(imgui_node_context);
}


static Color32 to_color32(glm::vec4 v) {
	Color32 c;
	c.r = glm::clamp(v.r * 255.f, 0.f, 255.f);
	c.g = glm::clamp(v.g * 255.f, 0.f, 255.f);
	c.b = glm::clamp(v.b * 255.f, 0.f, 255.f);
	c.a = glm::clamp(v.a * 255.f, 0.f, 255.f);
	return c;
}
static glm::vec4 to_vec4(Color32 color) {
	return glm::vec4(color.r, color.g, color.b, color.a) / 255.f;
}

Color32 mix_with(Color32 c, Color32 mix, float fac) {
	return to_color32(glm::mix(to_vec4(c), to_vec4(mix), fac));
}

Color32 add_brightness(Color32 c, int brightness) {
	int r = c.r + brightness;
	int g = c.g + brightness;
	int b = c.b + brightness;
	if (r < 0) r = 0;
	if (r > 255) r = 255;
	if (g < 0)g = 0;
	if (g > 255)g = 255;
	if (b < 0)b = 0;
	if (b > 255)b = 255;
	c.r = r;
	c.g = g;
	c.b = b;
	return c;
}
unsigned int color32_to_int(Color32 color) {
	return *(unsigned int*)&color;
}


void AnimationGraphEditor::begin_draw()
{
	is_modifier_pressed = ImGui::GetIO().KeyAlt;


	if (ImGui::GetIO().MouseClickedCount[0] == 2) {

		if (ImNodes::NumSelectedNodes() == 1) {
			int node = 0;
			ImNodes::GetSelectedNodes(&node);

			Editor_Graph_Node* mynode = find_node_from_id(node);
			if (mynode->type == animnode_type::state || mynode->type == animnode_type::statemachine) {
				auto findtab = find_tab(mynode);
				if (findtab) {
					findtab->mark_for_selection = true;
				}
				else {

					tab t;
					t.layer = &mynode->sublayer;
					t.owner_node = mynode;
					t.open = true;
					t.pan = glm::vec2(0.f);
					tabs.push_back(t);
				}
			}
			ImNodes::ClearNodeSelection();

		}
	}


	if (ImGui::Begin("animation graph property editor"))
	{

	}
	ImGui::End();

	ImGui::Begin("animation graph editor");

	const char* names[] = { "a","b","c" };
	static bool open[3] = { true,true,true };

	int rendered = 0;

	update_tab_names();

	if (ImGui::BeginTabBar("tabs")) {

		for (int n = 0; n < tabs.size(); n++) {
			auto flags = (tabs[n].mark_for_selection) ? ImGuiTabItemFlags_SetSelected : 0;
			bool* open_bool = (tabs[n].owner_node) ? &tabs[n].open : nullptr;
			if (ImGui::BeginTabItem(tabs[n].tabname.c_str(), open_bool, flags))
			{
				uint32_t layer = (tabs[n].layer) ? tabs[n].layer->id : 0;
				auto context = (tabs[n].layer) ? tabs[n].layer->context : default_editor;
				ImNodes::EditorContextSet(context);
				if (active_tab_index != n) {
					ImNodes::ClearNodeSelection();
				}
				draw_graph_layer(layer);
				rendered++;
				active_tab_index = n;
				ImGui::EndTabItem();
				tabs[n].mark_for_selection = false;
			}
		}
		ImGui::EndTabBar();
	}
	ASSERT(rendered <=1);

	for (int i = 0; i < tabs.size(); i++) {
		if (!tabs[i].open) {
			tabs.erase(tabs.begin() + i);
			i--;
		}
	}


	int start_atr = 0;
	int end_atr = 0;
	int link_id = 0;
	bool open_popup_menu_from_drop = false;

	if (ImNodes::IsLinkCreated(&start_atr, &end_atr))
	{
		if (start_atr >= INPUT_START && start_atr < OUTPUT_START)
			std::swap(start_atr, end_atr);

		uint32_t start_node_id = Editor_Graph_Node::get_nodeid_from_output_id(start_atr);
		uint32_t end_node_id = Editor_Graph_Node::get_nodeid_from_input_id(end_atr);
		uint32_t start_idx = Editor_Graph_Node::get_slot_from_id(start_atr);
		uint32_t end_idx = Editor_Graph_Node::get_slot_from_id(end_atr);

		ASSERT(start_idx == 0);
		
		Editor_Graph_Node* node_s = find_node_from_id(start_node_id);
		Editor_Graph_Node* node_e = find_node_from_id(end_node_id);

		node_e->inputs[end_idx] = node_s;
	}
	if (ImNodes::IsLinkDropped(&start_atr)) {
		open_popup_menu_from_drop = true;

		bool is_input = start_atr >= INPUT_START && start_atr < OUTPUT_START;
		uint32_t id = 0;
		if (is_input)
			id = Editor_Graph_Node::get_nodeid_from_input_id(start_atr);
		else
			id = Editor_Graph_Node::get_nodeid_from_output_id(start_atr);
		drop_state.from = find_node_from_id(id);
		drop_state.from_is_input = is_input;
		drop_state.slot = Editor_Graph_Node::get_slot_from_id(start_atr);
	}
	if (ImNodes::IsLinkDestroyed(&link_id)) {

		uint32_t node_id = Editor_Graph_Node::get_nodeid_from_link_id(link_id);
		uint32_t slot = Editor_Graph_Node::get_slot_from_id(link_id);
		Editor_Graph_Node* node_s = find_node_from_id(node_id);
		node_s->inputs[slot] = nullptr;
	}


	is_focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows | ImGuiFocusedFlags_RootWindow);

	if (open_popup_menu_from_drop || 
		(is_focused && ImGui::GetIO().MouseClicked[1]))
		ImGui::OpenPopup("my_select_popup");

	if (ImGui::BeginPopup("my_select_popup"))
	{
		bool is_sm = tabs[active_tab_index].is_statemachine_tab();

		draw_node_creation_menu(is_sm);
		ImGui::EndPopup();
	}
	else {
		drop_state.from = nullptr;
	}

	ImGui::End();
}

void AnimationGraphEditor::draw_graph_layer(uint32_t layer)
{
	ImNodes::BeginNodeEditor();
	for (auto node : nodes) {

		if (node->graph_layer != layer) continue;


		ImNodes::PushColorStyle(ImNodesCol_TitleBar, color32_to_int(node->node_color));
		Color32 select_color = add_brightness(node->node_color, 30);
		Color32 hover_color = add_brightness(mix_with(node->node_color, { 5, 225, 250 }, 0.6), 5);
		ImNodes::PushColorStyle(ImNodesCol_TitleBarHovered, color32_to_int(hover_color));
		ImNodes::PushColorStyle(ImNodesCol_TitleBarSelected, color32_to_int(select_color));

		ImNodes::BeginNode(node->id);

		ImNodes::BeginNodeTitleBar();
		ImGui::Text("%s\n", node->title.c_str());
		ImNodes::EndNodeTitleBar();

		for (int j = 0; j < node->num_inputs; j++) {
			ImNodes::BeginInputAttribute(node->getinput_id(j), ImNodesPinShape_Triangle);
			ImGui::TextUnformatted("input");
			ImNodes::EndInputAttribute();
		}

		if (node->has_output_pins()) {
			ImNodes::BeginOutputAttribute(node->getoutput_id(0));
			ImGui::TextUnformatted("output");
			ImNodes::EndOutputAttribute();
		}

		ImNodes::EndNode();

		ImNodes::PopColorStyle();
		ImNodes::PopColorStyle();
		ImNodes::PopColorStyle();

		for (int j = 0; j < node->num_inputs; j++) {
			if (node->inputs[j]) {

				ImNodes::Link(node->getlink_id(j), node->inputs[j]->getoutput_id(0), node->getinput_id(j), node->draw_flat_links());

			}
		}
	}


	ImNodes::MiniMap();
	ImNodes::EndNodeEditor();
}

void AnimationGraphEditor::handle_event(const SDL_Event& event)
{
	switch (event.type)
	{
	case SDL_KEYDOWN:
		switch (event.key.keysym.scancode)
		{
		case SDL_SCANCODE_DELETE:
			if (is_focused) {
				delete_selected();
			}
			break;
		}
		break;
	}

	if (eng->game_focused) {
		if (event.type == SDL_MOUSEWHEEL) {
			out.camera.scroll_callback(event.wheel.y);
		}
	}
}

void AnimationGraphEditor::delete_selected()
{
	std::vector<int> ids;
	ids.resize(ImNodes::NumSelectedLinks());
	if (ids.size() > 0) {
		ImNodes::GetSelectedLinks(ids.data());
		for (int i = 0; i < ids.size(); i++) {
			uint32_t nodeid = Editor_Graph_Node::get_nodeid_from_link_id(ids[i]);
			uint32_t slot = Editor_Graph_Node::get_slot_from_id(ids[i]);
			Editor_Graph_Node* node = find_node_from_id(nodeid);
			node->inputs[slot] = nullptr;
		}
	}

	ids.resize(ImNodes::NumSelectedNodes());
	if (ids.size() > 0) {
		ImNodes::GetSelectedNodes(ids.data());
		for (int i = 0; i < ids.size(); i++) {
			remove_node_from_id(ids[i]);
		}
	}

	ImNodes::ClearNodeSelection();
	ImNodes::ClearLinkSelection();
}

Editor_Graph_Node* AnimationGraphEditor::add_node()
{
	Editor_Graph_Node* node = new Editor_Graph_Node;
	node->id = current_id++;
	nodes.push_back(node);
	return node;
}

void AnimationGraphEditor::remove_node_from_id(uint32_t id)
{
	return remove_node_from_index(find_for_id(id));
}
void AnimationGraphEditor::remove_node_from_index(int index)
{
	auto node = nodes.at(index);

	if (!node->can_user_delete())
		return;

	for (int i = 0; i < nodes.size(); i++)
		if (i != index) nodes[i]->remove_reference(node);

	// node has a sublayer, remove child nodes
	if (node->sublayer.context) {
		for (int i = 0; i < nodes.size(); i++) {
			if (nodes[i]->graph_layer == node->sublayer.id && i != index)
				remove_node_from_index(i);
		}
	}

	// remove tab reference
	int tab = find_tab_index(node);
	if (tab != -1) {
		tabs.erase(tabs.begin() + tab);
	}

	delete node;
	nodes.erase(nodes.begin() + index);
}

int AnimationGraphEditor::find_for_id(uint32_t id)
{
	for (int i = 0; i < nodes.size(); i++)
		if (nodes[i]->id == id)return i;
	ASSERT(0);
	return -1;
}

void AnimationGraphEditor::save_graph(const std::string& name)
{

}



static const char* get_animnode_name(animnode_type type)
{
	switch (type)
	{
	case animnode_type::root: return "OUTPUT";
	case animnode_type::statemachine: return "State machine";
	case animnode_type::state: return "State";
	case animnode_type::source: return "Source";
	case animnode_type::blend: return "Blend";
	case animnode_type::add: return "Add";
	case animnode_type::subtract: return "Subtract";
	case animnode_type::aimoffset: return "Aim Offset";
	case animnode_type::blend2d: return "Blend 2D";
	case animnode_type::selector: return "Selector";
	case animnode_type::mask: return "Mask";
	case animnode_type::mirror: return "Mirror";
	case animnode_type::play_speed: return "Speed";
	case animnode_type::rootmotion_speed: return "Rootmotion Speed";
	case animnode_type::sync: return "Sync";
	case animnode_type::start_statemachine: return "START";
	default: ASSERT(!"no name defined for state");
	}
}

static Color32 get_animnode_color(animnode_type type)
{
	switch (type)
	{
	case animnode_type::root:
	case animnode_type::statemachine:
	case animnode_type::state:
		return { 28, 109, 153 };
	case animnode_type::source:
	case animnode_type::blend:
	case animnode_type::add:
		return { 115, 85, 44 };
	case animnode_type::subtract:
	case animnode_type::aimoffset:
	case animnode_type::blend2d:
		return { 107, 18, 18 };
	case animnode_type::selector:
	case animnode_type::mask:
	case animnode_type::mirror:
	case animnode_type::play_speed:
	case animnode_type::rootmotion_speed:
	case animnode_type::sync:
	default:
		return { 13, 82, 44 };
	}
}

static bool animnode_allow_creation_from_menu(bool in_state_mode, animnode_type type) {
	if (in_state_mode) return type == animnode_type::state;
	else {
		return type != animnode_type::root && type != animnode_type::state;
	}
}

void AnimationGraphEditor::draw_node_creation_menu(bool is_state_mode)
{
	int count = (int)animnode_type::COUNT;
	for (int i = 0; i < count; i++) {
		animnode_type type = animnode_type(i);
		if (animnode_allow_creation_from_menu(is_state_mode,type))
		{
			const char* name = get_animnode_name(type);
			if (ImGui::Selectable(name)) {
				auto a = create_graph_node_from_type(type);
				a->graph_layer = get_current_layer_from_tab();

				ImNodes::SetNodeScreenSpacePos(a->id, ImGui::GetMousePos());

				if (drop_state.from) {
					if (drop_state.from_is_input) {
						drop_state.from->add_input(a, drop_state.slot);
					}
					else
						a->add_input(drop_state.from, 0);
				}
			}
		}
	}

}

template<typename T>
static T* create_node_type(Animation_Tree_CFG& cfg)
{
	auto c = cfg.arena.alloc_bottom(sizeof(T));
	c = new(c)T(&cfg);
	return (T*)c;
}

Editor_Graph_Node* AnimationGraphEditor::create_graph_node_from_type(animnode_type type)
{
	auto node = add_node();

	node->title = get_animnode_name(type);
	node->node_color = get_animnode_color(type);

	node->type = type;
	switch (type)
	{
	case animnode_type::source:
		node->node = create_node_type<Clip_Node_CFG>(*editing_tree);

		node->num_inputs = 0;
		break;
	case animnode_type::statemachine:
		node->node = create_node_type<Statemachine_Node_CFG>(*editing_tree);
		node->sublayer = create_new_layer(true);

		node->num_inputs = 0;
		break;
	case animnode_type::selector:
		//node->node = create_node_type<Statemachine_Node_CFG>(*editing_tree);
		break;
	case animnode_type::mask:
		//node->node = create_node_type<CFG>(*editing_tree);

		break;
	case animnode_type::blend:
		node->node = create_node_type<Blend_Node_CFG>(*editing_tree);

		node->num_inputs = 2;
		break;
	case animnode_type::blend2d:
		node->node = create_node_type<Blend2d_CFG>(*editing_tree);

		node->num_inputs = 9;
		break;
	case animnode_type::add:
		node->node = create_node_type<Add_Node_CFG>(*editing_tree);

		node->num_inputs = 2;
		break;
	case animnode_type::subtract:
		node->node = create_node_type<Subtract_Node_CFG>(*editing_tree);

		node->num_inputs = 2;
		break;
	case animnode_type::aimoffset:
		node->num_inputs = 9;
		break;
	case animnode_type::mirror:
		node->node = create_node_type<Mirror_Node_CFG>(*editing_tree);
		node->num_inputs = 1;
		break;
	case animnode_type::play_speed:
		break;
	case animnode_type::rootmotion_speed:
		node->node = create_node_type<Scale_By_Rootmotion_CFG>(*editing_tree);
		node->num_inputs = 1;
		break;
	case animnode_type::sync:
		node->node = create_node_type<Sync_Node_CFG>(*editing_tree);

		node->num_inputs = 1;
		break;
	case animnode_type::state:
		node->sublayer = create_new_layer(false);

		node->num_inputs = 1;

		node->state.state_ptr = (State*)editing_tree->arena.alloc_bottom(sizeof(State));
		node->state.state_ptr = new(node->state.state_ptr)State;

		node->state.state_ptr->tree = nullptr;
		break;
	case animnode_type::start_statemachine:
		node->num_inputs = 0;
		break;
	case animnode_type::root:
		node->num_inputs = 1;
		break;
	default:
		ASSERT(0);
		break;
	}
	return node;
}

void Editor_Graph_Node::remove_reference(Editor_Graph_Node* node)
{
	for (int i = 0; i < num_inputs; i++) {
		if (inputs[i] == node) inputs[i] = nullptr;
	}
	
	for (int i = 0; i < sm.states.size(); i++) {
		if (sm.states[i] == node) {
			sm.states.erase(sm.states.begin() + i);
			i--;
		}
	}
	if (state.parent_statemachine == node)
		state.parent_statemachine = nullptr;

	// FIXME remove references inside internal nodes
}

void Editor_Graph_Node::on_state_change(AnimationGraphEditor* ed)
{
	switch (type)	
	{
	case animnode_type::source: {
		auto source = (Clip_Node_CFG*)node;
		int i = strlen(source->clip_name);
		if (i != 0) set_node_title(source->clip_name);
		else set_node_title("Source");
		num_inputs = 0;
	}break;
	case animnode_type::statemachine: {
		auto source = (Statemachine_Node_CFG*)node;

		auto rootnode = ed->find_first_node_in_layer(sublayer.id, animnode_type::root);
		if (rootnode) {
			auto startnode = rootnode->inputs[0];
			if (startnode && startnode->type == animnode_type::state) {
				source->start_state = startnode->state.state_ptr;
				ASSERT(source->start_state);
			}
		}

		num_inputs = 0;
	}break;
	case animnode_type::selector:
		break;
	case animnode_type::mask:
		break;
	case animnode_type::blend:
		break;
	case animnode_type::blend2d:
		break;
	case animnode_type::add:
		break;
	case animnode_type::subtract:
		break;
	case animnode_type::aimoffset:
		break;
	case animnode_type::mirror:
		break;
	case animnode_type::play_speed:
		break;
	case animnode_type::rootmotion_speed:
		break;
	case animnode_type::sync:
		break;
	case animnode_type::state:


		break;
	case animnode_type::root:
		break;
	case animnode_type::COUNT:
		break;
	default:
		break;
	}
}

void AnimationGraphEditor::tick(float dt)
{
	{
		int x = 0, y = 0;
		if (eng->game_focused)
			SDL_GetRelativeMouseState(&x, &y);
		out.camera.update_from_input(eng->keys, x, y, glm::mat4(1.f));
	}

	out.vs = View_Setup(out.camera.position, out.camera.front, glm::radians(70.f), 0.01, 100.0, eng->window_w.integer(), eng->window_h.integer());
}

void AnimationGraphEditor::overlay_draw()
{

}

void AnimationGraphEditor::open(const char* name)
{
	this->name = name;

	out.model = mods.find_or_load("player_FINAL.glb");
	out.set = out.model->animations.get();
	out.anim.set_model(out.model);
	out.obj = idraw->register_obj();

	Render_Object ro;
	ro.mesh = &out.model->mesh;
	ro.mats = &out.model->mats;
	//ro.transform = out.model->skeleton_root_transform;

	idraw->update_obj(out.obj, ro);
}
