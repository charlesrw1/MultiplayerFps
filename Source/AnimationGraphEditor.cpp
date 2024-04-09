#include "AnimationGraphEditor.h"
#include <cstdio>
#include "imnodes.h"
#include "glm/glm.hpp"

#include "Game_Engine.h"

std::string remove_whitespace(const char* str)
{
	std::string s = str;
	while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\n'))
		s.pop_back();
	while (!s.empty() && (s[0] == ' ' || s[0] == '\t' || s[0] == '\n'))
		s.erase(s.begin() + 0);
	return s;
}


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
					t.mark_for_selection = true;
					tabs.push_back(t);
				}
			}
			ImNodes::ClearNodeSelection();

		}
	}


	if (ImGui::Begin("animation graph property editor"))
	{
		if (ImGui::TreeNode("Node property editor")) {
			if (ImNodes::NumSelectedNodes() == 1) {
				int node = 0;
				ImNodes::GetSelectedNodes(&node);

				Editor_Graph_Node* mynode = find_node_from_id(node);
				mynode->draw_property_editor(this);
			}
			ImGui::TreePop();
		}

		bool need_parameter_list_update = false;

		if (ImGui::TreeNode("Parameter list")) {
			
			if (ed_params.param.empty() || !ed_params.param.back().fake_entry) {
				ed_params.param.push_back({});
				
				static uint32_t uid = 0;
				ed_params.param.back().id = uid++;

				ed_params.param.back().fake_entry = true;

			}

			uint32_t prop_editor_flags = ImGuiTableFlags_PadOuterX | ImGuiTableFlags_Resizable | ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg;

			if (ImGui::BeginTable("Parameters", 2, prop_editor_flags))
			{
				ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 80.f, 0);
				ImGui::TableSetupColumn("Type", 0, 0.0f, 1);
				ImGui::TableHeadersRow();

				ImGui::PushStyleColor(ImGuiCol_FrameBg, 0);

				int cell = 0;

				int delete_this_index = -1;
				for (auto& param : ed_params.param)
				{

					ImGui::TableNextRow();

					ImGui::TableSetColumnIndex(0);
					ImGui::SetNextItemWidth(-FLT_MIN);
					ImGui::PushID(param.id);

					static char buffer[256];
					memcpy(buffer, param.s.c_str(), param.s.size());
					buffer[param.s.size()] = 0;
					if (ImGui::InputText("##cell", buffer, 256, ImGuiInputTextFlags_EnterReturnsTrue)) {
						if (param.fake_entry) param.fake_entry = false;
						need_parameter_list_update = true;
					}
					param.s = buffer;

					if (ImGui::IsItemFocused() && ImGui::IsKeyDown(ImGuiKey_Delete))
						delete_this_index = cell;


					ImGui::TableSetColumnIndex(1);

					ImGui::SetNextItemWidth(150.f);
					static const char* type_strs[] = { "vec2","float","int" };
					if (ImGui::Combo("##label", (int*)&param.type, type_strs, 3)) {
						if (!param.fake_entry)
							need_parameter_list_update = true;
					}

					ImGui::SameLine();
					ImGui::SetNextItemWidth(150.f);
					if (!param.fake_entry && cell < out.tree_rt.parameters.parameters.size())
						ImGui::DragFloat("##label223123", &out.tree_rt.parameters.parameters.at(cell).fval, 0.05, 0.0);
				

					ImGui::PopID();

					cell++;
				}

				if (delete_this_index != -1) {

					ed_params.param.erase(ed_params.param.begin() + delete_this_index);
					need_parameter_list_update = true;

				}


				ImGui::PopStyleColor();
				ImGui::EndTable();
			}

			ImGui::TreePop();

		}

		if (need_parameter_list_update) {
			ed_params.update_real_param_list(&editing_tree->parameters);

			out.tree_rt.init_from_cfg(editing_tree, out.model, out.set);

			update_every_node();
		}



	}
	ImGui::End();

	ImGui::Begin("animation graph editor");


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

		node_e->add_input(this, node_s, end_idx);

		update_every_node();
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

		update_every_node();
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
			if (!node->input_pin_names[j].empty())
				ImGui::TextUnformatted(node->input_pin_names[j].c_str());
			else
				ImGui::TextUnformatted("in");
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
	case SDL_MOUSEBUTTONDOWN:
		if (event.button.button == 3) {
			SDL_SetRelativeMouseMode(SDL_TRUE);
			int x, y;
			SDL_GetRelativeMouseState(&x, &y);
			eng->game_focused = true;
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
		if (i != index) nodes[i]->remove_reference(this, node);

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

	update_every_node();
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
						drop_state.from->add_input(this, a, drop_state.slot);
					}
					else
						a->add_input(this, drop_state.from, 0);
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
		node->num_inputs = 2;
		break;
	case animnode_type::blend:
		node->node = create_node_type<Blend_Node_CFG>(*editing_tree);

		node->num_inputs = 2;
		break;
	case animnode_type::blend2d:
		node->node = create_node_type<Blend2d_CFG>(*editing_tree);

		node->input_pin_names[0] = "idle";
		node->input_pin_names[1] = "s";
		node->input_pin_names[2] = "sw";
		node->input_pin_names[3] = "w";
		node->input_pin_names[4] = "nw";
		node->input_pin_names[5] = "n";
		node->input_pin_names[6] = "ne";
		node->input_pin_names[7] = "e";
		node->input_pin_names[8] = "se";

		node->num_inputs = 9;
		break;
	case animnode_type::add:
		node->node = create_node_type<Add_Node_CFG>(*editing_tree);

		node->input_pin_names[0] = "diff";
		node->input_pin_names[1] = "base";


		node->num_inputs = 2;
		break;
	case animnode_type::subtract:
		node->node = create_node_type<Subtract_Node_CFG>(*editing_tree);
		node->input_pin_names[0] = "ref";
		node->input_pin_names[1] = "source";
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

	out.tree_rt.init_from_cfg(editing_tree, out.model, out.set);

	return node;
}

