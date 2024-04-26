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



class AgLispCodeEditorProperty : public IPropertyEditor
{
public:
	using IPropertyEditor::IPropertyEditor;

	// Inherited via IPropertyEditor
	virtual void internal_update() override
	{
		ASSERT(prop->type == core_type_id::Struct);

		auto script = (ScriptExpression*)prop->get_ptr(instance);

		ImguiInputTextCallbackUserStruct user;
		user.string = &script->script_str;
		ImGui::InputTextMultiline("##source", (char*)script->script_str.data(), script->script_str.size()+1, ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 4), ImGuiInputTextFlags_CallbackResize, imgui_input_text_callback_function, &user);
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
		if (strcmp(prop->custom_type_str, "AG_CLIP_TYPE") == 0) {
			return new FindAnimationClipPropertyEditor(instance, prop);
		}
		else if (strcmp(prop->custom_type_str, "AG_LISP_CODE") == 0) {
			return new AgLispCodeEditorProperty(instance, prop);
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

				IAgEditorNode* mynode = find_node_from_id(node);

				if (node != node_last_frame) {
					node_props.clear_all();

					std::vector<PropertyListInstancePair> info;
					mynode->get_props(info);

					for (int i = 0; i < info.size(); i++)
						node_props.add_property_list_to_grid(info[i].list, info[i].instance);

					node_last_frame = node;
				}

				//mynode->draw_property_editor(this);

				node_props.update();
			}
			else if (ImNodes::NumSelectedLinks() == 1) {
				int link = 0;
				ImNodes::GetSelectedLinks(&link);

				uint32_t node_id = IAgEditorNode::get_nodeid_from_link_id(link);
				uint32_t slot = IAgEditorNode::get_slot_from_id(link);
				IAgEditorNode* node_s = find_node_from_id(node_id);

				//node_s->get_props()
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

		uint32_t start_node_id = IAgEditorNode::get_nodeid_from_output_id(start_atr);
		uint32_t end_node_id = IAgEditorNode::get_nodeid_from_input_id(end_atr);
		uint32_t start_idx = IAgEditorNode::get_slot_from_id(start_atr);
		uint32_t end_idx = IAgEditorNode::get_slot_from_id(end_atr);

		ASSERT(start_idx == 0);

		IAgEditorNode* node_s = find_node_from_id(start_node_id);
		IAgEditorNode* node_e = find_node_from_id(end_node_id);

		bool destroy = node_e->add_input(this, node_s, end_idx);
	}
	if (ImNodes::IsLinkDropped(&start_atr)) {
		*open_popup_menu_from_drop = true;

		bool is_input = start_atr >= INPUT_START && start_atr < OUTPUT_START;
		uint32_t id = 0;
		if (is_input)
			id = IAgEditorNode::get_nodeid_from_input_id(start_atr);
		else
			id = IAgEditorNode::get_nodeid_from_output_id(start_atr);
		drop_state.from = find_node_from_id(id);
		drop_state.from_is_input = is_input;
		drop_state.slot = IAgEditorNode::get_slot_from_id(start_atr);
	}
	if (ImNodes::IsLinkDestroyed(&link_id)) {

		uint32_t node_id = IAgEditorNode::get_nodeid_from_link_id(link_id);
		uint32_t slot = IAgEditorNode::get_slot_from_id(link_id);
		IAgEditorNode* node_s = find_node_from_id(node_id);

		node_s->on_remove_pin(slot);
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

			IAgEditorNode* mynode = find_node_from_id(node);
			
			editor_layer* layer = mynode->get_layer();

			if (layer) {
				auto findtab = graph_tabs.find_tab_index(mynode);
				if (findtab!=-1) {
					graph_tabs.push_tab_to_view(findtab);
				}
				else {
					graph_tabs.add_tab(layer, mynode, glm::vec2(0.f), true);
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
	auto strong_error = mats.find_texture("icon/fatalerr.png");
	auto weak_error = mats.find_texture("icon/error.png");
	auto info_str = mats.find_texture("icon/question.png");

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

		if (!node->compile_error_string.empty()) {
			ImGui::SameLine();
			ImGui::Image((ImTextureID)strong_error->gl_id, ImVec2(16, 16));

			if (ImGui::IsItemHovered() && ImGui::BeginTooltip()) {
				ImGui::Text(node->compile_error_string.c_str());
				ImGui::EndTooltip();
			}

		}
		
		if (node->children_have_errors) {
			ImGui::SameLine();
			ImGui::Image((ImTextureID)weak_error->gl_id, ImVec2(16, 16));

			if (ImGui::IsItemHovered() && ImGui::BeginTooltip()) {
				ImGui::Text("children have errors");
				ImGui::EndTooltip();
			}
		}



		ImNodes::EndNodeTitleBar();

		for (int j = 0; j < node->num_inputs; j++) {

			ImNodesPinShape pin = ImNodesPinShape_Quad;

			if (node->inputs[j].other_node) pin = ImNodesPinShape_TriangleFilled;

			ImNodes::BeginInputAttribute(node->getinput_id(j), pin);
			if (!node->inputs[j].pin_name.empty())
				ImGui::TextUnformatted(node->inputs[j].pin_name.c_str());
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
			if (node->inputs[j].other_node) {

				if (node->draw_flat_links()) {
					ImNodes::PushColorStyle(ImNodesCol_Link, color32_to_int({ 0,0xff,0 }));
					ImNodes::PushColorStyle(ImNodesCol_LinkHovered, color32_to_int({ 0xff,0xff,0xff }));
					ImNodes::PushColorStyle(ImNodesCol_LinkSelected, color32_to_int({ 0xff,0xff,0xff }));

				}
				ImNodes::Link(node->getlink_id(j), node->inputs[j].other_node->getoutput_id(0), node->getinput_id(j), node->draw_flat_links());

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
			uint32_t nodeid = IAgEditorNode::get_nodeid_from_link_id(ids[i]);
			uint32_t slot = IAgEditorNode::get_slot_from_id(ids[i]);
			IAgEditorNode* node = find_node_from_id(nodeid);

			node->on_remove_pin(slot);
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



void AnimationGraphEditor::remove_node_from_id(uint32_t id)
{
	return remove_node_from_index(find_for_id(id), false);
}

void AnimationGraphEditor::nuke_layer(uint32_t id)
{
	for (int i = 0; i < nodes.size(); i++) {
		if (!nodes[i]) continue;

		if (nodes[i]->graph_layer != id)
			continue;

		auto sublayer = nodes[i]->get_layer();
		if (sublayer) {
			graph_tabs.remove_nodes_tab(nodes[i]);
			nuke_layer(sublayer->id);
		}

		delete nodes[i];
		nodes[i] = nullptr;
	}
}

void AnimationGraphEditor::remove_node_from_index(int index, bool force)
{
	auto node = nodes.at(index);

	if (!node->can_user_delete() && !force)
		return;

	for (int i = 0; i < nodes.size(); i++) {
		if (i != index) {
			nodes[i]->remove_reference(node);
		}
	}

	// node has a sublayer, remove child nodes

	auto sublayer = node->get_layer();
	if (sublayer) {
		// remove tab reference
		graph_tabs.remove_nodes_tab(node);
		nuke_layer(sublayer->id);
	}


	delete node;
	nodes[index] = nullptr;

	int sum = 0;
	for (int i = 0; i < nodes.size(); i++) {
		if (nodes[i]) {
			nodes[sum++] = nodes[i];
		}
	}
	nodes.resize(sum);
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
	case animnode_type::start_statemachine:
		return { 94, 2, 2 };

	case animnode_type::statemachine:
		return { 82, 2, 94 };
	case animnode_type::state:
		return { 15, 61, 16 };
	case animnode_type::source:
		return { 1, 0, 74 };


	case animnode_type::selector:
	case animnode_type::mask:
	case animnode_type::blend:
	case animnode_type::aimoffset:
	case animnode_type::blend2d:
		return { 26, 75, 79 };

	case animnode_type::add:
	case animnode_type::subtract:
		return { 44, 57, 71 };

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

void AgEditor_BaseNode::init()
{
	node = get_animnode_typedef(type).create(ed.editing_tree);
	ASSERT(node);

	num_inputs = 2;

	switch (type) {
	case animnode_type::blend2d: {
		inputs[0].pin_name = "idle";
		inputs[1].pin_name = "s";
		inputs[2].pin_name = "sw";
		inputs[3].pin_name = "w";
		inputs[4].pin_name = "nw";
		inputs[5].pin_name = "n";
		inputs[6].pin_name = "ne";
		inputs[7].pin_name = "e";
		inputs[8].pin_name = "se";
		num_inputs = 9;
	}break;
	case animnode_type::add:
		inputs[Add_Node_CFG::DIFF].pin_name = "diff";
		inputs[Add_Node_CFG::BASE].pin_name = "base";
		break;
	case animnode_type::subtract:
		inputs[Subtract_Node_CFG::REF].pin_name = "ref";
		inputs[Subtract_Node_CFG::SOURCE].pin_name = "source";
		break;

	case animnode_type::selector:
	case animnode_type::mirror:
	case animnode_type::play_speed:
	case animnode_type::rootmotion_speed:
	case animnode_type::sync:
		num_inputs = 1; 
		break;
	case animnode_type::source: 
		num_inputs = 0; 
		break;

	}
}

bool AgEditor_BaseNode::compile_my_data()
{
	ASSERT(node->input.count >= num_inputs);	// FIXME
	bool has_null_input = false;
	for (int i = 0; i < num_inputs; i++) {
		has_null_input |= !bool(inputs[i].other_node);
		if (inputs[i].other_node && inputs[i].other_node->get_graph_node()) {
			node->input[i] = inputs[i].other_node->get_graph_node();
		}
	}
	
	compile_error_string.clear();
	compile_info_string.clear();

	// selector nodes can have empty inputs
	if (type != animnode_type::selector) {
		if (has_null_input) {
			append_fail_msg("[ERROR] missing inputs\n");
		}
	}
	else {
		if (has_null_input)
			append_info_msg("[INFO] missing inputs\n");
	}

	if (type != animnode_type::rootmotion_speed
		&& type != animnode_type::blend2d
		&& type != animnode_type::aimoffset
		&& type != animnode_type::subtract
		&& type != animnode_type::source

		) {

		if (!node->param.is_valid())
			append_fail_msg("[ERROR] missing parameter\n");
	}

	if (type == animnode_type::blend2d) {

		Blend2d_CFG* blend2d = (Blend2d_CFG*)node;
		if (!blend2d->xparam.is_valid() || !blend2d->yparam.is_valid())
			append_fail_msg("[ERROR] missing parameter(s)\n");
	}

	if (type == animnode_type::source) {

		Clip_Node_CFG* clip = (Clip_Node_CFG*)node;
		if (clip->clip_name.empty())
			append_info_msg("[ERROR] animation string is empty\n");
	}

	return compile_error_string.empty();
}

void AgEditor_BaseNode::get_props(std::vector<PropertyListInstancePair>& props)
{
	props.push_back({ node->get_property_list(),node });
}

bool AgEditor_StateNode::traverse_and_find_errors()
{
	children_have_errors = false;

	if (type != animnode_type::start_statemachine) {
		auto startnode = ed.find_first_node_in_layer(sublayer.id, animnode_type::root);

		if (startnode->inputs[0].other_node)
			children_have_errors |= !startnode->inputs[0].other_node->traverse_and_find_errors();
		// else is an error too, but its already built into compile_error_string
	}
	return !children_have_errors && compile_error_string.empty();
}

bool AgEditor_StateNode::compile_data_for_statemachine()
{
	compile_error_string.clear();

	State* s = parent_statemachine->get_state(state_handle);
	s->name = get_title();

	ASSERT(s->transitions.size() == output.size());
	for (int i = 0; i < output.size(); i++) {
		AgEditor_StateNode* out_state = output[i].output_to;
		State_Transition* st = s->transitions.data() + i;

		st->transition_state = output[i].output_to->state_handle;

		// compile script

		if (st->script.script_str.empty()) {
			append_fail_msg(string_format("[ERROR] script (-> %s) is empty\n", out_state->get_title().c_str()));
		}
		else {

			std::string code = st->script.script_str;
			const char* err_str = nullptr;
			try {
				auto exp = LispLikeInterpreter::parse(code);

				if (exp.type != LispExp::int_type) {
					err_str = "wrong output type, expected boolean";
				}
				else {
					st->script.compilied.compile(exp, ed.editing_tree->parameters);
				}
			}
			catch (LispError err) {
				err_str = err.msg;
			}
			catch (...) {
				err_str = "unknown error";

			}

			if (err_str) {
				append_fail_msg(string_format("[ERROR] script (-> %s) compile failed ( %s )\n", err_str));
			}
		}
	}

	// append tree

	if (type != animnode_type::start_statemachine) {
		auto startnode = ed.find_first_node_in_layer(sublayer.id, animnode_type::root);
		ASSERT(startnode);

		if (!startnode->inputs[0].other_node) {
			append_fail_msg(string_format("[ERROR] missing start state in blend tree \n"));
		}
		else {

			IAgEditorNode* rootnode = startnode->inputs[0].other_node;
			ASSERT(rootnode->get_graph_node());

			s->tree = rootnode->get_graph_node();
		}
	}

	return compile_error_string.empty();	// empty == no errors generated
	
}

void AgEditor_StateNode::on_remove_pin(int slot, bool force)
{
	ASSERT(inputs[slot].other_node);
	
	if (inputs[slot].other_node->is_state_node()) {
		((AgEditor_StateNode*)inputs[slot].other_node)->remove_output_to(this);
		inputs[slot].other_node = nullptr;
		inputs[slot].pin_name = "Unknown";
	}
}

void AgEditor_StateNode::get_props(std::vector<PropertyListInstancePair>& props)
{
	props.push_back({ State::get_props(),parent_statemachine->get_state(state_handle) });
}

 bool AgEditor_StateNode::add_input(AnimationGraphEditor* ed, IAgEditorNode* input, uint32_t slot) {

	for (int i = 0; i < inputs.size(); i++) {
		if (inputs[i].other_node == input) {
			return true;
		}
	}

	ASSERT(input->is_state_node());

	inputs[slot].other_node = input;
	inputs[slot].pin_name = input->get_title();

	AgEditor_StateNode* statenode = (AgEditor_StateNode*)input;
	statenode->on_output_create(this);


	if (grow_pin_count_on_new_pin()) {
		if (num_inputs > 0 && inputs[num_inputs - 1].other_node)
			num_inputs++;
	}

	return false;
}

void AgEditor_StateNode::get_link_props(std::vector<PropertyListInstancePair>& props, int slot)
{
	ASSERT(inputs[slot].other_node);
	ASSERT(state_handle.is_valid());
	ASSERT(inputs[slot].other_node->is_state_node());

	((AgEditor_StateNode*)inputs[slot].other_node)->get_transition_props(this, props);
}

void AgEditor_StateNode::on_output_create(AgEditor_StateNode* node_to_output)
{
	output.push_back({ node_to_output });
	parent_statemachine->get_state(state_handle)->transitions.push_back({});
}

void AgEditor_StateNode::remove_output_to(AgEditor_StateNode* node)
{

	bool already_seen = false;
	for (int i = 0; i < output.size(); i++) {

		// WARNING transitions invalidation potentially!
		if (output[i].output_to == node) {

			ASSERT(!already_seen);

			output.erase(output.begin() + i);
			auto state = parent_statemachine->get_state(state_handle);
			state->transitions.erase(state->transitions.begin() + i);
			already_seen = true;
			i--;
		}
	}

}

void AgEditor_StateNode::get_transition_props(AgEditor_StateNode* to, std::vector<PropertyListInstancePair>& props)
{
	for (int i = 0; i < output.size(); i++) {
		if (output[i].output_to == to) {
			
			State* s = parent_statemachine->get_state(state_handle);
			ASSERT(i < s->transitions.size());

			// WARNING: this pointer becomes invalid if s->transitions is resized, this shouldnt happen
			props.push_back({ State_Transition::get_props(), s->transitions.data() + i });
			return;
		}
	}
	ASSERT(0);
}

void AgEditor_StateMachineNode::get_props(std::vector<PropertyListInstancePair>& props)
{
	props.push_back({ node->get_property_list(),node });
}

void AgEditor_StateNode::init()
{
	if(type != animnode_type::start_statemachine)
		sublayer  = ed.create_new_layer(false);

	auto parent= ed.get_owning_node_for_layer(graph_layer);
	ASSERT(parent->is_statemachine());
	parent_statemachine = (AgEditor_StateMachineNode*)parent;

	state_handle = parent_statemachine->add_new_state(this);

	num_inputs = 1;

	if (type != animnode_type::start_statemachine)
		ed.add_root_node_to_layer(sublayer.id, false);
}

void AgEditor_StateMachineNode::init()
{
	node = (Statemachine_Node_CFG*)get_animnode_typedef(type).create(ed.editing_tree);
	ASSERT(node);

	sublayer = ed.create_new_layer(true);
	num_inputs = 0;


	ed.add_root_node_to_layer(sublayer.id, true);
}

bool AgEditor_StateMachineNode::traverse_and_find_errors()
{
	children_have_errors = false;

	for (int i = 0; i < states.size(); i++) {
		children_have_errors |= !states[i]->traverse_and_find_errors();
	}

	return !children_have_errors && compile_error_string.empty();
}

void AgEditor_StateNode::remove_reference(IAgEditorNode* node)
{	
	if (node->is_state_node()) {
		remove_output_to((AgEditor_StateNode*)node);
	}

	// node gets deleted since its in the layer
	IAgEditorNode::remove_reference(node);
	
	if (node == parent_statemachine) {
		parent_statemachine = nullptr;
	}
}

bool AgEditor_StateNode::compile_my_data()
{
	// empty, compiling done through statemachine node
	return true;
}


void AgEditor_StateMachineNode::remove_reference(IAgEditorNode* node)
{

	IAgEditorNode::remove_reference(node);

	for (int i = 0; i < states.size(); i++) {
		if (states[i] == node) {
			states[i] = nullptr;	// do this because it will be fixed up on save/load time
		}
	}
}

bool AgEditor_StateMachineNode::compile_my_data()
{

	int sum = 0;
	for (int i = 0; i < states.size(); i++) {
		if (states[i]) {
			states[sum] = states[i];
			node->states[sum] = node->states[i];
			sum++;
		}
	}
	node->states.resize(sum);
	states.resize(sum);

	bool has_errors = false;

	for (int i = 0; i < states.size(); i++) {
		has_errors |= !states[i]->compile_data_for_statemachine();
	}

	if (has_errors)
		append_fail_msg("[ERROR] state machine states contain errors\m");

	node->start_state = { -1 };

	auto state_enter = (AgEditor_StateNode*)ed.find_first_node_in_layer(sublayer.id, animnode_type::start_statemachine);
	ASSERT(state_enter);	// should never be deleted


	//if (!startnode.other_node) {
	//	append_fail_msg("[ERROR] state machine has no start state\n");
	//}
	//else {
		//ASSERT(startnode.other_node->is_state_node());
		auto handle = state_enter->state_handle;
		ASSERT(handle.is_valid());
		node->start_state = handle;
	//}
}

handle<State> AgEditor_StateMachineNode::add_new_state(AgEditor_StateNode* node_) {
	ASSERT(states.size() == node->states.size());
	int idx = states.size();
	states.resize(idx + 1);
	states[idx] = node_;

	node->states.resize(idx + 1);

	return { idx };
}

State* AgEditor_StateMachineNode::get_state(handle<State> state) {
	ASSERT(state.is_valid() && state.id < node->states.size());
	return &node->states.at(state.id);
}

IAgEditorNode* AnimationGraphEditor::create_graph_node_from_type(animnode_type type, uint32_t layer)
{

	IAgEditorNode* node = nullptr;

	if (type == animnode_type::statemachine) {
		node = new AgEditor_StateMachineNode;
	}
	else if (type == animnode_type::state || type == animnode_type::start_statemachine) {
		node = new AgEditor_StateNode;
	}
	else if (type == animnode_type::root) {
		node = new IAgEditorNode;
	}
	else {
		node = new AgEditor_BaseNode;
	}

	node->type = type;
	node->id = current_id++;
	nodes.push_back(node);
	node->graph_layer = layer;
	node->node_color = get_animnode_color(type);

	node->init();

	return node;
}

bool AnimationGraphEditor::compile_graph_for_playing()
{
	editing_tree->all_nodes.clear();
	for (int i = 0; i < nodes.size(); i++) {
		if (nodes[i]->get_graph_node())
			editing_tree->all_nodes.push_back(nodes[i]->get_graph_node());
	}

	for (int i = 0; i < nodes.size(); i++) {
		nodes[i]->compile_my_data();
	}

	IAgEditorNode* output_pose = find_first_node_in_layer(0, animnode_type::root);
	ASSERT(output_pose);
	output_pose->compile_error_string.clear();
	if (output_pose->inputs[0].other_node) {
		editing_tree->root = output_pose->inputs[0].other_node->get_graph_node();
		ASSERT(editing_tree->root);
	}
	else
		output_pose->append_fail_msg("[ERROR] no output pose\n");

	bool tree_is_good_to_run = output_pose->traverse_and_find_errors();

	return tree_is_good_to_run;
}

#include "ReflectionRegisterDefines.h"

PropertyInfoList IAgEditorNode::properties;

void IAgEditorNode::init()
{
	ASSERT(type == animnode_type::root || type == animnode_type::start_statemachine);

	if (type == animnode_type::root)
		num_inputs = 1;
	if (type == animnode_type::start_statemachine)
		num_inputs = 0;

}

void IAgEditorNode::register_props()
{
	START_PROPS
		REG_STDSTRING(IAgEditorNode, title, PROP_DEFAULT),
		REG_INT(IAgEditorNode, id, PROP_SERIALIZE, ""),
		REG_ENUM(IAgEditorNode, type, PROP_SERIALIZE, "", animnode_type_def.id)
	END_PROPS;
}



void IAgEditorNode::remove_reference(IAgEditorNode* node)
{
	for (int i = 0; i < num_inputs; i++) {
		if (inputs[i].other_node == node)
			on_remove_pin(i, true);
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

		auto bones = ed->out.model->bones;
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



bool IAgEditorNode::compile_my_data()
{
	return true;
}

bool IAgEditorNode::traverse_and_find_errors()
{
	children_have_errors = false;
	for (int i = 0; i < num_inputs; i++) {
		if (inputs[i].other_node)
			children_have_errors |= !inputs[i].other_node->traverse_and_find_errors();
	}

	return !children_have_errors && compile_error_string.empty();
}


void AnimationGraphEditor::tick(float dt)
{
	{
		int x = 0, y = 0;
		if (eng->game_focused)
			SDL_GetRelativeMouseState(&x, &y);
		out.camera.update_from_input(eng->keys, x, y, glm::mat4(1.f));
	}


	Render_Object ro;
	ro.mesh = &out.model->mesh;
	ro.mats = &out.model->mats;
	
	if (running) {
		out.anim.tick_tree_new(dt);
		ro.transform = out.model->skeleton_root_transform;
		ro.animator = &out.anim;
	}

	idraw->update_obj(out.obj, ro);

	auto window_sz = eng->get_game_viewport_dimensions();
	out.vs = View_Setup(out.camera.position, out.camera.front, glm::radians(70.f), 0.01, 100.0, window_sz.x, window_sz.y);
}


void AnimationGraphEditor::compile_and_run()
{
	bool good_to_run = compile_graph_for_playing();

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
	ro.transform = out.model->skeleton_root_transform;

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