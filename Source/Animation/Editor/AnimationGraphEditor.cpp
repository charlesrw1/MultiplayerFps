#include "AnimationGraphEditor.h"
#include <cstdio>
#include "imnodes.h"
#include "glm/glm.hpp"

#include "Texture.h"
#include "Game_Engine.h"

#include "Framework/EnumDefReflection.h"
#include "Framework/DictWriter.h"
#include "Framework/ReflectionRegisterDefines.h"
#include "Framework/StdVectorReflection.h"
#include "Framework/WriteObject.h"
#include "Framework/MyImguiLib.h"
#include "Framework/Files.h"

#include <fstream>

#include "State_node.h"
#include "Statemachine_node.h"



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
IEditorTool* g_anim_ed_graph = &ed;
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


ImVec4 scriptparamtype_to_color(control_param_type type)
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

		if (prop_.is_virtual_param) {
			ImGui::SameLine();
			ImGui::TextColored(color32_to_imvec4({ 146, 71, 237 }), "Virtual");
		}
		else
			prop_.virtual_param_code.clear();


		return open;
	}
	virtual void imgui_draw_closed_body(int index)
	{
		using proptype = EditorControlParamProp;
		std::vector<proptype>* array_ = (std::vector<proptype>*)prop->get_ptr(instance);
		ASSERT(index >= 0 && index < array_->size());
		proptype& prop_ = array_->at(index);

		ImGui::PushStyleColor(ImGuiCol_Text, color32_to_imvec4({ 153, 152, 156 }));
		const char* name = Enum::get_enum_name(control_param_type_def.id, (int)prop_.type);
		ImGui::Text("%s", name);
		ImGui::PopStyleColor();
	}

	virtual bool can_edit_array() override {
		return !ed.graph_is_read_only();
	}

	friend class ControlParamsWindow;
};


void ControlParamsWindow::refresh_props() {
	control_params.clear_all();
	if (ed.get_playback_state() == AnimationGraphEditor::graph_playback_state::stopped)
		control_params.add_property_list_to_grid(get_props(), this, PG_LIST_PASSTHROUGH);
	else
		control_params.add_property_list_to_grid(get_edit_value_props(), this, PG_LIST_PASSTHROUGH);
}

void AnimationGraphEditor::init()
{
	ASSERT(!is_initialized);

	imgui_node_context = ImNodes::CreateContext();
	ImNodes::GetIO().LinkDetachWithModifierClick.Modifier = &is_modifier_pressed;

	// init template nodes for creation menu
	template_creation_nodes.clear();
	auto& ed_factory = get_tool_node_factory();
	for (auto& obj : ed_factory.get_object_creator()) {
		template_creation_nodes.push_back(obj.second());
	}

	is_initialized = true;
}

void AnimationGraphEditor::close()
{
	if (!has_document_open())
		return;
	
	// fixme:
	save_document();

	if(out.obj.is_valid())
		idraw->remove_obj(out.obj);

	for (int i = 0; i < nodes.size(); i++) {
		delete nodes[i];
	}
	nodes.clear();

	if (is_owning_editing_tree) {
		delete editing_tree;
	}
	out.reset_animator();
	editing_tree = nullptr;
	name = "";
	out.model = nullptr;

	open_open_popup = false;
	open_save_popup = false;
	reset_prop_editor_next_tick = false;
	playback = graph_playback_state::stopped;
	sel = selection_state();
	drop_state = create_from_drop_state();
	current_id = 0;
	current_layer = 1;

	opt = settings();
	graph_tabs = TabState(this);
	node_props.clear_all();
	control_params.clear_all();

	ImNodes::EditorContextFree(default_editor);
	
	ASSERT(!has_document_open());
}

static std::string saved_settings = "";

