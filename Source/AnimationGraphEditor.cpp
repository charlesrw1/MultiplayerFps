#include "AnimationGraphEditor.h"
#include <cstdio>
#include "imnodes.h"
#include "glm/glm.hpp"

#include "Texture.h"
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

struct AnimCompletionCallbackUserData
{
	AnimationGraphEditor* ed = nullptr;
	enum {
		PARAMS,
		CLIPS,
		BONES,
		PROP_TYPE,
	}type;
};
std::vector<const char*>* anim_completion_callback_function(void* user, const char* word_start, int len);

AnimCompletionCallbackUserData param_completion = { &ed, AnimCompletionCallbackUserData::PARAMS };
AnimCompletionCallbackUserData clip_completion = { &ed, AnimCompletionCallbackUserData::CLIPS };
AnimCompletionCallbackUserData bone_completion = { &ed, AnimCompletionCallbackUserData::BONES };
AnimCompletionCallbackUserData prop_type_completion = { &ed, AnimCompletionCallbackUserData::PROP_TYPE };


class FindAnimationClipPropertyEditor : public IPropertyEditor
{
public:
	using IPropertyEditor::IPropertyEditor;


	// Inherited via IPropertyEditor
	virtual void internal_update() override
	{
		ASSERT(prop->type == core_type_id::StdString);

		auto str = (std::string*)prop->get_ptr(instance);
		auto options = anim_completion_callback_function(&clip_completion, "", 0);

		if (initial) {
			index = 0;
			for (int i = 0; i < options->size(); i++) {
				if (*str == options->at(i)) {
					index = i;
					break;
				}
			}
			initial = false;
		}

		ImGui::Combo("##combo", &index, options->data(), options->size());

		if(index < options->size())
			*str = (*options)[index];
	}

	bool initial = true;
	int index = 0;

};


class AnimationGraphEditorPropertyFactory : public IPropertyEditorFactory
{
public:

	// Inherited via IPropertyEditorFactory
	virtual IPropertyEditor* try_create(PropertyInfo* prop, void* instance) override
	{
		if (prop->name == "AG_CLIP_TYPE") {
			return new FindAnimationClipPropertyEditor(instance, prop);
		}

		return nullptr;
	}

};
static AnimationGraphEditorPropertyFactory g_AnimationGraphEditorPropertyFactory;



void AnimationGraphEditor::init()
{


	
}

