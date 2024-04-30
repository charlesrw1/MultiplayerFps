#include "AnimationGraphEditor.h"
#include <cstdio>
#include "imnodes.h"
#include "glm/glm.hpp"

#include "Texture.h"
#include "Game_Engine.h"
#include "GlobalEnumMgr.h"

#include "DictWriter.h"
#include <fstream>

#include "MyImguiLib.h"

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
int EditorControlParamProp::unique_id_generator = 0;

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
		ASSERT(prop->type == core_type_id::StdString);

		auto script = (std::string*)prop->get_ptr(instance);

		ImguiInputTextCallbackUserStruct user;
		user.string = script;
		if (ImGui::InputTextMultiline("##source",
			(char*)script->data(),
			script->size() + 1,
			ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 4),
			ImGuiInputTextFlags_CallbackResize,
			imgui_input_text_callback_function,
			&user)) {
			script->resize(strlen(script->data()));
		}

	}

	bool initial = true;
	int index = 0;

};


class AgEnumFinder : public IPropertyEditor
{
public:
	using IPropertyEditor::IPropertyEditor;

	// Inherited via IPropertyEditor
	virtual void internal_update() override
	{
		ASSERT(prop->type == core_type_id::Int32);

		ImGui::Text("Hello world\n");
	}

};


ImVec4 scriptparamtype_to_color(script_parameter_type type)
{
	ASSERT((int)type < 4);

	static ImVec4 color[] = {
		color32_to_imvec4({120, 237, 100}),
		color32_to_imvec4({245, 27, 223}),
		color32_to_imvec4({10, 175, 240}),
		color32_to_imvec4({240, 198, 12}),
	};
	return color[(int)type];
}

class AgParamFinder : public IPropertyEditor
{
public:
	using IPropertyEditor::IPropertyEditor;

	// Inherited via IPropertyEditor
	virtual void internal_update() override
	{
		ASSERT(prop->type == core_type_id::Int32);

		int selected_id = prop->get_int(instance);

		const auto& options = ed.control_params.get_control_params();
		int array_idx = -1;
		for (int i = 0; i < options.size(); i++) {
			if (options[i].current_id == selected_id) {
				array_idx = i;
				break;
			}
		}
		if (array_idx == -1) selected_id = -1;

		if (array_idx != -1)
			ImGui::PushStyleColor(ImGuiCol_Text, scriptparamtype_to_color(options[array_idx].type));

		if (!ImGui::BeginCombo("##paramfinder", (array_idx == -1) ? nullptr : options[array_idx].name.c_str(), ImGuiComboFlags_None)) {
			if (array_idx != -1)
				ImGui::PopStyleColor();
			return;
		}

		if (array_idx != -1)
			ImGui::PopStyleColor();

		bool value_changed = false;
		bool does_id_map_to_anything = false;
		for (int i = 0; i < options.size(); i++)
		{
			ImGui::PushID(i);
			const bool item_selected = options[i].current_id == selected_id;
			does_id_map_to_anything |= item_selected;
			const char* item_text = options[i].name.c_str();
		
			ImGui::PushStyleColor(ImGuiCol_Text, scriptparamtype_to_color(options[i].type));

			if (ImGui::Selectable(item_text, item_selected))
			{
				value_changed = true;
				selected_id = options[i].current_id;
			}
			if (item_selected)
				ImGui::SetItemDefaultFocus();
			ImGui::PopID();

			ImGui::PopStyleColor();
		}

		ImGui::EndCombo();
		prop->set_int(instance, selected_id);
	}

};



class ControlParamArrayHeader : public IArrayHeader
{
	using IArrayHeader::IArrayHeader;

	// Inherited via IArrayHeader
	virtual bool has_delete_all() override { return false; }
	virtual bool imgui_draw_header(int index) {
		using proptype = EditorControlParamProp;
		std::vector<proptype>* array_ = (std::vector<proptype>*)prop->get_ptr(instance);
		ASSERT(index >= 0 && index < array_->size());
		proptype& prop_ = array_->at(index);
		bool open = ImGui::TreeNode("##header");
		if (open) 
			ImGui::TreePop();
		ImGui::SameLine(0);

		ImGui::PushStyleColor(ImGuiCol_Text, scriptparamtype_to_color(prop_.type));
		ImGui::Text(prop_.name.c_str());
		ImGui::PopStyleColor();


		return open;
	}
	virtual void imgui_draw_closed_body(int index)
	{
		using proptype = EditorControlParamProp;
		std::vector<proptype>* array_ = (std::vector<proptype>*)prop->get_ptr(instance);
		ASSERT(index >= 0 && index < array_->size());
		proptype& prop_ = array_->at(index);

		ImGui::PushStyleColor(ImGuiCol_Text, color32_to_imvec4({ 153, 152, 156 }));
		const char* name = GlobalEnumDefMgr::get().get_enum_name(script_parameter_type_def.id, (int)prop_.type);
		ImGui::Text("%s", name);
		ImGui::PopStyleColor();
	}
	friend class ControlParamsWindow;
};



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



	// Handle Hovered links
	int link = 0;
	if (ImNodes::IsLinkHovered(&link)) {


		uint32_t node_id = IAgEditorNode::get_nodeid_from_link_id(link);
		uint32_t slot = IAgEditorNode::get_slot_from_id(link);
		IAgEditorNode* node_s = ed.find_node_from_id(node_id);
		if (node_s->is_state_node()) {

			AgEditor_StateNode* state = (AgEditor_StateNode*)node_s;
			if (state->inputs[slot].other_node) {
				ImGui::BeginTooltip();
				ASSERT(state->inputs[slot].other_node->is_state_node());
				auto other_node = (AgEditor_StateNode*)state->inputs[slot].other_node;
				// should always be true
				auto other_name = !other_node->name_is_default() ? other_node->title : other_node->get_default_name();

				auto my_name = !state->name_is_default() ? state->title : state->get_default_name();

				ImGui::Text("%s ->\n%s", other_name.c_str(), my_name.c_str());
				ImGui::Separator();

				auto st = other_node->get_state_transition_to(state);
				ASSERT(st);

				if (st->is_a_continue_transition())
					ImGui::TextColored(ImVec4(0.2, 1.0, 0.2, 1.0), "CONTINUE");
				else
					ImGui::TextColored(ImVec4(1.0, 0.3, 0.3, 1.0), st->script_uncompilied.c_str());

				ImGui::EndTooltip();
			}

		}

	}
}