void Editor_Graph_Node::remove_reference(AnimationGraphEditor* ed, Editor_Graph_Node* node)
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
}

struct completion_callback_data
{
	AnimationGraphEditor* ed;
	enum search_list {
		PARAMS,
		CLIPS
	}type;
};

int node_completion_callback(ImGuiInputTextCallbackData* data)
{
	completion_callback_data* ccd = (completion_callback_data*)data->UserData;
	auto ed = ccd->ed;

	if (data->EventFlag != ImGuiInputTextFlags_CallbackCompletion) return 0;

	const char* word_end = data->Buf + data->CursorPos;
	const char* word_start = word_end;
	while (word_start > data->Buf)
	{
		const char c = word_start[-1];
		if (c == ' ' || c == '\t' || c == ',' || c == ';')
			break;
		word_start--;
	}

	// Build a list of candidates
	ImVector<const char*> candidates;
	if (ccd->type == completion_callback_data::CLIPS) {
		for (int i = 0; i < ed->out.set->clips.size(); i++)
			if (_strnicmp(ed->out.set->clips[i].name.c_str(), word_start, (int)(word_end - word_start)) == 0)
				candidates.push_back((ed->out.set->clips[i].name.c_str()));
	}
	else if (ccd->type == completion_callback_data::PARAMS) {
		for (int i = 0; i < ed->ed_params.param.size(); i++)
			if (_strnicmp(ed->ed_params.param.at(i).s.c_str(), word_start, (int)(word_end - word_start)) == 0)
				candidates.push_back((ed->ed_params.param.at(i).s.c_str()));
	}

	if (candidates.Size == 0)
	{
		// No match
		sys_print("No match for \"%.*s\"!\n", (int)(word_end - word_start), word_start);
	}
	else if (candidates.Size == 1)
	{
		// Single match. Delete the beginning of the word and replace it entirely so we've got nice casing.
		data->DeleteChars((int)(word_start - data->Buf), (int)(word_end - word_start));
		data->InsertChars(data->CursorPos, candidates[0]);
		data->InsertChars(data->CursorPos, " ");
	}
	else
	{
		// Multiple matches. Complete as much as we can..
		// So inputing "C"+Tab will complete to "CL" then display "CLEAR" and "CLASSIFY" as matches.
		int match_len = (int)(word_end - word_start);
		for (;;)
		{
			int c = 0;
			bool all_candidates_matches = true;
			for (int i = 0; i < candidates.Size && all_candidates_matches; i++)
				if (i == 0)
					c = toupper(candidates[i][match_len]);
				else if (c == 0 || c != toupper(candidates[i][match_len]))
					all_candidates_matches = false;
			if (!all_candidates_matches)
				break;
			match_len++;
		}

		if (match_len > 0)
		{
			data->DeleteChars((int)(word_start - data->Buf), (int)(word_end - word_start));
			data->InsertChars(data->CursorPos, candidates[0], candidates[0] + match_len);
		}

		// List matches
		sys_print("Possible matches:\n");
		for (int i = 0; i < candidates.Size; i++)
			sys_print("- %s\n", candidates[i]);
	}
	return 0;

}