void AnimationGraphEditor::close()
{
	idraw->remove_obj(out.obj);

	ImNodes::DestroyContext(imgui_node_context);

	for (int i = 0; i < nodes.size(); i++) {
		delete nodes[i];
	}
	nodes.clear();

	assert(editing_tree);
	editing_tree->arena.shutdown();
	delete editing_tree;
	editing_tree = nullptr;
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

void TabState::imgui_draw() {

	update_tab_names();

	int rendered = 0;

	if (tabs.empty()) return;

	auto forward_img = mats.find_texture("icon/forward.png");
	auto back_img = mats.find_texture("icon/back.png");


	bool wants_back = (ImGui::IsWindowFocused() && !ImGui::GetIO().WantCaptureKeyboard && ImGui::IsKeyPressed(ImGuiKey_LeftArrow));
	bool wants_forward = (ImGui::IsWindowFocused() && !ImGui::GetIO().WantCaptureKeyboard && ImGui::IsKeyPressed(ImGuiKey_RightArrow));


	ImGui::BeginDisabled(active_tab_hist.empty());
	if (ImGui::ImageButton(ImTextureID(back_img->gl_id), ImVec2(16, 16)) || (wants_back && !active_tab_hist.empty()) ) {

		forward_tab_stack.push_back(active_tab);
		active_tab = active_tab_hist.back();
		active_tab_hist.pop_back();
		active_tab_dirty = true;
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::BeginDisabled(forward_tab_stack.empty());
	if (ImGui::ImageButton(ImTextureID(forward_img->gl_id), ImVec2(16, 16)) || (wants_forward && !forward_tab_stack.empty()) ) {
		active_tab_hist.push_back(active_tab);
		active_tab = forward_tab_stack.back();
		forward_tab_stack.pop_back();
		active_tab_dirty = true;
	}
	ImGui::EndDisabled();

	// keeps a history of what that last tab actually rendered was
	static int actual_last_tab_rendered = -1;

	bool any_deleted = false;

	if (ImGui::BeginTabBar("tabs")) {
		for (int n = 0; n < tabs.size(); n++) {

			bool needs_select = n == active_tab && active_tab_dirty;

			auto flags = (needs_select) ? ImGuiTabItemFlags_SetSelected : 0;
			bool prev_open = tabs[n].open;
			bool* open_bool = (tabs[n].owner_node) ? &tabs[n].open : nullptr;
			if (ImGui::BeginTabItem(tabs[n].tabname.c_str(), open_bool, flags))
			{
				bool this_is_an_old_active_tab_or_just_skip = n != active_tab && active_tab_dirty;

				if (this_is_an_old_active_tab_or_just_skip) {
					ImGui::EndTabItem();
					continue;
				}

				bool this_tab_needs_a_reset = n != active_tab;

				uint32_t layer = (tabs[n].layer) ? tabs[n].layer->id : 0;
				auto context = (tabs[n].layer) ? tabs[n].layer->context : parent->get_default_node_context();
				if (this_tab_needs_a_reset) {
					ImNodes::ClearNodeSelection();
				}
				ImNodes::EditorContextSet(context);
				if (this_tab_needs_a_reset) {
					ImNodes::ClearNodeSelection();
				}

				auto winsize = ImGui::GetWindowSize();

				if (tabs[n].reset_pan_to_middle_next_draw) {
					ImNodes::EditorContextResetPanning(ImVec2(0,0));
					ImNodes::EditorContextResetPanning(ImVec2(winsize.x / 4, winsize.y / 2.4));
					tabs[n].reset_pan_to_middle_next_draw = false;
				}

				parent->draw_graph_layer(layer);

				rendered++;
				ImGui::EndTabItem();

				if (active_tab != n) {
					ASSERT(!active_tab_dirty);
					active_tab = n;
				}
			}
			any_deleted |= !tabs[n].open;
		}
		ImGui::EndTabBar();
	}

	active_tab_dirty = false;

	if (any_deleted) {
		int sum = 0;
		for (int i = 0; i < tabs.size(); i++) {
			if (tabs[i].open) {
				tabs[sum++] = tabs[i];
			}
		}

		forward_tab_stack.clear();
		active_tab_hist.clear();


		tabs.resize(sum);
	}
}

void AnimationGraphEditor::save_document()
{
}

void AnimationGraphEditor::create_new_document()
{
}

void AnimationGraphEditor::draw_menu_bar()
{
	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("New")) {
				create_new_document();
			}
			if (ImGui::MenuItem("Open", "Ctrl+O")) {
				open_open_dialouge = true;
			}
			if (ImGui::MenuItem("Save", "Ctrl+S")) {
				save_document();
			}

			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("View")) {
			ImGui::Checkbox("Timeline", &open_timeline);
			ImGui::Checkbox("Viewport", &open_viewport);
			ImGui::Checkbox("Property Ed", &open_prop_editor);
			ImGui::EndMenu();

		}

		if (ImGui::BeginMenu("Settings")) {
			ImGui::Checkbox("Statemachine passthrough", &statemachine_passthrough);
			if (ImGui::IsItemHovered() && ImGui::BeginTooltip()) {
				ImGui::Text("Enable passing through the blend tree\n"
					"when selecting a state node if the\n" 
					"blend tree is just a nested statemachine");
				ImGui::EndTooltip();
			}
			ImGui::EndMenu();


		}
		ImGui::EndMenuBar();
	}
}
void AnimationGraphEditor::draw_prop_editor()
{
	if (ImGui::Begin("animation graph property editor"))
	{
		if (ImGui::TreeNodeEx("Node property editor", ImGuiTreeNodeFlags_DefaultOpen)) {
			if (ImNodes::NumSelectedNodes() == 1) {
				int node = 0;
				ImNodes::GetSelectedNodes(&node);

				Editor_Graph_Node* mynode = find_node_from_id(node);

				if (node != node_last_frame) {
					node_props.clear_all();
					mynode->add_node_props(&node_props);
					node_last_frame = node;
				}

				//mynode->draw_property_editor(this);

				node_props.update();
			}
			else if (ImNodes::NumSelectedLinks() == 1) {
				int link = 0;
				ImNodes::GetSelectedLinks(&link);

				uint32_t node_id = Editor_Graph_Node::get_nodeid_from_link_id(link);
				uint32_t slot = Editor_Graph_Node::get_slot_from_id(link);
				Editor_Graph_Node* node_s = find_node_from_id(node_id);

				node_s->draw_link_property_editor(this, slot);
			}
			else {
				node_last_frame = link_last_frame = -1;
			}


			ImGui::TreePop();
		}

		//if (ImGui::TreeNodeEx("Control params", ImGuiTreeNodeFlags_DefaultOpen)) {
		//	//ed_params->imgui_draw();
		//	ImGui::TreePop();
		//}

	}
	ImGui::End();
}