DECLARE_ENGINE_CMD(dump_imgui_ini)
{
	if (args.size() != 2) {
		sys_print("usage: dump_imgui_ini <file>");
		return;
	}
	ImGui::SaveIniSettingsToDisk(args.at(1));
}
DECLARE_ENGINE_CMD(load_imgui_ini)
{
	if (args.size() != 2) {
		sys_print("usage: load_imgui_ini <file>");
		return;
	}
	ImGui::LoadIniSettingsFromDisk(args.at(1));
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
			ImGui::PushID(tabs[n].owner_node);
			if (ImGui::BeginTabItem(string_format("%s###",tabs[n].tabname.c_str()), open_bool, flags))
			{
				bool this_is_an_old_active_tab_or_just_skip = n != active_tab && active_tab_dirty;

				if (this_is_an_old_active_tab_or_just_skip) {
					ImGui::EndTabItem();
					ImGui::PopID();
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
			ImGui::PopID();
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


		uint32_t node_id = Base_EdNode::get_nodeid_from_link_id(link);
		uint32_t slot = Base_EdNode::get_slot_from_id(link);
		Base_EdNode* node_s = ed.find_node_from_id(node_id);
		if (node_s->is_state_node()) {

			auto state = dynamic_cast<State_EdNode*>(node_s);
			ASSERT(state);

			if (state->inputs[slot]) {
				ImGui::BeginTooltip();
				ASSERT(state->inputs[slot]->is_state_node());
				auto other_node = dynamic_cast<State_EdNode*>(state->inputs[slot]);
				ASSERT(other_node);
				// should always be true
				auto other_name = other_node->get_title();
				auto my_name = state->get_title();

				ImGui::Text("%s ->\n%s", other_name.c_str(), my_name.c_str());
				ImGui::Separator();

				auto st = other_node->get_state_transition_to(state, slot);
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

bool AnimationGraphEditor::save_document()
{
	if (playback == graph_playback_state::running) {
		sys_print("!!! cant save graph while playing\n");
		return false;
	}
	if (!current_document_has_path()) {
		open_save_popup = true;
		return false;
	}

	// first compile, compiling writes editor node data out to the CFG node
	bool good = compile_graph_for_playing();

	DictWriter write;
	write.set_should_add_indents(false);

	editing_tree->write_to_dict(write);
	save_editor_nodes(write);

	std::ofstream outfile(name);
	outfile.write(write.get_output().c_str(), write.get_output().size());
	outfile.close();

	return true;
}


struct getter_ednode
{
	static void get(std::vector<PropertyListInstancePair>& props, Base_EdNode* node) {
		node->add_props(props);
	}
};


bool AnimationGraphEditor::load_editor_nodes(DictParser& in)
{
	AgSerializeContext context(get_tree());
	TypedVoidPtr userptr(NAME("AgSerializeContext"), &context);

	if (!in.expect_string("editor") || !in.expect_item_start())
		return false;
	{
		if (!in.expect_string("rootstate") || !in.expect_item_start())
			return false;
		auto out = read_properties(*get_props(), this, in, {}, userptr);
		if (!out.second || !in.check_item_end(out.first))
			return false;
	}

	{
		if (!in.expect_string("nodes") || !in.expect_list_start())
			return false;
		bool good = in.read_list_and_apply_functor([&](StringView view) -> bool {
			Base_EdNode* node = read_object_properties<Base_EdNode, getter_ednode>(get_tool_node_factory(), userptr, in, view);
			if (node) {
				nodes.push_back(node);
				return true;
			}
			return false;
			});
		if (!good)
			return false;
	}

	if (!in.expect_item_end())
		return false;

	return true;
}

void AnimationGraphEditor::save_editor_nodes(DictWriter& out)
{
	AgSerializeContext context(get_tree());
	TypedVoidPtr userptr(NAME("AgSerializeContext"), &context);

	out.write_value("editor");
	out.write_item_start();

	out.write_key("rootstate");
	out.write_item_start();
	write_properties(*get_props(), this, out, {});
	out.write_item_end();

	out.write_key_list_start("nodes");
	for (int i = 0; i < nodes.size(); i++) {
		Base_EdNode* node = nodes[i];
		write_object_properties<Base_EdNode, getter_ednode>(node, userptr, out);
	}
	out.write_list_end();

	out.write_item_end();
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
				open("");
			}
			if (ImGui::MenuItem("Open", "Ctrl+O")) {
				open_open_popup = true;

			}
			if (ImGui::MenuItem("Save", "Ctrl+S")) {
				open_save_popup = true;
			}

			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("View")) {
			ImGui::Checkbox("Graph", &opt.open_graph);
			ImGui::Checkbox("Control params", &opt.open_control_params);
			ImGui::Checkbox("Viewport", &opt.open_viewport);
			ImGui::Checkbox("Property Ed", &opt.open_prop_editor);
			ImGui::EndMenu();

		}

		if (ImGui::BeginMenu("Settings")) {
			ImGui::Checkbox("Statemachine passthrough", &opt.statemachine_passthrough);
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

	control_params.refresh_props();
}
void AnimationGraphEditor::draw_prop_editor()
{
	if (ImGui::Begin("animation graph property editor"))
	{
		
		if (ImNodes::NumSelectedNodes() == 1) {
			int node = 0;
			ImNodes::GetSelectedNodes(&node);

			Base_EdNode* mynode = find_node_from_id(node);

			if (node != sel.node_last_frame || reset_prop_editor_next_tick) {
				sel.link_last_frame = -1;

				node_props.clear_all();

				std::vector<PropertyListInstancePair> info;
				mynode->add_props(info);
				mynode->add_props_for_editable_element(info);

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

			uint32_t node_id = Base_EdNode::get_nodeid_from_link_id(link);
			uint32_t slot = Base_EdNode::get_slot_from_id(link);
			Base_EdNode* node_s = find_node_from_id(node_id);

			if (!node_s->is_state_node()) {
				sel.link_last_frame = -1;
				sel.node_last_frame = -1;
			}
			else {
				if (link != sel.link_last_frame || reset_prop_editor_next_tick) {
					sel.node_last_frame = -1;

					node_props.clear_all();

					std::vector<PropertyListInstancePair> info;
					node_s->get_link_props(info, slot);

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
		reset_prop_editor_next_tick = false;

	}
	ImGui::End();
}

void AnimationGraphEditor::stop_playback()
{
	playback = graph_playback_state::stopped;

	control_params.refresh_props();
}

template<typename FUNCTOR>
static void open_or_save_file_dialog(FUNCTOR&& callback, const bool is_save_dialog)
{
	static bool alread_exists = false;
	static bool cant_open_path = false;
	static char buffer[256];
	static bool init = true;
	if (init) {
		buffer[0] = 0;
		alread_exists= false;
		cant_open_path = false;
		init = false;
	}
	bool write_out = false;

	bool returned_true = false;
	if (!alread_exists || !is_save_dialog) {
		ImGui::Text("Enter path: ");
		ImGui::SameLine();
		returned_true = ImGui::InputText("##pathinput", buffer, 256, ImGuiInputTextFlags_EnterReturnsTrue);
	}

	if (returned_true) {
		const char* full_path = string_format("./Data/Animations/Graphs/%s", buffer);
		bool file_already_exists = FileSys::does_os_file_exist(full_path);
		cant_open_path = false;
		alread_exists = false;

		if (is_save_dialog) {

			if (file_already_exists)
				alread_exists = true;
			else {
				std::ofstream test_open(full_path);
				if (!test_open)
					cant_open_path = true;
			}
			if (!alread_exists && !cant_open_path) {
				write_out = true;
			}
		}
		else {

			if (file_already_exists)
				write_out = true;
			else
				cant_open_path = true;

		}
	}
	if (alread_exists) {

		if (is_save_dialog) {

			ImGui::Text("File already exists. Overwrite?");
			if (ImGui::Button("Yes")) {
				write_out = true;
			}
			ImGui::SameLine();
			if (ImGui::Button("No")) {
				alread_exists = false;
			}
		}
		else {
			// open file dialog
			write_out = true;
		}
	}
	else if (cant_open_path) {
		ImGui::Text("Cant open path\n");
	}
	ImGui::Separator();
	if (ImGui::Button("Cancel")) {
		init = true;
		ImGui::CloseCurrentPopup();
	}

	if (write_out) {
		init = true;
		ImGui::CloseCurrentPopup();
		callback(buffer);
	}

	ImGui::EndPopup();
}

void AnimationGraphEditor::draw_popups()
{
	if (open_open_popup) {
		ImGui::OpenPopup("Open file dialog");
		open_open_popup = false;
	}
	if (open_save_popup) {
		ImGui::OpenPopup("Save file dialog");
		open_save_popup = false;
	}

	if (ImGui::BeginPopupModal("Save file dialog")) {
		ImGui::TextColored(ImVec4(0.5, 0.5, 0.5, 1), "Graphs are saved under /Data/Animations/Graphs/");
		open_or_save_file_dialog([&](const char* buf) {
			name = buf;
			save_document();
		}, true);
	}

	if (ImGui::BeginPopupModal("Open file dialog")) {

		ImGui::TextColored(ImVec4(0.5, 0.5, 0.5, 1), "Graphs are searched in $WorkingDir/Data/Animations/Graphs");
		open_or_save_file_dialog([&](const char* buf) {
			open(buf);
		},false);
	}
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
#if 0
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
#endif
void AnimationGraphEditor::handle_imnode_creations(bool* open_popup_menu_from_drop)
{
	int start_atr = 0;
	int end_atr = 0;
	int link_id = 0;

	if (ImNodes::IsLinkCreated(&start_atr, &end_atr))
	{
		if (start_atr >= INPUT_START && start_atr < OUTPUT_START)
			std::swap(start_atr, end_atr);

		uint32_t start_node_id = Base_EdNode::get_nodeid_from_output_id(start_atr);
		uint32_t end_node_id = Base_EdNode::get_nodeid_from_input_id(end_atr);
		uint32_t start_idx = Base_EdNode::get_slot_from_id(start_atr);
		uint32_t end_idx = Base_EdNode::get_slot_from_id(end_atr);

		ASSERT(start_idx == 0);

		Base_EdNode* node_s = find_node_from_id(start_node_id);
		Base_EdNode* node_e = find_node_from_id(end_node_id);

		bool destroy = node_e->add_input(this, node_s, end_idx);
	}
	if (ImNodes::IsLinkDropped(&start_atr)) {
		*open_popup_menu_from_drop = true;

		bool is_input = start_atr >= INPUT_START && start_atr < OUTPUT_START;
		uint32_t id = 0;
		if (is_input)
			id = Base_EdNode::get_nodeid_from_input_id(start_atr);
		else
			id = Base_EdNode::get_nodeid_from_output_id(start_atr);
		drop_state.from = find_node_from_id(id);
		drop_state.from_is_input = is_input;
		drop_state.slot = Base_EdNode::get_slot_from_id(start_atr);
	}
	if (ImNodes::IsLinkDestroyed(&link_id)) {

		uint32_t node_id = Base_EdNode::get_nodeid_from_link_id(link_id);
		uint32_t slot = Base_EdNode::get_slot_from_id(link_id);
		Base_EdNode* node_s = find_node_from_id(node_id);

		node_s->on_remove_pin(slot);

		node_s->on_post_remove_pins();
	}


}

void draw_curve_test()
{
	return;

	static ImVec2 points[20];
	static int init = true;
	static int selected = 0;
	if (init) {
		for (int i = 0; i < 20; i++)
			points[i].x = 1000.0;
		init = false;
	}
	ImGui::Curve("test", ImVec2(300, 200), 10, points, &selected);
}

void AnimationGraphEditor::draw_frame()
{
	// draws into a viewport image that is later sampled when drawing gui
	auto vs = get_vs();
	idraw->scene_draw(vs, this);
}

void AnimationGraphEditor::imgui_draw()
{
	dock_over_viewport(ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

	node_props.set_read_only(graph_is_read_only());

	if (opt.open_prop_editor)
		draw_prop_editor();

	control_params.imgui_draw();


	is_modifier_pressed = ImGui::GetIO().KeyAlt;


	ImGui::Begin("animation graph editor");
	draw_curve_test();

	if (ImGui::GetIO().MouseClickedCount[0] == 2) {

		if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_RootWindow) && ImNodes::NumSelectedNodes() == 1) {
			int node = 0;
			ImNodes::GetSelectedNodes(&node);

			Base_EdNode* mynode = find_node_from_id(node);
			
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

		ImGui::BeginDisabled(!is_stopped);
		ImGui::SameLine();
		if (ImGui::ImageButton((ImTextureID)saveimg->gl_id,
			ImVec2(32, 32),
			ImVec2(0, 0), ImVec2(1, 1), -1, ImVec4(0, 0, 0, 0),
			ImVec4(1, 1, 1, 1)))
		{
			save_document();
		}
		ImGui::PopStyleColor(3);
		ImGui::EndDisabled();
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

void AnimationGraphEditor::draw_graph_layer(uint32_t layer)
{
	auto strong_error = mats.find_texture("icon/fatalerr.png");
	auto weak_error = mats.find_texture("icon/error.png");
	auto info_img = mats.find_texture("icon/question.png");

	ImNodes::BeginNodeEditor();
	for (auto node : nodes) {

		if (node->graph_layer != layer) continue;


		Color32 nodecolor = node->get_node_color();

		ImNodes::PushColorStyle(ImNodesCol_TitleBar,nodecolor.to_uint());
		Color32 select_color = add_brightness(nodecolor, 30);
		Color32 hover_color = add_brightness(mix_with(nodecolor, { 5, 225, 250 }, 0.6), 5);
		ImNodes::PushColorStyle(ImNodesCol_TitleBarHovered, hover_color.to_uint());
		ImNodes::PushColorStyle(ImNodesCol_TitleBarSelected, select_color.to_uint());

		// node is selected
		if (node->id == sel.node_last_frame) {
			ImNodes::PushStyleVar(ImNodesStyleVar_NodeBorderThickness, 2.0);
			ImNodes::PushColorStyle(ImNodesCol_NodeOutline, color32_to_int({ 255, 174, 0 }));
		}

		ImNodes::BeginNode(node->id);

		ImNodes::BeginNodeTitleBar();

		ImGui::Text("%s\n", node->get_title().c_str());

		if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort) && ImGui::BeginTooltip()) {
			ImGui::TextUnformatted(node->get_tooltip().c_str());
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

		int num_inputs = node->get_num_inputs();
		for (int j = 0; j < num_inputs; j++) {

			ImNodesPinShape pin = ImNodesPinShape_Quad;

			if (node->inputs[j]) pin = ImNodesPinShape_TriangleFilled;
			ImNodes::BeginInputAttribute(node->getinput_id(j), pin);
			auto str = node->get_input_pin_name(j);

			ImGui::TextColored(pin_color, str.c_str());
			ImNodes::EndInputAttribute();
		}
		if (node->has_output_pin()) {
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
		for (int j = 0; j < num_inputs; j++) {
			if (node->inputs[j]) {

				int offset = 0;
				if (draw_flat_links) {
					for (int k = 0; k < j; k++) {
						if (node->inputs[k] == node->inputs[j])
							offset++;
					}
				}


				bool pushed_colors = node->push_imnode_link_colors(j);
	
				ImNodes::Link(node->getlink_id(j), node->inputs[j]->getoutput_id(0), node->getinput_id(j), draw_flat_links, offset);

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


bool AnimationGraphEditor::handle_event(const SDL_Event& event)
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
	return false;
}

void AnimationGraphEditor::delete_selected()
{
	if (graph_is_read_only())
		return;

	std::vector<int> ids;
	ids.resize(ImNodes::NumSelectedLinks());
	if (ids.size() > 0) {
		ImNodes::GetSelectedLinks(ids.data());

		std::vector<Base_EdNode*> stuff;

		for (int i = 0; i < ids.size(); i++) {
			uint32_t nodeid = Base_EdNode::get_nodeid_from_link_id(ids[i]);
			uint32_t slot = Base_EdNode::get_slot_from_id(ids[i]);
			Base_EdNode* node = find_node_from_id(nodeid);
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

	if (!node->can_delete() && !force)
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




Base_EdNode* AnimationGraphEditor::user_create_new_graphnode(const char* typename_, uint32_t layer)
{
	auto& factory = get_tool_node_factory();
	if (!factory.hasClass(typename_)) {
		printf("factory doesnt have node for typename %s\n", typename_);
		return nullptr;
	}
	Base_EdNode* node = factory.createObject(typename_);

	node->post_construct(current_id++, layer);
	nodes.push_back(node);
	node->init();

	return node;
}

void AnimationGraphEditor::draw_node_creation_menu(bool is_state_mode)
{
	int count = template_creation_nodes.size();
	for (int i = 0; i < count; i++) {
		const Base_EdNode* node = template_creation_nodes[i];

		if (node->is_state_node() != is_state_mode)
			continue;

		if (!node->allow_creation_from_menu())
			continue;

		const std::string& name = node->get_name();

		if (ImGui::Selectable(name.c_str())) {

			int cur_layer = graph_tabs.get_current_layer_from_tab();

			Base_EdNode* a  = user_create_new_graphnode(node->get_typeinfo().name, cur_layer);

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


static void delete_cfg_node(Node_CFG* node)
{
	ASSERT(!ed.graph_is_read_only());

	// erase node from tree
	int index = 0;
	auto& all_nodes = ed.editing_tree->all_nodes;
	for (int; index < all_nodes.size(); index++) {
		if (all_nodes[index] == node)
			break;
	}
	ASSERT(index != all_nodes.size());
	all_nodes.erase(all_nodes.begin() + index);

	// call destructor on node, since its allocated in an arena, ie no deletes
	node->~Node_CFG();
}

#if 0
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
		node->input[i] = nullptr;
		has_null_input |= !bool(inputs[i].other_node);
		if (inputs[i].other_node && inputs[i].other_node->get_graph_node()) {
			node->input[i] = inputs[i].other_node->get_graph_node();
		}
	}
	
	// selector nodes can have empty inputs

	if (has_null_input)
		append_fail_msg("[ERROR] missing inputs\n");

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

#endif

#if 0
void AgEditor_BaseNode::get_props(std::vector<PropertyListInstancePair>& props)
{
	Base_EdNode::get_props(props);

	props.push_back({ node->get_props(),node });
}

void AgEditor_StateNode::get_props(std::vector<PropertyListInstancePair>& props)
{
	Base_EdNode::get_props(props);

	props.push_back({ State::get_props(),&self_state });
}
#endif




bool AnimationGraphEditor::compile_graph_for_playing()
{
	// removed unreferenced graph nodes
	{
		auto tree = get_tree();

		std::unordered_set<Node_CFG*> refed_nodes;

		std::vector<Node_CFG*> extra_nodes;
		for (int i = 0; i < nodes.size(); i++) {
			refed_nodes.insert(nodes[i]->get_graph_node());
			nodes[i]->get_any_extra_refed_graph_nodes(extra_nodes);
		}
		for (auto node : extra_nodes)
			refed_nodes.insert(node);

		int num_deleted = 0;
		int count = 0;
		std::unordered_set<Node_CFG*> removed_nodes;
		for (int i = 0; i < tree->all_nodes.size(); i++) {
			auto cfg_node = tree->all_nodes[i];
			if (refed_nodes.find(cfg_node)==refed_nodes.end()) {
				// guard against double deletes
				ASSERT(removed_nodes.find(cfg_node) == removed_nodes.end());
				removed_nodes.insert(cfg_node);
				num_deleted++;
				delete cfg_node;
			}
			else {
				tree->all_nodes[count++] = cfg_node;
			}
		}
		tree->all_nodes.resize(count);
		printf("Deleted %d unreferenced nodes\n", num_deleted);
	}

	// add control parameters to cfg list
	control_params.add_parameters_to_tree(editing_tree->params.get());

	// add cfg vars to library
	// link var lib and other libs to script program before compiling all the scripts
	// called again when calling post_init_load()
	editing_tree->init_program_libs();

	// initialize memory offets for runtime
	editing_tree->data_used = 0;

	// to get access to ptr->index hashmap
	AgSerializeContext ctx(get_tree());

	// compile all nodes
	for (int i = 0; i < nodes.size(); i++) {
		if (!nodes[i]->dont_call_compile()) {
			nodes[i]->compile_error_string.clear();
			nodes[i]->compile_info_string.clear();
			nodes[i]->compile_my_data(&ctx);
		}
	}

	Base_EdNode* output_pose = find_first_node_in_layer(0, "Root_EdNode");
	ASSERT(output_pose);
	output_pose->compile_error_string.clear();	//???
	Node_CFG* root_node = nullptr;
	if (output_pose->inputs[0]) {
		root_node = output_pose->inputs[0]->get_graph_node();
		ASSERT(root_node);
	}
	else {
		root_node = nullptr;
		output_pose->append_fail_msg("[ERROR] no output pose\n");
	}
	editing_tree->root = ptr_to_serialized_nodecfg_ptr(root_node, &ctx);


	bool tree_is_good_to_run = output_pose->traverse_and_find_errors();

	// after all updates have been run, fixup editor id/indexes for control props
	control_params.recalculate_control_prop_ids();

	editing_tree->graph_is_valid = tree_is_good_to_run;

	return tree_is_good_to_run;
}

#include <algorithm>



std::vector<const char*>* anim_completion_callback_function(void* user, const char* word_start, int len)
{
	AnimCompletionCallbackUserData* auser = (AnimCompletionCallbackUserData*)user;

	static std::vector<const char*> vec;
	vec.clear();

	auto ed = auser->ed;

	if (auser->type == AnimCompletionCallbackUserData::CLIPS) {
		auto& clips = ed->out.model->get_skel()->get_clips_hashmap();
		for (const auto& c : clips)
			if (_strnicmp(c.first.c_str(), word_start, len) == 0)
				vec.push_back(c.first.c_str());
		std::sort(vec.begin(), vec.end(), [](const char* a, const char* b) -> bool {
			return strcmp(a, b) < 0;
			});
		
	}
	else if (auser->type == AnimCompletionCallbackUserData::BONES) {
		assert(0);
		//auto& bones = ed->out.
		//for (int i = 0; i < bones.size(); i++)
		//	if (_strnicmp(bones[i].name.c_str(), word_start, len) == 0)
		//		vec.push_back(bones[i].name.c_str());

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

void AnimationGraphEditor::on_change_focus(editor_focus_state newstate)
{
	if (newstate == editor_focus_state::Background) {
		stop_playback();
		compile_and_run();
		control_params.refresh_props();
		if (out.obj.is_valid())
			idraw->remove_obj(out.obj);
		playback = graph_playback_state::running;
		Cmd_Manager::get()->execute(Cmd_Execute_Mode::NOW, "dump_imgui_ini animdock.ini");
	}
	else if(newstate == editor_focus_state::Closed){
		close();
		if (out.obj.is_valid())
			idraw->remove_obj(out.obj);
		Cmd_Manager::get()->execute(Cmd_Execute_Mode::NOW, "dump_imgui_ini animdock.ini");
	}
	else {
		// focused, stuff can start being rendered
		playback = graph_playback_state::stopped;
		control_params.refresh_props();
		Cmd_Manager::get()->execute(Cmd_Execute_Mode::NOW, "load_imgui_ini animdock.ini");
	}
}

void AnimationGraphEditor::tick(float dt)
{
	assert(get_focus_state() != editor_focus_state::Closed);

	if(get_focus_state()==editor_focus_state::Focused)
	{
		assert(eng->get_state() != Engine_State::Game);

		int x = 0, y = 0;
		if (eng->game_focused) {
			SDL_GetRelativeMouseState(&x, &y);
			out.camera.update_from_input(eng->keys, x, y, glm::mat4(1.f));
		}

		if (!out.obj.is_valid())
			out.obj = idraw->register_obj();

		Render_Object ro;

		ro.model = out.model;

		if (get_playback_state() == graph_playback_state::running) {
			out.get_local_animator().tick_tree_new(dt * g_slomo.get_float());
		}
		if (get_playback_state() != graph_playback_state::stopped && ro.model->get_skel()) {
			ro.transform = out.model->get_root_transform();
			ro.animator = &out.get_local_animator();
		}
		//ro.animator = nullptr;

		idraw->update_obj(out.obj, ro);

		auto window_sz = eng->get_game_viewport_dimensions();
		out.vs = View_Setup(out.camera.position, out.camera.front, glm::radians(70.f), 0.01, 100.0, window_sz.x, window_sz.y);
	}
	else {	// not focused, game running likely
		playback = graph_playback_state::running;
	}

}

#include "Framework/StdVectorReflection.h"
PropertyInfoList* EditorControlParamProp::get_props()
{
	START_PROPS(EditorControlParamProp)
		REG_STDSTRING(name, PROP_EDITABLE),
		REG_ENUM(type, PROP_EDITABLE, "", control_param_type_def.id),
		REG_INT_W_CUSTOM(enum_type, PROP_SERIALIZE, "-1", "AG_ENUM_TYPE_FINDER"),
		REG_BOOL(is_virtual_param, PROP_EDITABLE, "0"),
		REG_STDSTRING_CUSTOM_TYPE(virtual_param_code, PROP_EDITABLE, "AG_LISP_CODE"),
	END_PROPS(EditorControlParamProp)
}

PropertyInfoList* EditorControlParamProp::get_ed_control_null_prop()
{
	START_PROPS(EditorControlParamProp)
		REG_STRUCT_CUSTOM_TYPE(name, PROP_EDITABLE, "AG_CONTROL_PARAM_RUN_EDIT")	// custom struct type for editing
	END_PROPS(EditorControlParamProp)
}

PropertyInfoList* ControlParamsWindow::get_edit_value_props()
{
	static StdVectorCallback<EditorControlParamProp> vecdef_props(EditorControlParamProp::get_ed_control_null_prop());
	START_PROPS(ControlParamsWindow)
		REG_STDVECTOR_W_CUSTOM(props, PROP_EDITABLE, "AG_CONTROL_PARAM_ARRAY_RUN_EDIT")
	END_PROPS(ControlParams-Editing)
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
	get_tree()->post_load_init();
	if (good_to_run) {
		out.get_local_animator().initialize_animator(out.model, get_tree(), nullptr, nullptr);
		playback = graph_playback_state::running;
	}
}

void AnimationGraphEditor::overlay_draw()
{

}


void AnimationGraphEditor::create_new_document()
{
	printf("creating new document");
	this->name = "";	// when saving, user is prompted
	editing_tree = new Animation_Tree_CFG;
	is_owning_editing_tree = true;
	// add the output pose node to the root layer
	add_root_node_to_layer(nullptr, 0, false);
	// create context for root layer
	default_editor = ImNodes::EditorContextCreate();
	ImNodes::EditorContextSet(default_editor);
}

void AnimationGraphEditor::try_load_preview_models()
{
	{
		Material* mymat = mats.create_temp_shader("sprite_texture");
		mymat->billboard = billboard_setting::SCREENSPACE;
		mymat->images[0] = mats.find_texture("icon/light.png");
		mymat->blend = blend_state::BLEND;
		mymat->type = material_type::UNLIT;
		mymat->diffuse_tint = glm::vec4(1.0);
	
		auto handle= idraw->register_obj();
		Render_Object obj;
		obj.model = mods.get_sprite_model();
		obj.transform = glm::scale(glm::translate(glm::mat4(1.0),glm::vec3(0, 2.0, 0.0)),glm::vec3(0.08,0.08,1.0));
		obj.mat_override = mymat;
		obj.visible = true;


		idraw->update_obj(handle, obj);

		handle = idraw->register_obj();
		obj=Render_Object();
		obj.model = mods.get_sprite_model();
		obj.transform = glm::scale(glm::translate(glm::mat4(1.0), glm::vec3(1.0, 2.0, 0.0)), glm::vec3(0.08, 0.08, 1.0));
		obj.mat_override = mymat;
		obj.visible = true;


		idraw->update_obj(handle, obj);
	}

	out.model = mods.find_or_load("player_FINAL.cmdl");
	if(out.is_valid_for_preview())
		out.get_local_animator().initialize_animator(out.model, get_tree(), nullptr, nullptr);
}

void AnimationGraphEditor::open(const char* name)
{
	assert(get_focus_state() != editor_focus_state::Closed);	// must have opened

	if (!is_initialized)
		init();

	if (get_focus_state() == editor_focus_state::Background) {
		sys_print("!!! cant open Animation Editor while game is running. Close the level then try again.\n");
		return;
	}

	// close currently open document
	close();

	ASSERT(!has_document_open());

	bool needs_new_doc = true;
	if (strlen(name)!=0) {
		// try loading graphname, create new document on fail
		DictParser parser;

		double start = GetTime();
		editing_tree = anim_tree_man->load_animation_tree_file(name, parser);
		printf("loaded in %f\n", GetTime() - start);
		if (editing_tree) {

			bool good = load_editor_nodes(parser);
			if (!good) {
				editing_tree = nullptr;
				sys_print("!!! couldn't load editor nodes for tree %s\n", name);
			}
			else {
				needs_new_doc = false;
				is_owning_editing_tree = false;
				this->name = name;

				for (int i = 0; i < nodes.size(); i++) {
					nodes[i]->init();
				}

			}
		}
		if(needs_new_doc)
			sys_print("!!! Couldn't open animation tree file %s, creating new document instead\n", name);
	}

	if (needs_new_doc) {
		this->name = "";	// empty name
		create_new_document();
		ASSERT(!current_document_has_path());
	}


	{
		const char* window_name = "unnamed";
		if (!needs_new_doc)
			window_name = this->name.c_str();
		SDL_SetWindowTitle(eng->window, string_format("AnimationEditor: %s", window_name));
	}

	ASSERT(editing_tree);

	// initialize other state
	graph_tabs = TabState(this);
	// push root tab, always visible
	graph_tabs.add_tab(nullptr, nullptr, glm::vec2(0.f), true);

	// load preview model and set and register renderable
	try_load_preview_models();

	// refresh control_param editor
	control_params.init_from_tree(get_tree()->params.get());
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



#if 0
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
#endif

class CotrolParamEditorRunTime : public IPropertyEditor
{
public:
	using IPropertyEditor::IPropertyEditor;

	virtual void internal_update() override {

		assert(prop->type == core_type_id::Struct);
		EditorControlParamProp* prop = (EditorControlParamProp*)instance;
		// prop.id is the index into runtime/cfg vars
		Animation_Tree_RT* rt = ed.get_runtime_tree();
		program_script_vars_instance* vars = &rt->vars;
		ControlParam_CFG* params = ed.editing_tree->params.get();
		AG_ControlParam* control = &params->types.at(prop->current_id);
		ControlParamHandle handle = { prop->current_id };
		if (control->type == control_param_type::bool_t) {
			bool b = params->get_bool(vars, handle);
			ImGui::Checkbox("##checkbox", &b);
			params->set_bool(vars, handle, b);
		}
		else if (control->type == control_param_type::int_t) {
			int i = params->get_int(vars, handle);
			ImGui::InputInt("##inputint", &i);
			params->set_int(vars, handle, i);
		}
		else if (control->type == control_param_type::float_t) {
			float f = params->get_float(vars, handle);
			ImGui::SliderFloat("##slidefloat", &f, 0.0, 1.0);
			params->set_float(vars, handle, f);
		}
		else if (control->type == control_param_type::enum_t) {
			ImGui::Text("ENUM PLACEHOLDER");
		}
		ImGui::SameLine();
	}

};

#include "Framework/AddClassToFactory.h"

struct AutoStruct_asdf {
	AutoStruct_asdf() {
		auto& pfac = get_property_editor_factory();

		pfac.registerClass<FindAnimationClipPropertyEditor>("AG_CLIP_TYPE");
		pfac.registerClass<AgLispCodeEditorProperty>("AG_LISP_CODE");
		pfac.registerClass<AgEnumFinder>("AG_ENUM_TYPE_FINDER");
		pfac.registerClass<AgParamFinder>("AG_PARAM_FINDER");
		pfac.registerClass<AgEdtior_BlendSpaceParameteriation>("AG_EDITOR_BLEND_SPACE_PARAMETERIZATION");
		pfac.registerClass<CotrolParamEditorRunTime>("AG_CONTROL_PARAM_RUN_EDIT");

		auto& afac = get_array_header_factory();

		afac.registerClass<ControlParamArrayHeader>("AG_CONTROL_PARAM_ARRAY");
		afac.registerClass<AgEditor_BlendSpaceArrayHead>("AG_EDITOR_BLEND_SPACE");
		afac.registerClass<ControlParamArrayHeader>("AG_CONTROL_PARAM_ARRAY_RUN_EDIT");

		auto& sfac = get_property_serializer_factory();

		sfac.registerClass<SerializeImNodeState>("SerializeImNodeState");
		sfac.registerClass<SerializeNodeCFGRef>("SerializeNodeCFGRef");

	}
};

static AutoStruct_asdf add_to_factories_asdf;

static const char* strs[] = {
	"FiveVert",
	"NineVert",
	"FifteenVert",
};
AutoEnumDef BlendSpace2dTopology_def = AutoEnumDef("blend2d", 3, strs);

std::string SerializeImNodeState::serialize(DictWriter& out, const PropertyInfo& info, void* inst, TypedVoidPtr userptr)
{
	auto context = *(ImNodesEditorContext**)info.get_ptr(inst);
	return ImNodes::SaveEditorStateToIniString(context);
}

void SerializeImNodeState::unserialize(DictParser& in, const PropertyInfo& info, void* inst, StringView token, TypedVoidPtr userptr)
{
	std::string inistring(token.str_start, token.str_len);
	auto context = (ImNodesEditorContext**)info.get_ptr(inst);
	*context = ImNodes::EditorContextCreate();
	ImNodes::EditorContextSet(*context);
	ImNodes::LoadEditorStateFromIniString(*context, inistring.c_str(), inistring.size());
}

std::string SerializeNodeCFGRef::serialize(DictWriter& out, const PropertyInfo& info, void* inst, TypedVoidPtr userptr)
{
	ASSERT(userptr.name == NAME("AgSerializeContext"));
	AgSerializeContext* context = (AgSerializeContext*)userptr.ptr;
	auto node = *(Node_CFG**)info.get_ptr(inst);
	ASSERT(context->ptr_to_index.find(node) != context->ptr_to_index.end());

	return std::to_string(context->ptr_to_index.find(node)->second);
}

void SerializeNodeCFGRef::unserialize(DictParser& in, const PropertyInfo& info, void* inst, StringView token, TypedVoidPtr userptr)
{
	ASSERT(userptr.name == NAME("AgSerializeContext"));
	AgSerializeContext* context = (AgSerializeContext*)userptr.ptr;
	auto node_ptr = (Node_CFG**)info.get_ptr(inst);
	int index = atoi(token.to_stack_string().c_str());
	ASSERT(index >= 0 && index < context->tree->all_nodes.size());
	*node_ptr = context->tree->all_nodes.at(index);
}

void GraphOutput::reset_animator()
{
	anim = Animator();
}

#include "Player.h"

Animator* GraphOutput::get_animator()
{
	if (eng->get_state() == Engine_State::Game) {
		Player* p = eng->get_local_player();
		if (p && p->animator.get() && p->animator.get()->runtime_dat.cfg == ed.editing_tree) {
			return p->animator.get();
		}
	}
	return &anim;
}