void Editor_Graph_Node::draw_property_editor(AnimationGraphEditor* ed)
{
	bool update_me = false;
	bool update_everything = false;

	if (ImGui::InputText("Name", text_buffer0, sizeof text_buffer0)) {
		title = text_buffer0;
	}

	switch (type)
	{
	case animnode_type::source: {
		auto source = (Clip_Node_CFG*)node;
		auto flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCompletion;
		completion_callback_data ccd = { ed,completion_callback_data::CLIPS };
		if (ImGui::InputText("Clipname", text_buffer1, sizeof text_buffer1, flags, node_completion_callback, &ccd)) {
			source->clip_name = remove_whitespace(text_buffer1);
			update_me = true;
		}

		ImGui::Checkbox("Loop", &source->loop);
		ImGui::Checkbox("Can be sync leader", &source->can_be_leader);
		ImGui::DragFloat("Speed", &source->speed, 0.05);
		static const char* rootmotion_options[] = { "default", "remove" };
		update_me |= ImGui::ListBox("xroot", (int*)&source->rootmotion[0], rootmotion_options, 2);
		update_me |= ImGui::ListBox("yroot", (int*)&source->rootmotion[1], rootmotion_options, 2);
		update_me |= ImGui::ListBox("zroot", (int*)&source->rootmotion[2], rootmotion_options, 2);
	}
		break;
	case animnode_type::statemachine:
		break;
	case animnode_type::selector:
	{

	}

		break;
	case animnode_type::mask:
		break;
	case animnode_type::blend:
	{
		auto source = (Blend_Node_CFG*)node;
		completion_callback_data ccd = { ed,completion_callback_data::PARAMS };
		if (ImGui::InputText("Param", text_buffer1, sizeof text_buffer1, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCompletion, node_completion_callback , &ccd)) {
			update_me = true;
		}
		ImGui::DragFloat("Damp", &source->damp_factor, 0.01, 0.0, 1.0);
	}
		break;
	case animnode_type::blend2d:
	{
		auto source = (Blend2d_CFG*)node;
		completion_callback_data ccd = { ed,completion_callback_data::PARAMS };

		if (ImGui::InputText("ParamX", text_buffer1, sizeof text_buffer1, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCompletion, node_completion_callback, &ccd)) {
			update_me = true;
		}
		if (ImGui::InputText("ParamY", text_buffer2, sizeof text_buffer2, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCompletion, node_completion_callback, &ccd)) {
			update_me = true;
		}
		ImGui::DragFloat("Fade from idle", &source->fade_in, 0.05,0.0);
		ImGui::DragFloat("Damp", &source->weight_damp, 0.025, 0.0);
	}
		break;
	case animnode_type::add:
		break;
	case animnode_type::subtract:
		break;
	case animnode_type::aimoffset:
		break;
	case animnode_type::mirror:
	{
		auto source = (Mirror_Node_CFG*)node;
		completion_callback_data ccd = { ed,completion_callback_data::PARAMS };
		if (ImGui::InputText("Param", text_buffer1, sizeof text_buffer1, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCompletion, node_completion_callback, &ccd)) {
			update_me = true;
		}
	}
		break;
	case animnode_type::play_speed:
		break;
	case animnode_type::rootmotion_speed:
		break;
	case animnode_type::sync:
		break;
	case animnode_type::state:
	{
		std::vector<const char*> transition_names;
		std::vector<int> indirect;
		for (int i = 0; i < num_inputs; i++) {
			if (inputs[i]) {
				indirect.push_back(i);
				transition_names.push_back(inputs[i]->title.c_str());
			}
		}
		completion_callback_data ccd = { ed,completion_callback_data::PARAMS };

		if (transition_names.size() > 0) {
			ImGui::Combo("Transition", &state.selected_transition_for_prop_ed, transition_names.data(), transition_names.size());

			int idx = indirect[state.selected_transition_for_prop_ed];

			static char buffer[256];
			memcpy(buffer, state.transitions.at(idx).code.data(), state.transitions.at(idx).code.size());
			buffer[state.transitions.at(idx).code.size()] = 0;
			ImGui::PushID(idx);
			if (ImGui::InputText("Transition condition", buffer, sizeof buffer, ImGuiInputTextFlags_CallbackCompletion, node_completion_callback, &ccd)) {
				update_me = true;
			}
			ImGui::PopID();
			state.transitions.at(idx).code = buffer;
			
			update_me |= ImGui::DragFloat("Blend in", &state.transitions.at(idx).blend_time, 0.02, 0.0);
		}
		ImGui::Separator();
		ImGui::DragFloat("State time", &state.state_ptr->state_duration, 0.05, 0.0);
	}
		break;
	case animnode_type::root:
		break;
	case animnode_type::start_statemachine:
		break;
	case animnode_type::COUNT:
		break;
	default:
		break;
	}

	if (update_me)
		ed->update_every_node();
}