void AnimationGraphEditor::draw_popups()
{

}

static ImGuiID dock_over_viewport(const ImGuiViewport* viewport, ImGuiDockNodeFlags dockspace_flags, const ImGuiWindowClass* window_class = nullptr)
{
	using namespace ImGui;

	if (viewport == NULL)
		viewport = GetMainViewport();

	SetNextWindowPos(viewport->WorkPos);
	SetNextWindowSize(viewport->WorkSize);
	SetNextWindowViewport(viewport->ID);

	ImGuiWindowFlags host_window_flags = 0;
	host_window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking;
	host_window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_MenuBar;
	if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
		host_window_flags |= ImGuiWindowFlags_NoBackground;

	PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	Begin("MAIN DOCKWIN", NULL, host_window_flags);
	PopStyleVar(3);

	ImGuiID dockspace_id = GetID("DockSpace");
	DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags, window_class);

	ed.draw_menu_bar();

	End();

	return dockspace_id;
}

void Timeline::draw_imgui()
{
	auto playimg = mats.find_texture("icon/play.png");
	auto stopimg = mats.find_texture("icon/stop.png");
	auto pauseimg = mats.find_texture("icon/pause.png");
	auto saveimg = mats.find_texture("icon/Save.png");

	if(ImGui::Begin("Timeline")) {

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5, 0.5, 0.5, 1.0));

		if (is_playing) {
			if (ImGui::ImageButton((ImTextureID)pauseimg->gl_id, ImVec2(32, 32))) {
				pause();
			}
		}
		else {
			if (ImGui::ImageButton((ImTextureID)playimg->gl_id, ImVec2(32, 32))) {
				play();
			}
		}
		ImGui::SameLine();

		auto greyed_out = ImVec4(1, 1, 1, 0.3);

		if (ImGui::ImageButton((ImTextureID)stopimg->gl_id, 
			ImVec2(32, 32), 
			ImVec2(0, 0), ImVec2(1, 1), -1, ImVec4(0, 0, 0, 0), 
			is_reset? ImVec4(1, 1, 1, 0.3) : ImVec4(1,1,1,1) ))
		{
			if(is_reset)
				stop();
		}
		ImGui::SameLine();
		if (ImGui::ImageButton((ImTextureID)saveimg->gl_id,
			ImVec2(32, 32),
			ImVec2(0, 0), ImVec2(1, 1), -1, ImVec4(0, 0, 0, 0),
			(!needs_compile) ? ImVec4(1, 1, 1, 0.3) : ImVec4(1, 1, 1, 1)))
		{
			if(needs_compile)
				save();
		}
		ImGui::PopStyleColor();


		static bool init = false;
		if (!init) {
			seq.mFrameMin = 0;
			seq.mFrameMax = 100;

			seq.add_manual_track("FIRST", 10, 30);
			seq.add_manual_track("SECOND", 12, 60);
			seq.add_manual_track("THIRD", 61, 90);
			seq.add_manual_track("FOURTH", 92, 99);


			init = true;
		}

		ImGui::PushItemWidth(100);
		ImGui::InputInt("Frame Min", &seq.mFrameMin);
		ImGui::SameLine();
		ImGui::InputInt("Frame ", &current_tick);
		ImGui::SameLine();
		ImGui::InputInt("Frame Max", &seq.mFrameMax);
		ImGui::PopItemWidth();
		int selected = -1;
		Sequencer(&seq, &current_tick, &expaned, &selected, &first_frame, ImSequencer::SEQUENCER_CHANGE_FRAME);


	}
	ImGui::End();
}