void AnimationGraphEditor::save_document()
{
	if (name.empty()) {
		ImGui::PushID(0);
		ImGui::OpenPopup("Save file dialog");
		ImGui::PopID();
		return;
	}


	DictWriter write;
	editing_tree->write_to_dict(write);
	save_editor_nodes(write);

	std::ofstream outfile(name);
	outfile.write(write.get_output().c_str(), write.get_output().size());
	outfile.close();
}

void AnimationGraphEditor::save_editor_nodes(DictWriter& out)
{
	std::unordered_map<Node_CFG*, int> node_ptr_to_output_index;
	// use these to fixup editor nodes on reload
	for (int i = 0; i < editing_tree->all_nodes.size(); i++) {
		node_ptr_to_output_index[editing_tree->all_nodes[i]] = i;
	}

	out.write_value("editor");
	out.write_item_start();
	out.write_key_list_start("nodes");
	for (int i = 0; i < nodes.size(); i++) {
		IAgEditorNode* node = nodes[i];
		out.write_item_start();
		out.write_key_value("name", node->get_title().c_str());
		Node_CFG* node_cfg = node->get_graph_node();
		if (node_cfg) {
			ASSERT(node_ptr_to_output_index.find(node_cfg) != node_ptr_to_output_index.end());
			out.write_key_value("node_idx", string_format("%d",node_ptr_to_output_index[node_cfg]));
		}

		out.write_key_value("graph_layer", string_format("%d",node->graph_layer));
		out.write_key_value("id", string_format("%d", node->id));
		
		const char* enum_type = GlobalEnumDefMgr::get().get_enum_name(animnode_type_def.id, (int)node->type);
		out.write_key_value("type", enum_type);
		
		if (node->get_layer()) {
			auto sublayer = node->get_layer();
			out.write_key_value("sublayer_id", string_format("%d", sublayer->id));
			// serialize ImNodes context string

			out.write_key_value("sublayer_state", ImNodes::SaveEditorStateToIniString(sublayer->context));
		}

		if (node->is_state_node()) {

			AgEditor_StateNode* statenode = (AgEditor_StateNode*)node;
			out.write_key_value("state_handle", string_format("%d", statenode->state_handle_internal.id));
			ASSERT(node_ptr_to_output_index.find(statenode->parent_statemachine->node) != node_ptr_to_output_index.end());
			out.write_key_value("parent_state_machine", string_format("%d", node_ptr_to_output_index[statenode->parent_statemachine->node]));
		}
		out.write_item_end();
	}
	out.write_list_end();
	out.write_item_end();
}

void AnimationGraphEditor::create_new_document()
{
}