void Editor_Graph_Node::on_state_change(AnimationGraphEditor* ed)
{
	switch (type)	
	{
	case animnode_type::source: {
		auto source = (Clip_Node_CFG*)node;
		if (!source->clip_name.empty()) set_node_title(source->clip_name);
		else set_node_title("Source");
		num_inputs = 0;
	}break;
	case animnode_type::statemachine: {
		auto source = (Statemachine_Node_CFG*)node;

		source->start_state = nullptr;
		auto rootnode = ed->find_first_node_in_layer(sublayer.id, animnode_type::root);
		if (rootnode) {
			auto startnode = rootnode->inputs[0];
			if (startnode && startnode->type == animnode_type::state && startnode->is_node_valid()) {
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
	case animnode_type::blend: {
		auto source = (Blend_Node_CFG*)node;


		handle<Parameter> param = ed->editing_tree->find_param(remove_whitespace(text_buffer1).c_str());
		if (!param.is_valid() || ed->editing_tree->parameters.get_type(param) != script_parameter_type::animfloat) {
			printf("Param, %s, not valid\n", text_buffer1);
		}
		source->param = param;

		source->posea = get_nodecfg_for_slot(0);
		source->poseb = get_nodecfg_for_slot(1);
	}
		break;
	case animnode_type::blend2d: {
		auto source = (Blend2d_CFG*)node;
		handle<Parameter> param = ed->editing_tree->find_param(remove_whitespace(text_buffer1).c_str());
		if (!param.is_valid() || ed->editing_tree->parameters.get_type(param) != script_parameter_type::animfloat) {
			printf("Param, %s, not valid\n", text_buffer1);
		}
		source->xparam = param;

		param = ed->editing_tree->find_param(remove_whitespace(text_buffer2).c_str());
		if (!param.is_valid() || ed->editing_tree->parameters.get_type(param) != script_parameter_type::animfloat) {
			printf("Param, %s, not valid\n", text_buffer2);
		}
		source->yparam = param;

		source->idle = get_nodecfg_for_slot(0);
		for (int i = 0; i < 8; i++) {
			source->directions[i] = get_nodecfg_for_slot(1 + i);
		}
	}
		break;
	case animnode_type::add:
	{
		auto source = (Add_Node_CFG*)node;

		source->diff_pose = get_nodecfg_for_slot(0);
		source->base_pose = get_nodecfg_for_slot(1);
	}	break;
	case animnode_type::subtract:
	{
		auto source = (Subtract_Node_CFG*)node;

		source->ref = get_nodecfg_for_slot(0);
		source->source = get_nodecfg_for_slot(1);
	}
		break;
	case animnode_type::aimoffset:
		break;
	case animnode_type::mirror:
	{
		auto source = (Mirror_Node_CFG*)node;
		handle<Parameter> param = ed->editing_tree->find_param(remove_whitespace(text_buffer1).c_str());
		if (!param.is_valid() || ed->editing_tree->parameters.get_type(param) != script_parameter_type::animfloat) {
			printf("Param, %s, not valid\n", text_buffer1);
		}
		source->param = param;
		source->input = get_nodecfg_for_slot(0);
	}
		break;
	case animnode_type::play_speed:
		break;
	case animnode_type::rootmotion_speed:
	{
		auto source = (Scale_By_Rootmotion_CFG*)node;

		handle<Parameter> param = ed->editing_tree->find_param(remove_whitespace(text_buffer1).c_str());
		if (!param.is_valid() || ed->editing_tree->parameters.get_type(param) != script_parameter_type::animfloat) {
			printf("Param, %s, not valid\n", text_buffer1);
		}
		source->param = param;

		source->child = get_nodecfg_for_slot(0);

	}
		break;
	case animnode_type::sync:
	{
		auto source = (Sync_Node_CFG*)node;

		source->child = get_nodecfg_for_slot(0);
	}
		break;
	case animnode_type::state: {

		State* s = state.state_ptr;
		s->name = title;

		auto startnode = ed->find_first_node_in_layer(sublayer.id, animnode_type::start_statemachine);
		if (!startnode)
			s->tree = nullptr;
		else {
			ASSERT(startnode->node);
			s->tree = startnode->node;
		}

		s->transitions.clear();
		for (int i = 0; i < num_inputs; i++) {
			if (inputs[i] && !state.transitions[i].code.empty()) {
				ASSERT(inputs[i]->is_state_node());
				State_Transition st;
				st.transition_state = inputs[i]->state.state_ptr;
				std::string code = state.transitions[i].code;
				try {
					auto exp = LispLikeInterpreter::parse(code);
					st.script.compilied.compile(exp, ed->editing_tree->parameters);
				}
				catch (LispError err) {
					printf("Err: %s\n", err.msg);
					printf("Failed to compilie script transition for \"%s\" (%s)\n", title.c_str(), code.c_str());

					continue;
				}
				catch (...) {
					printf("Unknown err\n");
					printf("Failed to compilie script transition for \"%s\" (%s)\n", title.c_str(), code.c_str());
					continue;
				}
				s->transitions.push_back(st);
			}
		}
	}
		break;
	case animnode_type::root:

		// this is always the ROOT layer
		if (graph_layer == 0) {

			ed->editing_tree->root = get_nodecfg_for_slot(0);

		}


		break;
	case animnode_type::COUNT:
		break;
	default:
		break;
	}

	if (node) {
		NodeRt_Ctx ctx;
		ctx.model = ed->out.model;
		ctx.set = ed->out.set;
		ctx.tree = &ed->out.tree_rt;
		ASSERT(ctx.tree->data.size() > 0);
		ASSERT(ctx.tree->cfg != nullptr);

		node->construct(ctx);
	}
}

bool Editor_Graph_Node::is_node_valid()
{
	switch (type)
	{
	case animnode_type::source:
	{
		return true;
	}
		break;
	case animnode_type::statemachine:
	{
		auto source = (Statemachine_Node_CFG*)node;
		return source->start_state != nullptr;
	}
		break;
	case animnode_type::selector:
		break;
	case animnode_type::mask:
		break;
	case animnode_type::blend:
	{
		auto source = (Blend_Node_CFG*)node;
		return source->param.is_valid() && source->posea&&source->poseb;
	}
		break;
	case animnode_type::blend2d:
	{
		auto source = (Blend2d_CFG*)node;
		bool posesvalid = source->idle;
		for (int i = 0; i < 8; i++)
			posesvalid &= bool(source->directions[i]);
		return posesvalid && source->xparam.is_valid() && source->yparam.is_valid();
	}
		break;
	case animnode_type::add:
	{
		auto source = (Add_Node_CFG*)node;
		return source->base_pose && source->diff_pose && source->param.is_valid();
	}
		break;
	case animnode_type::subtract:
	{
		auto source = (Subtract_Node_CFG*)node;
		return source->ref && source->source;
	}
		break;
	case animnode_type::aimoffset:
		break;
	case animnode_type::mirror:
	{
		auto source = (Mirror_Node_CFG*)node;
		return source->input && source->param.is_valid();
	}
		break;
	case animnode_type::play_speed:
		break;
	case animnode_type::rootmotion_speed:
	{
		auto source = (Scale_By_Rootmotion_CFG*)node;
		return source->child && source->param.is_valid();
	}
		break;
	case animnode_type::sync:
	{
		auto source = (Sync_Node_CFG*)node;
		return source->child;
	}
		break;
	case animnode_type::state:
	{
		return state.state_ptr->tree;
	}
		break;
	case animnode_type::root:
		break;
	case animnode_type::start_statemachine:
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

	out.anim.tick_tree_new(dt);
	out.anim.ConcatWithInvPose();

	Render_Object ro;
	ro.mesh = &out.model->mesh;
	ro.mats = &out.model->mats;
	ro.transform = out.model->skeleton_root_transform;
	ro.animator = &out.anim;

	idraw->update_obj(out.obj, ro);

	auto window_sz = eng->get_game_viewport_dimensions();
	out.vs = View_Setup(out.camera.position, out.camera.front, glm::radians(70.f), 0.01, 100.0, window_sz.x, window_sz.y);
}

void AnimationGraphEditor::overlay_draw()
{

}
extern void load_mirror_remap(Model* model, const char* path);
void AnimationGraphEditor::open(const char* name)
{
	this->name = name;

	out.model = mods.find_or_load("player_FINAL.glb");
	load_mirror_remap(out.model, "./Data/Animations/remap.txt");

	out.set = out.model->animations.get();
	out.anim.set_model(out.model);
	out.obj = idraw->register_obj();

	out.anim.tree = &out.tree_rt;

	Render_Object ro;
	ro.mesh = &out.model->mesh;
	ro.mats = &out.model->mats;
	//ro.transform = out.model->skeleton_root_transform;

	idraw->update_obj(out.obj, ro);
}

void Editor_Parameter_list::update_real_param_list(ScriptVars_CFG* cfg)
{
	cfg->name_to_index.clear();
	cfg->types.clear();

	for (int i = 0; i < param.size(); i++) {
		if (param.at(i).fake_entry) continue;

		cfg->name_to_index[param.at(i).s] = cfg->types.size();
		cfg->types.push_back(param.at(i).type);
	}
}