void AnimationGraphEditor::handle_imnode_creations(bool* open_popup_menu_from_drop)
{

	int start_atr = 0;
	int end_atr = 0;
	int link_id = 0;


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

		bool destroy = node_e->add_input(this, node_s, end_idx);
	}
	if (ImNodes::IsLinkDropped(&start_atr)) {
		*open_popup_menu_from_drop = true;

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


}

void AnimationGraphEditor::begin_draw()
{

	dock_over_viewport(ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);


	draw_popups();


	if (open_timeline)
		timeline.draw_imgui();

	if (open_prop_editor)
		draw_prop_editor();

	is_modifier_pressed = ImGui::GetIO().KeyAlt;


	ImGui::Begin("animation graph editor");

	if (ImGui::GetIO().MouseClickedCount[0] == 2) {

		if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_RootWindow) && ImNodes::NumSelectedNodes() == 1) {
			int node = 0;
			ImNodes::GetSelectedNodes(&node);

			Editor_Graph_Node* mynode = find_node_from_id(node);
			if (mynode->type == animnode_type::state || mynode->type == animnode_type::statemachine) {
				auto findtab = graph_tabs.find_tab_index(mynode);
				if (findtab!=-1) {
					graph_tabs.push_tab_to_view(findtab);
				}
				else {
					graph_tabs.add_tab(&mynode->sublayer, mynode, glm::vec2(0.f), true);
				}
			}
			ImNodes::ClearNodeSelection();

		}
	}

	graph_tabs.imgui_draw();

	bool open_popup_menu_from_drop = false;
	handle_imnode_creations(&open_popup_menu_from_drop);


	is_focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows | ImGuiFocusedFlags_RootWindow);

	if (open_popup_menu_from_drop || 
		(is_focused && ImGui::GetIO().MouseClicked[1]))
		ImGui::OpenPopup("my_select_popup");

	if (ImGui::BeginPopup("my_select_popup"))
	{
		bool is_sm = graph_tabs.get_active_tab()->is_statemachine_tab();

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
		ImGui::Text("%s\n", node->get_title().c_str());


		auto er = mats.find_texture("icon/error.png");
		ImGui::SameLine();
		ImGui::Image((ImTextureID)er->gl_id, ImVec2(16, 16));

		ImNodes::EndNodeTitleBar();

		for (int j = 0; j < node->num_inputs; j++) {

			ImNodesPinShape pin = ImNodesPinShape_Quad;

			if (node->inputs[j]) pin = ImNodesPinShape_TriangleFilled;

			ImNodes::BeginInputAttribute(node->getinput_id(j), pin);
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

				if (node->draw_flat_links()) {
					ImNodes::PushColorStyle(ImNodesCol_Link, color32_to_int({ 0,0xff,0 }));
					ImNodes::PushColorStyle(ImNodesCol_LinkHovered, color32_to_int({ 0xff,0xff,0xff }));
					ImNodes::PushColorStyle(ImNodesCol_LinkSelected, color32_to_int({ 0xff,0xff,0xff }));

				}
				ImNodes::Link(node->getlink_id(j), node->inputs[j]->getoutput_id(0), node->getinput_id(j), node->draw_flat_links());

				if (node->draw_flat_links()) {
					ImNodes::PopColorStyle();
					ImNodes::PopColorStyle();
					ImNodes::PopColorStyle();

				}
			}
		}
	}


	ImNodes::MiniMap();
	ImNodes::EndNodeEditor();
}
#include "DictWriter.h"
#include <fstream>
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

		case SDL_SCANCODE_SPACE:
			compile_graph_for_playing();
			{
				DictWriter write;
				editing_tree->write_to_dict(write);

				std::ofstream outfile("out.txt");
				outfile.write(write.get_output().c_str(), write.get_output().size());
				outfile.close();
			}break;
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
	graph_tabs.remove_nodes_tab(node);

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
				auto a = create_graph_node_from_type(type, graph_tabs.get_current_layer_from_tab());

				ImNodes::ClearNodeSelection();
				ImNodes::SetNodeScreenSpacePos(a->id, ImGui::GetMousePos());
				ImNodes::SelectNode(a->id);

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