void AnimationGraphEditor::pause_playback()
{
	ASSERT(playback != graph_playback_state::stopped);
	playback = graph_playback_state::paused;
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
void AnimationGraphEditor::start_or_resume_playback()
{
	if (playback == graph_playback_state::stopped)
		compile_and_run();
	else
		playback = graph_playback_state::running;
}
void AnimationGraphEditor::draw_prop_editor()
{
	if (ImGui::Begin("animation graph property editor"))
	{
		
		if (ImNodes::NumSelectedNodes() == 1) {
			int node = 0;
			ImNodes::GetSelectedNodes(&node);

			IAgEditorNode* mynode = find_node_from_id(node);

			if (node != sel.node_last_frame) {
				sel.link_last_frame = -1;

				node_props.clear_all();

				std::vector<PropertyListInstancePair> info;
				mynode->get_props(info);

				for (int i = 0; i < info.size(); i++) {
					if(info[i].list) /* some nodes have null props */
						node_props.add_property_list_to_grid(info[i].list, info[i].instance);
				}

				sel.node_last_frame = node;
			}

			node_props.update();
		}
		else if (ImNodes::NumSelectedLinks() == 1) {
			int link = 0;
			ImNodes::GetSelectedLinks(&link);

			uint32_t node_id = IAgEditorNode::get_nodeid_from_link_id(link);
			uint32_t slot = IAgEditorNode::get_slot_from_id(link);
			IAgEditorNode* node_s = find_node_from_id(node_id);

			if (!node_s->is_state_node()) {
				sel.link_last_frame = -1;
				sel.node_last_frame = -1;
			}
			else {
				AgEditor_StateNode* state = (AgEditor_StateNode*)node_s;
				if (link != sel.link_last_frame) {
					sel.node_last_frame = -1;

					node_props.clear_all();

					std::vector<PropertyListInstancePair> info;
					state->get_link_props(info, slot);

					for (int i = 0; i < info.size(); i++) {
						if (info[i].list) /* some nodes have null props */
							node_props.add_property_list_to_grid(info[i].list, info[i].instance);
					}

					sel.link_last_frame = link;
				}

				node_props.update();
			}
		
		}
		else {
			sel.link_last_frame = -1;
			sel.node_last_frame = -1;

			ImGui::Text("No node selected\n");
		}

	}
	ImGui::End();
}

void AnimationGraphEditor::stop_playback()
{
	playback = graph_playback_state::stopped;
}

void AnimationGraphEditor::draw_popups()
{

	ImGui::PushID(0);
	if (ImGui::BeginPopupModal("Save file dialog")) {

		static bool alread_exists_err = false;
		static bool cant_open_path_err = false;
		static char buffer[256];
		static bool init = true;
		if (init) {
			buffer[0] = 0;
			alread_exists_err = false;
			cant_open_path_err = false;
			init = false;
		}
		bool write_out = false;
		ImGui::TextColored(ImVec4(0.5, 0.5, 0.5, 1), "Graphs are saved under /Data/Animations/Graphs/");

		bool returned_true = false;
		if (!alread_exists_err) {
			ImGui::Text("Enter path: ");
			ImGui::SameLine();
			returned_true = ImGui::InputText("##pathinput", buffer, 256, ImGuiInputTextFlags_EnterReturnsTrue);
		}

		if (returned_true) {
			const char* full_path = string_format("./Data/Animations/Graphs/%s", buffer);
			bool already_exists = Files::does_file_exist(full_path);
			cant_open_path_err = false;
			alread_exists_err = false;
			if (already_exists)
				alread_exists_err = true;
			else {
				std::ofstream test_open(full_path);
				if (!test_open)
					cant_open_path_err = true;
			}
			if (!alread_exists_err && !cant_open_path_err) {
				write_out = true;
			}
		}
		if (alread_exists_err) {
			ImGui::Text("File already exists. Overwrite?");
			if (ImGui::Button("Yes")) {
				write_out = true;
			}
			ImGui::SameLine();
			if (ImGui::Button("No")) {
				alread_exists_err = false;
			}
		}
		else if (cant_open_path_err) {
			ImGui::Text("Cant open path\n");
		}

		if (write_out) {
			name = buffer;
			init = true;
			ImGui::CloseCurrentPopup();
			save_document();
		}

		ImGui::EndPopup();
	}

	if (ImGui::BeginPopupModal("Open file dialog"), nullptr) {

		ImGui::Text("Animations are saved to $WorkingDir/Data/Animations/Graphs");
		const char* path = "Data/Animations/Graphs";
		char buffer[256];
		int selected = -1;
	}
	ImGui::PopID();
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

	if(ImGui::Begin("Timeline")) {


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

		node_s->on_post_remove_pins();
	}


}

void AnimationGraphEditor::begin_draw()
{
	dock_over_viewport(ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

	node_props.set_read_only(graph_is_read_only());
	control_params.set_read_only(graph_is_read_only());

	if (open_prop_editor)
		draw_prop_editor();

	control_params.imgui_draw();

	is_modifier_pressed = ImGui::GetIO().KeyAlt;


	ImGui::Begin("animation graph editor");

	if (ImGui::GetIO().MouseClickedCount[0] == 2) {

		if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_RootWindow) && ImNodes::NumSelectedNodes() == 1) {
			int node = 0;
			ImNodes::GetSelectedNodes(&node);

			IAgEditorNode* mynode = find_node_from_id(node);
			
			const editor_layer* layer = mynode->get_layer();

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

	{
		auto playimg = mats.find_texture("icon/play.png");
		auto stopimg = mats.find_texture("icon/stop.png");
		auto pauseimg = mats.find_texture("icon/pause.png");
		auto saveimg = mats.find_texture("icon/save.png");

		ImGui::PushStyleColor(ImGuiCol_Button,0);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1,1,1,0.5));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive,0);


		if (get_playback_state() == graph_playback_state::running) {
			if (ImGui::ImageButton((ImTextureID)pauseimg->gl_id, ImVec2(32, 32)))
				pause_playback();
		}
		else {
			if (ImGui::ImageButton((ImTextureID)playimg->gl_id, ImVec2(32, 32)))
				start_or_resume_playback();
		}
		ImGui::SameLine();
		bool is_stopped = get_playback_state() == graph_playback_state::stopped;
		auto greyed_out = ImVec4(1, 1, 1, 0.3);

		ImGui::BeginDisabled(is_stopped);
		if (ImGui::ImageButton((ImTextureID)stopimg->gl_id,
			ImVec2(32, 32),
			ImVec2(0, 0), ImVec2(1, 1), -1, ImVec4(0, 0, 0, 0),
			ImVec4(1, 1, 1, 1)))
		{
			if (!is_stopped)
				stop_playback();
		}
		ImGui::EndDisabled();

		ImGui::SameLine();
		if (ImGui::ImageButton((ImTextureID)saveimg->gl_id,
			ImVec2(32, 32),
			ImVec2(0, 0), ImVec2(1, 1), -1, ImVec4(0, 0, 0, 0),
			ImVec4(1, 1, 1, 1)))
		{
			save_document();
		}
		ImGui::PopStyleColor(3);
	}


	graph_tabs.imgui_draw();

	if (!graph_is_read_only()) {
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
	}

	draw_popups();


	ImGui::End();

}
#include "MyImguiLib.h"
void AnimationGraphEditor::draw_graph_layer(uint32_t layer)
{
	auto strong_error = mats.find_texture("icon/fatalerr.png");
	auto weak_error = mats.find_texture("icon/error.png");
	auto info_img = mats.find_texture("icon/question.png");

	ImNodes::BeginNodeEditor();
	for (auto node : nodes) {

		if (node->graph_layer != layer) continue;


		ImNodes::PushColorStyle(ImNodesCol_TitleBar, color32_to_int(node->node_color));
		Color32 select_color = add_brightness(node->node_color, 30);
		Color32 hover_color = add_brightness(mix_with(node->node_color, { 5, 225, 250 }, 0.6), 5);
		ImNodes::PushColorStyle(ImNodesCol_TitleBarHovered, color32_to_int(hover_color));
		ImNodes::PushColorStyle(ImNodesCol_TitleBarSelected, color32_to_int(select_color));

		// node is selected
		if (node->id == sel.node_last_frame) {
			ImNodes::PushStyleVar(ImNodesStyleVar_NodeBorderThickness, 2.0);
			ImNodes::PushColorStyle(ImNodesCol_NodeOutline, color32_to_int({ 255, 174, 0 }));
		}

		ImNodes::BeginNode(node->id);

		ImNodes::BeginNodeTitleBar();

		if(node->name_is_default())
			ImGui::Text("%s\n", node->get_default_name().c_str());
		else
			ImGui::Text("%s\n", node->get_title().c_str());

		if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort) && ImGui::BeginTooltip()) {
			ImGui::TextUnformatted(get_animnode_typedef(node->type).editor_tooltip);
			ImGui::EndTooltip();
		}


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

		if (!node->compile_info_string.empty()) {
			ImGui::SameLine();
			ImGui::Image((ImTextureID)info_img->gl_id, ImVec2(16, 16));

			if (ImGui::IsItemHovered() && ImGui::BeginTooltip()) {
				ImGui::Text(node->compile_info_string.c_str());
				ImGui::EndTooltip();
			}
		}



		ImNodes::EndNodeTitleBar();

		float x1 = ImGui::GetItemRectMin().x;
		float x2 = ImGui::GetItemRectMax().x;


		ImGui::BeginDisabled(graph_is_read_only());
		node->draw_node_top_bar();
		ImGui::EndDisabled();
		
		MyImSeperator(x1,x2,1.0);

		ImVec4 pin_color = ImGui::GetStyle().Colors[ImGuiCol_Text];
		if (node->has_pin_colors())
			pin_color = node->get_pin_colors();

		for (int j = 0; j < node->num_inputs; j++) {

			ImNodesPinShape pin = ImNodesPinShape_Quad;

			if (node->inputs[j].other_node) pin = ImNodesPinShape_TriangleFilled;
			ImNodes::BeginInputAttribute(node->getinput_id(j), pin);
			auto str = node->get_input_pin_name(j);

			ImGui::TextColored(pin_color, str.c_str());
			ImNodes::EndInputAttribute();
		}
		if (node->has_output_pins()) {
			ImNodes::BeginOutputAttribute(node->getoutput_id(0));
			ImGui::TextColored(pin_color, node->get_output_pin_name().c_str());
			ImNodes::EndOutputAttribute();
		}

		ImNodes::EndNode();

		ImNodes::PopColorStyle();
		ImNodes::PopColorStyle();
		ImNodes::PopColorStyle();

		if (node->id == sel.node_last_frame) {
			ImNodes::PopStyleVar();
			ImNodes::PopColorStyle();
		}

		bool draw_flat_links = node->draw_flat_links();
		for (int j = 0; j < node->num_inputs; j++) {
			if (node->inputs[j].other_node) {

				bool pushed_colors = node->push_imnode_link_colors(j);
	
				ImNodes::Link(node->getlink_id(j), node->inputs[j].other_node->getoutput_id(0), node->getinput_id(j), draw_flat_links);

				if (pushed_colors) {
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

bool AgEditor_StateNode::push_imnode_link_colors(int index)
{
	ASSERT(inputs[index].other_node);
	ASSERT(inputs[index].other_node->is_state_node());
	AgEditor_StateNode* other = (AgEditor_StateNode*)inputs[index].other_node;

	auto st = other->get_state_transition_to(this);
	ASSERT(st);

	if (st->is_a_continue_transition()) {
		ImNodes::PushColorStyle(ImNodesCol_Link, color32_to_int({ 242, 41, 41 }));
		ImNodes::PushColorStyle(ImNodesCol_LinkSelected, color32_to_int({ 245, 211, 211 }));
	}
	else {
		ImNodes::PushColorStyle(ImNodesCol_Link, color32_to_int({ 0,0xff,0 }));
		ImNodes::PushColorStyle(ImNodesCol_LinkSelected, color32_to_int({ 193, 247, 186 }));
	}

	ImNodes::PushColorStyle(ImNodesCol_LinkHovered, color32_to_int({ 0xff,0xff,0xff }));

	return true;
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

		case SDL_SCANCODE_SPACE:
			if (event.key.keysym.mod & KMOD_LCTRL && !ImGui::GetIO().WantCaptureKeyboard) {
				if (get_playback_state() == graph_playback_state::running)
					pause_playback();
				else
					start_or_resume_playback();
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

		std::vector<IAgEditorNode*> stuff;

		for (int i = 0; i < ids.size(); i++) {
			uint32_t nodeid = IAgEditorNode::get_nodeid_from_link_id(ids[i]);
			uint32_t slot = IAgEditorNode::get_slot_from_id(ids[i]);
			IAgEditorNode* node = find_node_from_id(nodeid);
			node->on_remove_pin(slot);
			stuff.push_back(node);
		}
		for(auto node : stuff)
			node->on_post_remove_pins();
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
			const char* name = get_animnode_typedef(type).editor_name;
			if (ImGui::Selectable(name)) {

				int cur_layer = graph_tabs.get_current_layer_from_tab();
				auto parent = ed.get_owning_node_for_layer(cur_layer);
				auto a = create_graph_node_from_type(parent, type, cur_layer);

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

// two paths: create new node
//			  load an existing node from a file

std::string AgEditor_BaseNode::get_input_pin_name(int index)
{
	switch (type)
	{
	case animnode_type::add:
		if (index == Add_Node_CFG::DIFF) return "diff";
		if (index == Add_Node_CFG::BASE) return "base";
		ASSERT(0);
	case animnode_type::subtract:
		if(index  == Subtract_Node_CFG::REF) return "ref";
		if (index == Subtract_Node_CFG::SOURCE) return "source";
		ASSERT(0);
	case animnode_type::root:
		if (index == 0) return "OUTPUT";
		ASSERT(0);
	case animnode_type::blend_by_int:
	{
		auto param = node->param;
		if (!param.is_valid() || ed.control_params.get_parameter_for_ed_id(param.id)->type != script_parameter_type::enum_t)
			return string_format("%d", index);
		return "enum placeholder";
	};


	default:
		return "in";
	}
}

std::string AgEditor_StateNode::get_input_pin_name(int index)
{
	if (!inputs[index].other_node) return {};

	std::string name = inputs[index].other_node->name_is_default() ? inputs[index].other_node->get_default_name() : inputs[index].other_node->title;
	if (name.size() > 16) {
		name.resize(13);
		name.append("...");
	}
	return name;
}

void AgEditor_BaseNode::init()
{
	ASSERT(node);
	node_color = get_animnode_typedef(type).editor_color;

	// find editor input nodes
	ASSERT(node->input.size()<=MAX_INPUTS);

	int init_input_count = 0;
	int max_input_slot = node->input.size();
	for (int i = 0; i < max_input_slot; i++) {

		Node_CFG* node_cfg = node->input[i];
		if (!node_cfg)
			continue;
		init_input_count++;
		IAgEditorNode* ed_node = ed.editor_node_for_cfg_node(node_cfg);

		if (!ed_node) {
			printf("!!! couldn't find editor node for cfg !!! (data read wrong from disk or out of date?)\n");

			ASSERT(0);
			// TODO: create the new editor node
		}

		add_input(&ed, ed_node, i);
	}

	int allowed_inputs = get_animnode_typedef(type).allowed_inputs;

	if (allowed_inputs != -1) {
		if (max_input_slot > allowed_inputs) {
			printf("!!! too many inputs for node !!! (out of date or corrupted?)\n");

			for (int j = allowed_inputs; j < max_input_slot; j++) {
				
				if (node->input[j]) {
					on_remove_pin(j, true);
					on_post_remove_pins();
				}
			}
			node->input.resize(allowed_inputs);
		}
		num_inputs = allowed_inputs;
	}

}



bool AgEditor_BaseNode::compile_my_data()
{
	ASSERT(node);
	
	// update inputs
	int actual_input_count = get_animnode_typedef(type).allowed_inputs;
	ASSERT(actual_input_count == -1 || actual_input_count == num_inputs);

	// read prop list to find parameter usage, pretty cursed
	auto prop_list = node->get_props();
	for (int j = 0; j < prop_list->count; j++) {
		if (strcmp(prop_list->list[j].custom_type_str, "AG_PARAM_FINDER") == 0) {
			ASSERT(prop_list->list[j].type == core_type_id::Int32);
			int ed_id = prop_list->list[j].get_int(node);
			int paramid = ed.control_params.get_index_of_prop_for_compiling(ed_id).id;
			prop_list->list[j].set_int(node, paramid);
		}
	}

	node->init_memory_offsets(ed.editing_tree, num_inputs);

	bool has_null_input = false;
	node->input.resize(num_inputs);
	for (int i = 0; i < num_inputs; i++) {
		has_null_input |= !bool(inputs[i].other_node);
		if (inputs[i].other_node && inputs[i].other_node->get_graph_node()) {
			node->input[i] = inputs[i].other_node->get_graph_node();
		}
	}
	
	// selector nodes can have empty inputs

	if (has_null_input)
		append_info_msg("[INFO] missing inputs\n");

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

		BlendSpace2d_CFG* blend2d = (BlendSpace2d_CFG*)node;
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

void AgEditor_BaseNode::draw_node_top_bar()
{
	bool uses_default_param = node->get_props()->find("param") != nullptr;
	if (uses_default_param) {
		auto param = ed.control_params.get_parameter_for_ed_id(node->param.id);
		if (param) {
			ImGui::TextColored(scriptparamtype_to_color(param->type), string_format(":= %s",param->name.c_str()));
		}
	}

	if (type == animnode_type::blend_by_int) {

		auto node_ = (Blend_Int_Node_CFG*)node;
		auto param = ed.control_params.get_parameter_for_ed_id(node->param.id);
		if (!node->param.is_valid() || (param && param->type == script_parameter_type::int_t)) {

			ImGui::PushStyleColor(ImGuiCol_Button, color32_to_int({ 0xff,0xff,0xff,50 }));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, color32_to_int({ 0xff,0xff,0xff,128 }));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, color32_to_int({ 0xff,0xff,0xff,50 }));


			if (ImGui::SmallButton("Add") && num_inputs < MAX_INPUTS) {
				num_inputs += 1;
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("Remove") && num_inputs > 0) {
				on_remove_pin(num_inputs-1, true);
				on_post_remove_pins();
				num_inputs -= 1;
			}

			ImGui::PopStyleColor(3);
		}

	}

}

void AgEditor_BaseNode::get_props(std::vector<PropertyListInstancePair>& props)
{
	IAgEditorNode::get_props(props);

	props.push_back({ node->get_props(),node });
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
	compile_info_string.clear();

	Statemachine_Node_CFG* sm_cfg = parent_statemachine->node;
	if (type == animnode_type::state) {

		self_state.transition_idxs.resize(0);
	}
	for (int i = 0; i < output.size(); i++) {
		AgEditor_StateNode* out_state = output[i].output_to;
		State_Transition* st = &output[i].st;

		st->transition_state = output[i].output_to->state_handle_internal;

		// compile script

		if (!st->script_uncompilied.empty() && st->is_a_continue_transition()) {
			append_info_msg(string_format("[INFO] is_continue_transition == true, but script is not empty.\n"));
		}
		else if(!st->is_a_continue_transition()) {

			const std::string& code = st->script_uncompilied;
			const char* err_str = nullptr;
			try {

				auto ret = st->script_condition.compile_new(&get_global_anim_bytecode_ctx(), &ed.editing_tree->parameters, code);
				if (strlen(ret) != 1 || ret[0] != 'i')
					err_str = "must return only 1 integer/boolean";

			}
			catch (LispError err) {
				err_str = err.msg;
			}
			catch (...) {
				err_str = "unknown error";

			}

			if (err_str) {
				const char* to_state = out_state->get_title().c_str();
				append_fail_msg(string_format("[ERROR] script (-> %s) compile failed ( %s )\n", to_state, err_str));
			}
		}

		sm_cfg->transitions.push_back(*st);
		int idx = sm_cfg->transitions.size() - 1;
		if (type == animnode_type::state) {
			self_state.transition_idxs.push_back(idx);
		}
		else {// entry state
			sm_cfg->entry_transitions.push_back(idx);
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

			self_state.tree = rootnode->get_graph_node();
		}

		sm_cfg->states.push_back(self_state);
	}

	return compile_error_string.empty();	// empty == no errors generated
	
}

void AgEditor_StateNode::on_remove_pin(int slot, bool force)
{
	ASSERT(inputs[slot].other_node);

	if (inputs[slot].other_node->is_state_node()) {
		((AgEditor_StateNode*)inputs[slot].other_node)->remove_output_to(this);
		inputs[slot].other_node = nullptr;
	}
	else
		ASSERT(!"not state node in state graph");
}

void AgEditor_StateNode::on_post_remove_pins()
{
	ASSERT(num_inputs >= 1);
	ASSERT(inputs[num_inputs-1].other_node == nullptr);
	
	int count = 0;
	for (int i = 0; i < num_inputs - 1; i++) {
		if (inputs[i].other_node) {
			ASSERT(inputs[i].other_node->is_state_node());
			inputs[count++] = inputs[i];
		}
	}
	num_inputs = count + 1;
	ASSERT(num_inputs >= 1 && num_inputs < MAX_INPUTS);
	inputs[num_inputs - 1].other_node = nullptr;



#ifdef _DEBUG
		ASSERT(num_inputs >= 1);
		ASSERT(inputs[num_inputs - 1].other_node == nullptr);
		if (num_inputs > 1) {
			for (int i = 0; i < num_inputs - 1; i++) {
				ASSERT(inputs[i].other_node != nullptr);
			}
		}
#endif // _DEBUG

}

void AgEditor_StateNode::get_props(std::vector<PropertyListInstancePair>& props)
{
	IAgEditorNode::get_props(props);

	props.push_back({ State::get_props(),&self_state });
}

 bool AgEditor_StateNode::add_input(AnimationGraphEditor* ed, IAgEditorNode* input, uint32_t slot) {

	for (int i = 0; i < inputs.size(); i++) {
		if (inputs[i].other_node == input) {
			return true;
		}
	}

	ASSERT(input->is_state_node());

	inputs[slot].other_node = input;

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
	ASSERT(inputs[slot].other_node->is_state_node());

	((AgEditor_StateNode*)inputs[slot].other_node)->get_transition_props(this, props);
}

void AgEditor_StateNode::on_output_create(AgEditor_StateNode* node_to_output)
{
	output.push_back({ node_to_output });
}

void AgEditor_StateNode::remove_output_to(AgEditor_StateNode* node)
{

	bool already_seen = false;
	for (int i = 0; i < output.size(); i++) {

		// WARNING transitions invalidation potentially!
		if (output[i].output_to == node) {

			ASSERT(!already_seen);

			output.erase(output.begin() + i);
			already_seen = true;
			i--;
		}
	}

}

void AgEditor_StateNode::get_transition_props(AgEditor_StateNode* to, std::vector<PropertyListInstancePair>& props)
{
	for (int i = 0; i < output.size(); i++) {
		if (output[i].output_to == to) {
			
			// WARNING: this pointer becomes invalid if output is resized, this shouldnt happen
			props.push_back({ State_Transition::get_props(), &output[i].st });
			return;
		}
	}
	ASSERT(0);
}

void AgEditor_StateMachineNode::get_props(std::vector<PropertyListInstancePair>& props)
{
	IAgEditorNode::get_props(props);

	props.push_back({ node->get_props(),node });
}

void AgEditor_StateNode::init()
{
	node_color = get_animnode_typedef(type).editor_color;

	if (type == animnode_type::start_statemachine)
		num_inputs = 0;
	else
		num_inputs = 1;

	bool good = parent_statemachine->add_node_to_statemachine(this);
	ASSERT(good);

	// read data from statenode to init stuff using the state_handle_internal which gives index into parent statemachine
}

void AgEditor_StateMachineNode::init()
{
	ASSERT(node);
	node_color = get_animnode_typedef(type).editor_color;
	num_inputs = 0;
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
		//parent_statemachine = nullptr;
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

	bool already_erased = false;
	for (int i = 0; i < states.size(); i++) {
		if (states[i] == node) {
			ASSERT(!already_erased);	// just a check
			states.erase(states.begin() + i);
			i--;
			already_erased = true;
		}
	}
}

std::string AgEditor_StateNode::get_default_name()
{
	bool any_non_defaults = false;

	if (type == animnode_type::start_statemachine)
		return get_animnode_typedef(type).editor_name;

	auto startnode = ed.find_first_node_in_layer(sublayer.id, animnode_type::root);

	if (!startnode->inputs[0].other_node || startnode->inputs[0].other_node->type != animnode_type::source)
		return get_animnode_typedef(type).editor_name;

	// get clip name to use as default state name
	return startnode->inputs[0].other_node->get_default_name();
}

std::string AgEditor_StateMachineNode::get_default_name()
{
	bool any_non_defaults = false;
	std::string name;
	for (int i = 0; i < states.size(); i++) {

		// states can be null...
		if (!states[i]) continue;

		if (states[i]->type == animnode_type::start_statemachine)
			continue;
		if (states[i]->name_is_default())
			continue;
		name += states[i]->title;
		name += '/';

		if (name.size() > 22) {
			name += "...";
			return name;
		}
	}
	if (name.empty())
		return get_animnode_typedef(type).editor_name;

	name.pop_back();
	return name;
}

bool AgEditor_StateMachineNode::compile_my_data()
{

	node->states.clear();
	for (int i = 0; i < states.size(); i++) {
		ASSERT(states[i]);
		if (states[i]->type == animnode_type::state) {
			states[i]->state_handle_internal = { i };
		}
	}
	node->entry_transitions.resize(0);
	node->transitions.clear();

	bool has_errors = false;

	for (int i = 0; i < states.size(); i++) {
		has_errors |= !states[i]->compile_data_for_statemachine();
	}

	if (has_errors)
		append_fail_msg("[ERROR] state machine states contain errors\n");


	auto state_enter = (AgEditor_StateNode*)ed.find_first_node_in_layer(sublayer.id, animnode_type::start_statemachine);
	ASSERT(state_enter);	// should never be deleted

	bool found_default_entry = false;
	for (int i = 0; i < node->entry_transitions.size(); i++) {

		auto st = node->transitions[node->entry_transitions[i]];
		if (st.is_a_continue_transition()) {
			if (found_default_entry) {
				append_fail_msg("[ERROR] state machine contains more than one default entry condition");
				break;
			}
			found_default_entry = true;

		}

	}
	if (!found_default_entry) {
		append_fail_msg("[ERROR] state machine does not have a default entry transition");
	}

	return true;

}


State* AgEditor_StateMachineNode::get_state(handle<State> state) {
	ASSERT(state.is_valid() && state.id < node->states.size());
	return &node->states.at(state.id);
}


AgEditor_StateMachineNode* create_statemachine_node(uint32_t id, uint32_t layer)
{
	auto cfg = (Statemachine_Node_CFG*)get_animnode_typedef(animnode_type::statemachine).create(ed.editing_tree);

	AgEditor_StateMachineNode* node = new AgEditor_StateMachineNode(
		cfg, 
		id, 
		layer, 
		animnode_type::statemachine, 
		ed.create_new_layer(true));

	ed.add_root_node_to_layer(node, node->sublayer.id, true);

	return node;
}

AgEditor_StateNode* create_state_node(IAgEditorNode* parent, uint32_t layer, animnode_type type, uint32_t id)
{
	ASSERT(parent);
	ASSERT(parent->is_statemachine());
	ASSERT(parent->get_layer()->id == layer);
	auto parent_sm = (AgEditor_StateMachineNode*)parent;

	editor_layer sublayer{};
	if (type == animnode_type::state)
		sublayer = ed.create_new_layer(false);

	AgEditor_StateNode* state = new AgEditor_StateNode(parent_sm, id, layer, type, sublayer);

	if (type == animnode_type::state)
		ed.add_root_node_to_layer(state, state->sublayer.id, false);

	return state;
}

AgEditor_BaseNode* create_generic_node(animnode_type type, uint32_t id, uint32_t layer)
{
	auto cfg = get_animnode_typedef(type).create(ed.editing_tree);

	if (type == animnode_type::blend1d || type == animnode_type::blend2d)
		return new AgEditor_BlendspaceNode(cfg, id,layer,type);
	else
		return new AgEditor_BaseNode(cfg,id,layer,type);
}


IAgEditorNode* AnimationGraphEditor::create_graph_node_from_type(IAgEditorNode* parent_, animnode_type type, uint32_t layer)
{

	IAgEditorNode* node = nullptr;

	uint32_t id = current_id++;

	if (type == animnode_type::statemachine) {
		node = create_statemachine_node(id,layer);
	}
	else if (type == animnode_type::state || type == animnode_type::start_statemachine) {
		ASSERT(parent_);
		node = create_state_node(parent_, layer, type, id);
	}
	else if (type == animnode_type::blend1d || type == animnode_type::blend2d) {
		node = create_generic_node(type, id, layer);
	}
	else if (type == animnode_type::root) {
		node = new IAgEditorNode(id,layer,type);
	}
	else {
		node = create_generic_node(type, id,layer);
	}

	nodes.push_back(node);

	if (node->get_graph_node())
		editing_tree->all_nodes.push_back(node->get_graph_node());

	node->init();

	return node;
}

bool AnimationGraphEditor::compile_graph_for_playing()
{
	control_params.add_parameters_to_tree(&editing_tree->parameters);

	// initialize memory offets for runtime
	editing_tree->data_used = 0;

	for (int i = 0; i < nodes.size(); i++) {
		if (!nodes[i]->dont_call_compile()) {
			nodes[i]->compile_error_string.clear();
			nodes[i]->compile_info_string.clear();
			nodes[i]->compile_my_data();
		}
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

	control_params.recalculate_control_prop_ids();

	return tree_is_good_to_run;
}

#include "ReflectionRegisterDefines.h"

void IAgEditorNode::init()
{
	node_color = get_animnode_typedef(type).editor_color;

	ASSERT(type == animnode_type::root || type == animnode_type::start_statemachine);

	if (type == animnode_type::root)
		num_inputs = 1;
	if (type == animnode_type::start_statemachine)
		num_inputs = 0;

}

PropertyInfoList* IAgEditorNode::get_prop_list()
{
	START_PROPS(IAgEditorNode)
		REG_STDSTRING( title, PROP_DEFAULT),
		REG_INT( id, PROP_SERIALIZE, ""),
		REG_ENUM( type, PROP_SERIALIZE, "", animnode_type_def.id),
		REG_INT( graph_layer, PROP_SERIALIZE, "")
	END_PROPS(IAgEditorNode)
}

std::string IAgEditorNode::get_default_name()
{
	if (type == animnode_type::source) {

		Clip_Node_CFG* node = (Clip_Node_CFG*)((AgEditor_BaseNode*)this)->node;

		return node->clip_name;
	}

	return get_animnode_typedef(type).editor_name;
}



void IAgEditorNode::remove_reference(IAgEditorNode* node)
{
	for (int i = 0; i < num_inputs; i++) {
		if (inputs[i].other_node == node) {
			on_remove_pin(i, true);

			on_post_remove_pins();
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
	
	if (get_playback_state() == graph_playback_state::running) {
		out.anim.tick_tree_new(dt);
	}
	if (get_playback_state() != graph_playback_state::stopped) {
		ro.transform = out.model->skeleton_root_transform;
		ro.animator = &out.anim;
	}

	idraw->update_obj(out.obj, ro);

	auto window_sz = eng->get_game_viewport_dimensions();
	out.vs = View_Setup(out.camera.position, out.camera.front, glm::radians(70.f), 0.01, 100.0, window_sz.x, window_sz.y);
}

#include "StdVectorReflection.h"
PropertyInfoList* EditorControlParamProp::get_props()
{
	START_PROPS(EditorControlParamProp)
		REG_STDSTRING(name, PROP_EDITABLE),
		REG_ENUM(type, PROP_EDITABLE, "", script_parameter_type_def.id),
		REG_INT_W_CUSTOM(enum_type, PROP_EDITABLE, "", "AG_ENUM_TYPE_FINDER"),
	END_PROPS(EditorControlParamProp)
}

PropertyInfoList* ControlParamsWindow::get_props()
{
	MAKE_VECTORCALLBACK(EditorControlParamProp, props)
	START_PROPS(ControlParamsWindow)
		REG_STDVECTOR_W_CUSTOM(props, PROP_EDITABLE, "AG_CONTROL_PARAM_ARRAY")
	END_PROPS(ControlParams)
}

void ControlParamsWindow::imgui_draw()
{
	if (ImGui::Begin("Control Parameters")) {
		control_params.update();
	}
	ImGui::End();
}


void AnimationGraphEditor::compile_and_run()
{
	bool good_to_run = compile_graph_for_playing();
	if (good_to_run) {
		out.anim.initialize_animator(out.model, out.set, editing_tree, nullptr, nullptr);
		playback = graph_playback_state::running;
	}
}

void AnimationGraphEditor::overlay_draw()
{

}
void AnimationGraphEditor::open(const char* name)
{
	//this->name = name;

	imgui_node_context = ImNodes::CreateContext();

	ImNodes::GetIO().LinkDetachWithModifierClick.Modifier = &is_modifier_pressed;

	editing_tree = new Animation_Tree_CFG;
	editing_tree->arena.init("ATREE ARENA", 1'000'000);	// spam the memory for the editor

	graph_tabs.add_tab(nullptr, nullptr, glm::vec2(0.f), true);
	add_root_node_to_layer(nullptr, 0, false);

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

	control_params.refresh_props();
}

class AgEditor_BlendSpaceArrayHead : public IArrayHeader
{
	using IArrayHeader::IArrayHeader;
	// Inherited via IArrayHeader
	virtual bool imgui_draw_header(int index)
	{
		return false;
	}
	virtual void imgui_draw_closed_body(int index)
	{
	}
};

class AgEdtior_BlendSpaceParameteriation : public IPropertyEditor
{
	using IPropertyEditor::IPropertyEditor;
	// Inherited via IPropertyEditor
	virtual void internal_update() override
	{

		std::vector<ImVec2> verts;
		std::vector<const char*> names;

		std::vector<int> indicies;

		verts.push_back(ImVec2(0.5, 0.5));
		names.push_back("[0]");

		verts.push_back(ImVec2(0, 0));
		names.push_back("[1]");

		verts.push_back(ImVec2(0, 1));
		names.push_back("[2]");

		verts.push_back(ImVec2(1,1));
		names.push_back("[3]");

		verts.push_back(ImVec2(1, 0));
		names.push_back("[4]");

		indicies.push_back(0);
		indicies.push_back(1);
		indicies.push_back(2);

		indicies.push_back(0);
		indicies.push_back(1);
		indicies.push_back(4);



		MyImDrawBlendSpace("##label", verts, indicies, names, ImVec2(0, 0), ImVec2(1, 1), nullptr);

	}
};

void AgEditor_BlendspaceNode::get_props(std::vector<PropertyListInstancePair>& props)
{
	AgEditor_BaseNode::get_props(props);
	props.push_back({ AgEditor_BlendspaceNode::get_props_list(), this });
}

void AgEditor_BlendspaceNode::init()
{
	AgEditor_BaseNode::init();
	num_inputs = 0;	// these are EDITOR inputs, not CFG inputs
	if (type == animnode_type::blend2d) {
		BlendSpace2d_CFG* node = (BlendSpace2d_CFG*)this->node;
		if (node->is_additive_blend_space)
			num_inputs = 1;

		int number_of_inputs_on_input = node->input.size();

		// default on creation to 9 vert blend space because its useful
		if (number_of_inputs_on_input == 0 || number_of_inputs_on_input == 9)
			topology_2d = BlendSpace2dTopology::NineVert;
		else if(number_of_inputs_on_input == 5)
			topology_2d = BlendSpace2dTopology::FiveVert;
		else if (number_of_inputs_on_input == 15)
			topology_2d = BlendSpace2dTopology::FifteenVert;
		else {
			printf("!!! AgEditor_BlendspaceNode got bad input count !!! (%d)", number_of_inputs_on_input);
			node->input.resize(0);
			topology_2d = BlendSpace2dTopology::NineVert;
		}
	}
	else {
		BlendSpace1d_CFG* node = (BlendSpace1d_CFG*)this->node;
		if (node->is_additive_blend_space)
			num_inputs = 1;
	}
}

bool AgEditor_BlendspaceNode::compile_my_data()
{
	return AgEditor_BaseNode::compile_my_data();
}

PropertyInfoList* AgEditor_BlendspaceNode::get_props_list()
{
	MAKE_VECTORCALLBACK(Blendspace_Input, blend_space_inputs)
	START_PROPS(AgEditor_BlendspaceNode)
		REG_ENUM(topology_2d,PROP_EDITABLE,"", BlendSpace2dTopology_def.id),
		REG_STDVECTOR_W_CUSTOM(blend_space_inputs, PROP_EDITABLE, ""),
		REG_INT_W_CUSTOM(parameterization, PROP_EDITABLE, "", "AG_EDITOR_BLEND_SPACE_PARAMETERIZATION"),
	END_PROPS(AgEditor_BlendspaceNode)
}

PropertyInfoList* AgEditor_BlendspaceNode::Blendspace_Input::get_props()
{
	START_PROPS(AgEditor_BlendspaceNode::Blendspace_Input)
		REG_STDSTRING_CUSTOM_TYPE(clip_name, PROP_EDITABLE, "AG_CLIP_TYPE"),
		REG_FLOAT(x,PROP_EDITABLE,""),
		REG_FLOAT(y,PROP_EDITABLE, ""),
	END_PROPS(AgEditor_BlendspaceNode::Blendspace_Input)
}

// Property Editor Factorys

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
		else if (strcmp(prop->custom_type_str, "AG_ENUM_TYPE_FINDER") == 0) {
			return new AgEnumFinder(instance, prop);
		}
		else if (strcmp(prop->custom_type_str, "AG_PARAM_FINDER") == 0) {
			return new AgParamFinder(instance, prop);
		}
		else if (strcmp(prop->custom_type_str, "AG_EDITOR_BLEND_SPACE_PARAMETERIZATION") == 0) {
			return new AgEdtior_BlendSpaceParameteriation(instance, prop);
		}

		return nullptr;
	}

};
static AnimationGraphEditorPropertyFactory g_AnimationGraphEditorPropertyFactory;


class AnimationGraphEditorArrayHeaderFactory : public IArrayHeaderFactory
{
public:

	// Inherited via IPropertyEditorFactory
	virtual IArrayHeader* try_create(PropertyInfo* prop, void* instance) override
	{
		if (strcmp(prop->custom_type_str, "AG_CONTROL_PARAM_ARRAY") == 0) {
			return new ControlParamArrayHeader(instance, prop);
		}
		else if (strcmp(prop->custom_type_str, "AG_EDITOR_BLEND_SPACE") == 0) {
			return new AgEditor_BlendSpaceArrayHead(instance, prop);
		}


		return nullptr;
	}

};
static AnimationGraphEditorArrayHeaderFactory g_AnimationGraphEditorArrayHeaderFactory;

static const char* strs[] = {
	"FiveVert",
	"NineVert",
	"FifteenVert",
};
AutoEnumDef BlendSpace2dTopology_def = AutoEnumDef("blend2d", 3, strs);