// nodes/particles/entities/everything has data that HAS to be serialized to disk and read back
// some of this has custom editing functions


template<typename T>
static T* create_node_type(Animation_Tree_CFG& cfg)
{
	auto c = cfg.arena.alloc_bottom(sizeof(T));
	c = new(c)T(&cfg);
	return (T*)c;
}

Editor_Graph_Node* AnimationGraphEditor::create_graph_node_from_type(animnode_type type, uint32_t layer)
{
	auto node = add_node();
	node->graph_layer = layer;

	node->get_title() = get_animnode_name(type);
	node->node_color = get_animnode_color(type);

	node->type = type;

	if (get_animnode_typedef(type).create) {
		node->node = get_animnode_typedef(type).create(editing_tree);
	}

	switch (type)
	{
	case animnode_type::source:

		break;
	case animnode_type::statemachine:
		node->sublayer = create_new_layer(true);
		node->sm = std::make_unique< Editor_Graph_Node::statemachine_data>();
		break;
	case animnode_type::selector:
		break;
	case animnode_type::mask:
		break;
	case animnode_type::blend:

		break;
	case animnode_type::blend2d:

		node->input_pin_names[0] = "idle";
		node->input_pin_names[1] = "s";
		node->input_pin_names[2] = "sw";
		node->input_pin_names[3] = "w";
		node->input_pin_names[4] = "nw";
		node->input_pin_names[5] = "n";
		node->input_pin_names[6] = "ne";
		node->input_pin_names[7] = "e";
		node->input_pin_names[8] = "se";
		break;
	case animnode_type::add:

		node->input_pin_names[Add_Node_CFG::DIFF] = "diff";
		node->input_pin_names[Add_Node_CFG::BASE] = "base";

		break;
	case animnode_type::subtract:
		node->input_pin_names[Subtract_Node_CFG::REF] = "ref";
		node->input_pin_names[Subtract_Node_CFG::SOURCE] = "source";
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
		node->node = create_node_type<Sync_Node_CFG>(*editing_tree);

		break;
	case animnode_type::state: {
		node->sublayer = create_new_layer(false);
		node->state = std::make_unique<Editor_Graph_Node::state_data>();
		node->state->parent_statemachine = get_owning_node_for_layer(graph_tabs.get_current_layer_from_tab());
		ASSERT(node->state->parent_statemachine->type == animnode_type::statemachine);
		node->state->sm_node_parent = (Statemachine_Node_CFG*)node->state->parent_statemachine->node;

		int size = (int)node->state->sm_node_parent->states.size();
		node->state->state_handle = { size };
		node->state->sm_node_parent->states.resize(size + 1);

		for (int i = 0; i < node->state->transitions.size(); i++) {
#if 0
			node->state->transitions[i].code_prop = make_string_prop("condition", {}, anim_completion_callback_function, &param_completion, false);
			node->state->transitions[i].time_prop = make_float_prop("transition time");
#endif
		}

		node->state->get_state()->tree = nullptr;
	}break;
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
	if(node->node)
		node->num_inputs = node->node->input.count;

	return node;
}

void AnimationGraphEditor::compile_graph_for_playing()
{
	return;

	editing_tree->all_nodes.clear();
	for (int i = 0; i < nodes.size(); i++) {
		if (nodes[i]->node)
			editing_tree->all_nodes.push_back(nodes[i]->node);
	}

	//out.tree_rt.init_from_cfg(editing_tree, out.model, out.set);
	for (int i = 0; i < nodes.size(); i++) {
		nodes[i]->on_state_change(this);
	}
}

#include "ReflectionRegisterDefines.h"

PropertyInfoList Editor_Graph_Node::properties;

void Editor_Graph_Node::register_props()
{
	START_PROPS
		REG_STDSTRING(Editor_Graph_Node, title, PROP_DEFAULT),
		REG_INT(Editor_Graph_Node, id, PROP_SERIALIZE, ""),
		REG_ENUM(Editor_Graph_Node, type, PROP_SERIALIZE, "", animnode_type_def.id)
	END_PROPS;
}

void Editor_Graph_Node::remove_reference(AnimationGraphEditor* ed, Editor_Graph_Node* node)
{
	for (int i = 0; i < num_inputs; i++) {
		if (inputs[i] == node) inputs[i] = nullptr;
	}
	
	if (sm) {
		for (int i = 0; i < sm->states.size(); i++) {
			if (sm->states[i] == node) {
				sm->states[i] = nullptr;	// do this because it will be fixed up on save/load time
			}
		}
	}
	if (state) {
		if (state->parent_statemachine == node) {
			state->parent_statemachine = nullptr;
			state->sm_node_parent = nullptr;
		}
	}
}



std::vector<const char*>* anim_completion_callback_function(void* user, const char* word_start, int len)
{
	AnimCompletionCallbackUserData* auser = (AnimCompletionCallbackUserData*)user;

	static std::vector<const char*> vec;
	vec.clear();

	auto ed = auser->ed;

	if (auser->type == AnimCompletionCallbackUserData::CLIPS) {
		auto set = ed->out.set;
		for (int i = 0; i < set->imports.size(); i++) {
			auto subset = set->imports[i].mod->animations.get();
			for (int j = 0; j < subset->clips.size(); j++) {
				if (_strnicmp(subset->clips[j].name.c_str(), word_start, len) == 0)
					vec.push_back(subset->clips[j].name.c_str());
			}
		}
	}
	else if (auser->type == AnimCompletionCallbackUserData::BONES) {
		
		auto bones  = ed->out.model->bones;
		for (int i = 0; i < bones.size(); i++)
			if (_strnicmp(bones[i].name.c_str(), word_start, len) == 0)
				vec.push_back(bones[i].name.c_str());

	}

	else if (auser->type == AnimCompletionCallbackUserData::PROP_TYPE) {
		vec.push_back("vec2");
		vec.push_back("float");
		vec.push_back("int");
	}


	//else if (auser->type == AnimCompletionCallbackUserData::PARAMS) {
	//	for (int i = 0; i < ed->ed_params.param.size(); i++)
	//		if (_strnicmp(ed->ed_params.param.at(i).s.c_str(), word_start,len) == 0)
	//			vec.push_back((ed->ed_params.param.at(i).s.c_str()));
	//}
	return &vec;
}



void Editor_Graph_Node::draw_property_editor(AnimationGraphEditor* ed)
{
	bool update_me = false;
	bool update_everything = false;

	ImGui::Text("NODE \"%s\" ( %s )", get_title().c_str(), get_animnode_name(type));


	//for (auto& prop : properties)
	//	update_me |= draw_node_property(prop);

	switch (type)
	{
	case animnode_type::state:
	{
		std::vector<const char*> transition_names;
		std::vector<int> indirect;
		for (int i = 0; i < num_inputs; i++) {
			if (inputs[i]) {
				indirect.push_back(i);
				transition_names.push_back(inputs[i]->get_title().c_str());
			}
		}
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

void Editor_Graph_Node::draw_link_property_editor(AnimationGraphEditor* ed, uint32_t slot_thats_selected)
{
	if (type != animnode_type::state)
		return;

	ASSERT(inputs[slot_thats_selected]);

	auto& transition = state->transitions[slot_thats_selected];

	ImGui::Text("TRANSITION ( %s ) -> ( %s )", get_title().c_str(), inputs[slot_thats_selected]->get_title().c_str());
	ImGui::Separator();
}

void Editor_Graph_Node::on_state_change(AnimationGraphEditor* ed)
{

	if (node) {
		for (int i = 0; i < node->input.count; i++) {
			node->input[i] = get_nodecfg_for_slot(i);
		}
	}

	switch (type)	
	{
	case animnode_type::source: {
	}break;
	case animnode_type::statemachine: {
		auto source = (Statemachine_Node_CFG*)node;

		ASSERT(source);

		source->start_state = { -1 };

		auto rootnode = ed->find_first_node_in_layer(sublayer.id, animnode_type::root);
		if (rootnode) {
			auto startnode = rootnode->inputs[0];
			if (startnode && startnode->type == animnode_type::state) {
				source->start_state = startnode->state->state_handle;
			}
		}

	}break;
	case animnode_type::selector:
		break;
	case animnode_type::mask:
		break;
	case animnode_type::blend: {
		auto source = (Blend_Node_CFG*)node;
	}
		break;
	case animnode_type::blend2d: {
		

	}
		break;
	case animnode_type::add:
	{
		
	
	}	break;
	case animnode_type::subtract:
	{
		
	}
		break;
	case animnode_type::aimoffset:
		break;
	case animnode_type::mirror:
	{
	
	}
		break;
	case animnode_type::play_speed:
		break;
	case animnode_type::rootmotion_speed:
	{
	
	}
		break;
	case animnode_type::sync:
	{
	
	}
		break;
	case animnode_type::state: {

		State* s = state->get_state();
		s->name = get_title();

		auto startnode = ed->find_first_node_in_layer(sublayer.id, animnode_type::start_statemachine);
		if (!startnode)
			s->tree = nullptr;
		else {
			ASSERT(startnode->node);
			s->tree = startnode->node;
		}

		s->transitions.clear();
		for (int i = 0; i < num_inputs; i++) {

#if 0 
			if (inputs[i] && !state->transitions[i].get_code().empty()) {
				ASSERT(inputs[i]->is_state_node());
				State_Transition st;
				st.transition_state = inputs[i]->state->state_handle;;
				std::string code = state->transitions[i].get_code();
				try {
					auto exp = LispLikeInterpreter::parse(code);
					st.script.compilied.compile(exp, ed->editing_tree->parameters);
				}
				catch (LispError err) {
					printf("Err: %s\n", err.msg);
					printf("Failed to compilie script transition for \"%s\" (%s)\n", get_title().c_str(), code.c_str());

					continue;
				}
				catch (...) {
					printf("Unknown err\n");
					printf("Failed to compilie script transition for \"%s\" (%s)\n", get_title().c_str(), code.c_str());
					continue;
				}
				s->transitions.push_back(st);
			}
#endif
		}
	}
		break;
	case animnode_type::root:


		break;
	case animnode_type::COUNT:
		break;
	default:
		break;
	}
}

bool Editor_Graph_Node::is_node_valid()
{
	if (!node) return false;

	bool inputs_valid = true;
	for (int i = 0; i < node->input.count; i++) {
		if (!node->input[i]) return false;
	}

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
		return source->start_state.is_valid();
	}
		break;
	case animnode_type::selector:
		break;
	case animnode_type::mask:
		break;
	case animnode_type::blend:
	{
		auto source = (Blend_Node_CFG*)node;
		return source->param.is_valid();
	}
		break;
	case animnode_type::blend2d:
	{
		auto source = (Blend2d_CFG*)node;

		return source->xparam.is_valid() && source->yparam.is_valid();
	}
		break;
	case animnode_type::add:
	{
		auto source = (Add_Node_CFG*)node;
		return  source->param.is_valid();
	}
		break;
	case animnode_type::subtract:

		break;
	case animnode_type::aimoffset:
		break;
	case animnode_type::mirror:
	{
		auto source = (Mirror_Node_CFG*)node;
		return source->param.is_valid();
	}
		break;
	case animnode_type::play_speed:
		break;
	case animnode_type::rootmotion_speed:
	{
		auto source = (Scale_By_Rootmotion_CFG*)node;
		return source->param.is_valid();
	}
		break;
	case animnode_type::sync:
	
		break;
	case animnode_type::state:
	{
		return state->get_state()->tree;
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

void Editor_Graph_Node::add_node_props(PropertyGrid* grid)
{
	if (node)
		grid->add_property_list_to_grid(node->get_property_list(), node);

	if (type == animnode_type::source) {

		static bool initial = true;
		static PropertyInfo source_prop;
		static PropertyInfoList list;
		if (initial) {
			auto prop_list = node->get_property_list();
			auto clip_prop = prop_list->find("clip_name");
			ASSERT(clip_prop);
			source_prop.flags = PROP_EDITABLE;	// dont serialize
			source_prop.name = "AG_CLIP_TYPE";	// special clip name
			source_prop.type = core_type_id::StdString;
			source_prop.offset = clip_prop->offset;
			list = { &source_prop, 1 };
			initial = false;
		}

		grid->add_property_list_to_grid(&list, node);

	}
	else if (type == animnode_type::state) {
		grid->add_property_list_to_grid(State::get_props(), state->get_state());	// FIXME: SO DANGEROUS!
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
void AnimationGraphEditor::open(const char* name)
{
	this->name = name;

	imgui_node_context = ImNodes::CreateContext();

	ImNodes::GetIO().LinkDetachWithModifierClick.Modifier = &is_modifier_pressed;

	editing_tree = new Animation_Tree_CFG;
	editing_tree->arena.init("ATREE ARENA", 1'000'000);	// spam the memory for the editor

	graph_tabs.add_tab(nullptr, nullptr, glm::vec2(0.f), true);
	add_root_node_to_layer(0, false);

	default_editor = ImNodes::EditorContextCreate();
	ImNodes::EditorContextSet(default_editor);

	out.model = mods.find_or_load("player_FINAL.glb");
	out.set = anim_tree_man->find_set("default.txt");

	out.anim.initialize_animator(out.model, out.set, editing_tree, nullptr, nullptr);

	out.obj = idraw->register_obj();

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
		//cfg->types.push_back(param.at(i).type);
	}
}
#if 0
void Table::imgui_draw()
{

	uint32_t prop_editor_flags = ImGuiTableFlags_PadOuterX | ImGuiTableFlags_Resizable | ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg;

	int num_cols = col_names.size();

	if (ImGui::Button("add row")) {

		Table_Row row = template_row;
		row.row_id = row_id_start++;
		for (int i = 0; i < row.props.size(); i++) {
			row.props[i].loc1 = row.row_id;
			row.props[i].loc2 = i;
		}
		rows.push_back(row);
	}

	if (ImGui::BeginTable(table_name.c_str(), num_cols+allow_selecting, prop_editor_flags))
	{

		if(allow_selecting)
			ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 0.f,0);


		for (int i = 0; i < col_names.size(); i++) {
			uint32_t flags = 0;
			float w = 0.0;
			if (col_names[i].fixed_width) {
				flags = ImGuiTableColumnFlags_WidthFixed;
				w = col_names[i].start_width;
			}
			ImGui::TableSetupColumn(col_names[i].name.c_str(), flags, w, i+allow_selecting);

		}

		ImGui::TableHeadersRow();

		ImGui::PushStyleColor(ImGuiCol_FrameBg, 0);

		int row_idx = 0;
		int delete_this_index = -1;
		for (auto& row : rows)
		{
			ImGui::TableNextRow();


			if (allow_selecting) {
				ImGui::TableSetColumnIndex(0);
				ImGuiSelectableFlags selectable_flags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap;
				if (ImGui::Selectable(string_format("%d",row_idx), selected_row == row.row_id, selectable_flags, ImVec2(0, 20.f)))
				{
					selected_row = row.row_id;
				}
			}


			int col_idx = allow_selecting;
			for (auto& col : row.props) {
				ImGui::PushID(row.row_id*num_cols + col_idx);
				ImGui::TableSetColumnIndex(col_idx);
				//draw_node_property(col);
				col_idx++;
				ImGui::PopID();
			}
			row_idx++;
		}

		ImGui::PopStyleColor();
		ImGui::EndTable();

	}
}
#endif