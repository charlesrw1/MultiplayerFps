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

#include "Root_node.h"

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


ImVec4 scriptparamtype_to_color(anim_graph_value type)
{
	ASSERT((uint32_t)type <= 4);

	static ImVec4 color[] = {
		color32_to_imvec4({235, 64, 52}),	// bool 
		color32_to_imvec4({131, 222, 27}),	// float
		color32_to_imvec4({98, 227, 177}),	// int
		color32_to_imvec4({250, 222, 62}),	// vec3
		color32_to_imvec4({195, 179, 242}),	// quat
	};
	return color[(uint32_t)type];
}

ImVec4 graph_pin_type_to_color(GraphPinType pin)
{
	if (pin.type == GraphPinType::value_t)
		return scriptparamtype_to_color(pin.value_type);
	else if (pin.type == GraphPinType::meshspace_pose)
		return color32_to_imvec4({ 130, 191, 237 });
	return ImVec4(1, 1, 1, 1);
}



script_types anim_graph_value_to_script_value(anim_graph_value type)
{
	switch (type)
	{
	case anim_graph_value::bool_t: return script_types::bool_t;
	case anim_graph_value::int_t: return script_types::int_t;
	case anim_graph_value::float_t: return script_types::float_t;

	default:
		return script_types::custom_t;
	}
}

void ControlParamsWindow::refresh_props() {
	props.clear();
	const AnimatorInstance* a = ed.out.get_animator();
	if (!a)
		return;

	std::vector<const PropertyInfoList*> getprop;
	const ClassTypeInfo* type = &a->get_type();
	for (; type; type = type->super_typeinfo)
		getprop.push_back(type->props);

	for (int i = 0; i < getprop.size(); i++) {
		auto list = getprop[i];
		for (int j = 0; list && j < list->count; j++) {
			VariableParam vp;
			bool good = false;
			vp.type = core_type_id_to_anim_graph_value(&good, list->list[j].type);
			if (!good)
				continue;
			auto& prop = list->list[j];
			vp.str = prop.name;
			vp.nativepi = &list->list[j];
			props.push_back(vp);
		}
	}
}


 unique_ptr<Script> ControlParamsWindow::add_parameters_to_tree() {
	std::vector<ScriptVariable> vars;
	for (int i = 0; i < props.size(); i++) {
		ScriptVariable var;
		var.is_native = true;
		var.name = props[i].str;
		var.type = anim_graph_value_to_script_value(props[i].type);
		var.native_pi_of_variable = nullptr;//FIXME ??? inspect how this is initialized later
		vars.push_back(var);
	}

	// create script with variables and the AnimatorInstance classname
	auto script = std::make_unique<Script>(vars, ed.opt.AnimatorClass);

	return script;
}
#include "Framework/CurveEditorImgui.h"
static SequencerImgui seqimgui;


class AnimationEventEditor : public SequencerEditorItem
{
public:
	AnimationEventEditor(int start, int end, Color32 c) {
		this->time_start = start;
		this->time_end = end;
		this->color = c;
	}
	AnimationEventEditor(int start, Color32 c) {
		this->time_start = start;
		this->instant_item = true;
		this->color = c;
	}
	virtual std::string get_name() { return "name"; }
};

void AnimationGraphEditor::init()
{
	imgui_node_context = ImNodes::CreateContext();
	ImNodes::GetIO().LinkDetachWithModifierClick.Modifier = &is_modifier_pressed;


	ImNodes::GetStyle().Flags |= ImNodesStyleFlags_GridSnapping | ImNodesStyleFlags_GridLinesPrimary;

	// init template nodes for creation menu
	for (int i = 0; i < template_creation_nodes.size(); i++)
		delete template_creation_nodes[i];
	template_creation_nodes.clear();
	
	auto iter = ClassBase::get_subclasses<Base_EdNode>();
	for (;!iter.is_end();iter.next()) {
		auto classtype = iter.get_type();
		if (classtype->allocate) {
			ClassBase* node = classtype->allocate();
			ASSERT(node->is_a<Base_EdNode>());
			template_creation_nodes.push_back((Base_EdNode*)node);
		}
	}

	seqimgui.add_item(new AnimationEventEditor(0, 5, COLOR_BLUE));
	seqimgui.add_item(new AnimationEventEditor(8, 10, COLOR_RED));
	seqimgui.add_item(new AnimationEventEditor(6, COLOR_GREEN));

}

void AnimationGraphEditor::close_internal()
{
	notify_observers("OnClose");

	out.clear();

	self_grid.clear_all();

	for (int i = 0; i < nodes.size(); i++) {
		delete nodes[i];
	}
	nodes.clear();

	if (is_owning_editing_tree) {
		delete editing_tree;
	}
	editing_tree = nullptr;

	reset_prop_editor_next_tick = false;
	playback = graph_playback_state::stopped;
	sel = selection_state();
	drop_state = create_from_drop_state();
	current_id = 0;
	current_layer = 1;

	opt = settings();
	graph_tabs = TabState(this);
	node_props.clear_all();

	ImNodes::EditorContextFree(default_editor);
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

	auto forward_img = g_imgs.find_texture("icon/forward.png");
	auto back_img = g_imgs.find_texture("icon/back.png");


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

			if (state->inputs[slot].node) {
				ImGui::BeginTooltip();
				auto other_node = state->inputs[slot].node->cast_to<State_EdNode>();
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
bool AnimationGraphEditor::can_save_document() {
	return playback != graph_playback_state::running;
}
bool AnimationGraphEditor::save_document_internal()
{
	// first compile, compiling writes editor node data out to the CFG node
	// this converts data to serialized form (ie Node* become indicies)
	bool good = compile_graph_for_playing();

	DictWriter write;
	write.set_should_add_indents(false);

	editing_tree->write_to_dict(write);
	save_editor_nodes(write);

	std::ofstream outfile(get_save_root_dir() + get_doc_name());
	outfile.write(write.get_output().c_str(), write.get_output().size());
	outfile.close();

	// now the graph is in a compilied state with serialized nodes, unserialize it so it works again
	// with the engine
	get_tree()->post_load_init();	// initialize the memory offsets

	return true;
}



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
			Base_EdNode* node = read_object_properties<Base_EdNode>(userptr, in, view);
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
		write_object_properties(node, userptr, out);
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
				open_the_open_popup();

			}
			if (ImGui::MenuItem("Save", "Ctrl+S")) {
				save();
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

inline void add_props_from_ClassBase(std::vector<PropertyListInstancePair>& info, ClassBase* base)
{
	if (!base)
		return;
	const ClassTypeInfo* ti = &base->get_type();
	while (ti) {
		if (ti->props)
			info.push_back({ ti->props, base });
		ti = ti->super_typeinfo;
	}
}

void AnimationGraphEditor::draw_prop_editor()
{
	bool should_draw = false;
		
	if (ImNodes::NumSelectedNodes() == 1) {
		int node = 0;
		ImNodes::GetSelectedNodes(&node);

		Base_EdNode* mynode = find_node_from_id(node);

		if (node != sel.node_last_frame || reset_prop_editor_next_tick) {
			sel.link_last_frame = -1;

			node_props.clear_all();

			std::vector<PropertyListInstancePair> info;

			add_props_from_ClassBase(info, mynode);
			add_props_from_ClassBase(info, mynode->get_graph_node());
			mynode->add_props_for_extra_editable_element(info);

			for (int i = 0; i < info.size(); i++) {
				if(info[i].list) /* some nodes have null props */
					node_props.add_property_list_to_grid(info[i].list, info[i].instance);
			}

			sel.node_last_frame = node;
		}

		should_draw = true;
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

			should_draw = true;
		}
	
	}
	else {
		sel.link_last_frame = -1;
		sel.node_last_frame = -1;
	}
	reset_prop_editor_next_tick = false;


	if (ImGui::Begin("animation graph property editor"))
	{
		if (should_draw)
			node_props.update();
		else
			ImGui::Text("No node selected\n");
	}
	ImGui::End();
}

void AnimationGraphEditor::stop_playback()
{
	playback = graph_playback_state::stopped;

	control_params.refresh_props();
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

		uint32_t start_node_id = Base_EdNode::get_nodeid_from_output_id(start_atr);
		uint32_t end_node_id = Base_EdNode::get_nodeid_from_input_id(end_atr);
		uint32_t start_idx = Base_EdNode::get_slot_from_id(start_atr);
		uint32_t end_idx = Base_EdNode::get_slot_from_id(end_atr);

		ASSERT(start_idx == 0);

		Base_EdNode* node_s = find_node_from_id(start_node_id);
		Base_EdNode* node_e = find_node_from_id(end_node_id);

		if(end_idx >= node_e->inputs.size() /* because state nodes can have exposed slots that dont exist yet*/ || node_s->can_output_to_type(node_e->inputs[end_idx].type))
			bool destroy = node_e->add_input(this, node_s, end_idx);
	}
	if (ImNodes::IsLinkDropped(&start_atr)) {

		bool is_input = start_atr >= INPUT_START && start_atr < OUTPUT_START;

		uint32_t id = 0;
		if (is_input)
			id = Base_EdNode::get_nodeid_from_input_id(start_atr);
		else
			id = Base_EdNode::get_nodeid_from_output_id(start_atr);

		auto node = find_node_from_id(id);

		drop_state.from = node;
		drop_state.from_is_input = is_input;
		drop_state.slot = Base_EdNode::get_slot_from_id(start_atr);
		*open_popup_menu_from_drop = true;
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

#include "Framework/CurveEditorImgui.h"

void AnimationGraphEditor::imgui_draw()
{
	CurveEditorImgui cei;
	//cei.draw();
	seqimgui.draw();

	node_props.set_read_only(graph_is_read_only());

	if (opt.open_prop_editor)
		draw_prop_editor();

	control_params.imgui_draw();

	animation_list.imgui_draw();

	if (ImGui::Begin("AnimGraph settings")) {
		self_grid.update();
	}
	ImGui::End();


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
		auto playimg = g_imgs.find_texture("icon/play.png");
		auto stopimg = g_imgs.find_texture("icon/stop.png");
		auto pauseimg = g_imgs.find_texture("icon/pause.png");
		auto saveimg = g_imgs.find_texture("icon/save.png");

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
			save();
		}
		ImGui::PopStyleColor(3);
		ImGui::EndDisabled();
	}


	graph_tabs.imgui_draw();

	static ImVec2 mouse_click_pos = ImVec2(0, 0);
	if (!graph_is_read_only()) {
		bool open_popup_menu_from_drop = false;

		handle_imnode_creations(&open_popup_menu_from_drop);

		is_focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows | ImGuiFocusedFlags_RootWindow);

		if (open_popup_menu_from_drop ||
			(is_focused && ImGui::GetIO().MouseClicked[1])) {
			ImGui::OpenPopup("my_select_popup");
			mouse_click_pos = ImGui::GetMousePos();
		}
		if (ImGui::BeginPopup("my_select_popup"))
		{
			bool is_sm = graph_tabs.get_active_tab()->is_statemachine_tab();
			draw_node_creation_menu(is_sm, mouse_click_pos);
			ImGui::EndPopup();
		}
		else {
			drop_state.from = nullptr;
		}
	}



	ImGui::End();

	IEditorTool::imgui_draw();
}

void AnimationGraphEditor::draw_graph_layer(uint32_t layer)
{
	auto strong_error = g_imgs.find_texture("icon/fatalerr.png");
	auto weak_error = g_imgs.find_texture("icon/error.png");
	auto info_img = g_imgs.find_texture("icon/question.png");

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

		{
			auto cursorpos = ImGui::GetCursorPos();
			ImGui::Dummy(ImVec2(100, 0.0));
			ImGui::SameLine(0, 0);
			ImGui::SetCursorPosX(cursorpos.x);
			ImGui::Text("%s\n", node->get_title().c_str());
		}

		if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal) && ImGui::BeginTooltip()) {
			auto drawlist = ImGui::GetForegroundDrawList();
			auto& style = ImGui::GetStyle();
			auto min = ImGui::GetCursorScreenPos();
			auto str = node->get_tooltip();
			auto sz = ImGui::CalcTextSize(str.c_str());
			float width = ImGui::CalcItemWidth();

			auto minrect = ImVec2(min.x - style.FramePadding.x * 5.0, min.y);
			auto maxrect = ImVec2(min.x + sz.x + style.FramePadding.x * 5.0, min.y + sz.y + style.FramePadding.y * 2.0);
			float border = 2.0;
			drawlist->AddRectFilled(ImVec2(minrect.x-border,minrect.y-border),ImVec2( maxrect.x+border,maxrect.y+border), Color32{ 0,0,0, 255 }.to_uint());


			drawlist->AddRectFilled(minrect,maxrect, Color32{ 255,255,255, 255 }.to_uint());
			auto cursor = ImGui::GetCursorScreenPos();
			drawlist->AddText(cursor, Color32{ 0,0,0 }.to_uint(), str.c_str());
			//ImGui::Text(str.c_str());
			//ImGui::Text(node->get_tooltip().c_str());
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

		int num_inputs = node->get_input_size();
		for (int j = 0; j < num_inputs; j++) {

			ImNodesPinShape pin = ImNodesPinShape_Quad;

			if (node->inputs[j].is_attached_to_node()) 
				pin = ImNodesPinShape_TriangleFilled;
			ImNodes::BeginInputAttribute(node->getinput_id(j), pin);
			auto str = node->get_input_pin_name(j);

			auto input_type = node->inputs[j].type;

			ImGui::TextColored(graph_pin_type_to_color(input_type), str.c_str());
			ImNodes::EndInputAttribute();
		}
		if (node->has_output_pin()) {
			ImNodes::BeginOutputAttribute(node->getoutput_id(0));
			auto output_str = node->get_output_pin_name();

			auto posX = (ImGui::GetCursorPosX() + (x2-x1) - ImGui::CalcTextSize(output_str.c_str()).x - 2 * ImGui::GetStyle().ItemSpacing.x)-5.0;
			if (posX > ImGui::GetCursorPosX())
				ImGui::SetCursorPosX(posX);

			auto output_type = node->get_output_type_general();
		
			ImGui::TextColored(graph_pin_type_to_color(output_type), output_str.c_str());
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
			if (node->inputs[j].is_attached_to_node()) {

				int offset = 0;
				if (draw_flat_links) {
					for (int k = 0; k < j; k++) {
						if (node->inputs[k].node == node->inputs[j].node)
							offset++;
					}
				}


				bool pushed_colors = node->push_imnode_link_colors(j);
	
				ImNodes::Link(node->getlink_id(j), node->inputs[j].node->getoutput_id(0), node->getinput_id(j), draw_flat_links, offset);

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

	if (ImGui::BeginDragDropTarget())
	{
		//const ImGuiPayload* payload = ImGui::GetDragDropPayload();
		//if (payload->IsDataType("AssetBrowserDragDrop"))
		//	sys_print("``` accepting\n");
		bool is_sm = graph_tabs.get_active_tab()->is_statemachine_tab();
		uint32_t layer = graph_tabs.get_current_layer_from_tab();
		if (!is_sm) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("AnimationItemAnimGraphEd"))
			{
				std::string* resource = *(std::string**)payload->Data;
				auto node = user_create_new_graphnode(Clip_EdNode::StaticType.classname, layer);
				Clip_EdNode* cl = node->cast_to<Clip_EdNode>();
				ASSERT(cl);

				cl->node->clip_name = *resource;

				ImNodes::ClearNodeSelection();
				ImNodes::SetNodeScreenSpacePos(cl->id, ImGui::GetMousePos());
				ImNodes::SelectNode(cl->id);
			}
			else if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("AnimGraphVariableDrag")) {
				VariableNameAndType* var = *(VariableNameAndType**)payload->Data;
				auto node = user_create_new_graphnode(Variable_EdNode::StaticType.classname, layer);
				Variable_EdNode* v = node->cast_to<Variable_EdNode>();
				ASSERT(v);

				v->variable = *var;

				ImNodes::ClearNodeSelection();
				ImNodes::SetNodeScreenSpacePos(v->id, ImGui::GetMousePos());
				ImNodes::SelectNode(v->id);
			}
		}
		ImGui::EndDragDropTarget();
	}
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
	
	}

	clipboard.handle_event(event);

	if (eng->get_game_focused()) {
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

	clipboard.remove_references(node);

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
		// FIXME: clipboard doesnt get nuked
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

	Base_EdNode* node = ClassBase::create_class<Base_EdNode>(typename_);//factory.createObject(typename_);

	if (!node) {
		printf("factory doesnt have node for typename %s\n", typename_);
		return nullptr;
	}

	node->post_construct(current_id++, layer);
	nodes.push_back(node);
	node->init();

	return node;
}

void AnimationGraphEditor::draw_node_creation_menu(bool is_state_mode, ImVec2 mouse_click_pos)
{
	int count = template_creation_nodes.size();
	for (int i = 0; i < count; i++) {
		const Base_EdNode* node = template_creation_nodes[i];

		if (node->is_state_node() != is_state_mode)
			continue;

		if (!node->allow_creation_from_menu())
			continue;

		if (drop_state.from && drop_state.from_is_input && drop_state.slot < drop_state.from->inputs.size() && /* because state nodes can have num slots greater than actual slots */
			!node->can_output_to_type(drop_state.from->inputs[drop_state.slot].type))
			continue;

		if (drop_state.from && !drop_state.from_is_input && !drop_state.from->is_a<State_EdNode>())
			continue;

		const std::string& name = node->get_name();

		if (ImGui::Selectable(name.c_str())) {

			int cur_layer = graph_tabs.get_current_layer_from_tab();

			Base_EdNode* a  = user_create_new_graphnode(node->get_type().classname, cur_layer);

			ImNodes::ClearNodeSelection();
			ImNodes::SetNodeScreenSpacePos(a->id, mouse_click_pos);
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

		std::unordered_set<BaseAGNode*> refed_nodes;

		std::vector<BaseAGNode*> extra_nodes;
		for (int i = 0; i < nodes.size(); i++) {
			refed_nodes.insert(nodes[i]->get_graph_node());
			nodes[i]->get_any_extra_refed_graph_nodes(extra_nodes);
		}
		for (auto node : extra_nodes)
			refed_nodes.insert(node);

		int num_deleted = 0;
		int count = 0;
		std::unordered_set<BaseAGNode*> removed_nodes;
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
	editing_tree->code.reset();										// delete script
	editing_tree->code = control_params.add_parameters_to_tree();	// recreate script
	editing_tree->code->link_to_native_class();

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

	auto output_pose = find_first_node_in_layer<Root_EdNode>(0);
	ASSERT(output_pose);
	output_pose->compile_error_string.clear();	//???
	Node_CFG* root_node = output_pose->get_root_node();
	if(!root_node) {
		output_pose->append_fail_msg("[ERROR] no output pose\n");
	}
	editing_tree->root = (Node_CFG*)ptr_to_serialized_nodecfg_ptr(root_node, &ctx);


	bool tree_is_good_to_run = output_pose->traverse_and_find_errors();


	editing_tree->graph_is_valid = tree_is_good_to_run;

	// recreate script AGAIN because nodes compilied functions into the script
	// but it gets compilied again after this
	editing_tree->code.reset();										// delete script
	editing_tree->code = control_params.add_parameters_to_tree();	// recreate script

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
		if (ed->out.get_model() && ed->out.get_model()->get_skel()) {

			auto& clips = ed->out.get_model()->get_skel()->get_clips_hashmap();
			for (const auto& c : clips)
				if (_strnicmp(c.first.c_str(), word_start, len) == 0)
					vec.push_back(c.first.c_str());
			std::sort(vec.begin(), vec.end(), [](const char* a, const char* b) -> bool {
				return strcmp(a, b) < 0;
				});
		}
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

		if (eng->get_state() != Engine_State::Game) {
			stop_playback();
			compile_and_run();
		}
		control_params.refresh_props();
		out.hide();
		playback = graph_playback_state::running;
		//Cmd_Manager::get()->execute(Cmd_Execute_Mode::NOW, "dump_imgui_ini animdock.ini");
	}
	else if(newstate == editor_focus_state::Closed){
		close();
		//Cmd_Manager::get()->execute(Cmd_Execute_Mode::NOW, "dump_imgui_ini animdock.ini");
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
		if (eng->get_game_focused()) {
			SDL_GetRelativeMouseState(&x, &y);
			out.camera.update_from_input(eng->keys, x, y, glm::mat4(1.f));
		}

		if (get_playback_state() == graph_playback_state::running) {
			auto animator = out.get_animator();
			if(animator)
				animator->tick_tree_new(dt * g_slomo.get_float());
		}
		
		out.show(get_playback_state() != graph_playback_state::stopped);

		auto window_sz = eng->get_game_viewport_dimensions();
		out.vs = View_Setup(out.camera.position, out.camera.front, glm::radians(70.f), 0.01, 100.0, window_sz.x, window_sz.y);
	}
	else {	// not focused, game running likely
		playback = graph_playback_state::running;
	}

}

#include "Framework/StdVectorReflection.h"

ControlParamsWindow::ControlParamsWindow()
{
	ed.register_me("OnSetAnimatorInstance", this);
	ed.register_me("OnClose", this);
	ed.register_me("OnOpenNewDocument", this);
}

void ControlParamsWindow::on_notify(const std::string& str)
{
	if (str == "OnSetAnimatorInstance") {
		refresh_props();
	}
	else if (str == "OnClose") {
		props.clear();
	}
}

void ControlParamsWindow::imgui_draw()
{
	if (!ImGui::Begin("Control parameters")) {
		ImGui::End();
		return;
	}

	uint32_t ent_list_flags = ImGuiTableFlags_PadOuterX | ImGuiTableFlags_Borders |
		ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable;


	if (ImGui::BeginTable("controlproplistabc", 2, ent_list_flags))
	{
		ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.0f, 0);
		ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 50.0f, 0);
		ImGui::TableHeadersRow();

		for (int row_n = 0; row_n < props.size(); row_n++)
		{
			auto& res = props[row_n];
			ImGui::PushID(res.str.c_str());
			
			ImGui::TableNextRow();
			ImGui::TableNextColumn();

			ImGuiSelectableFlags selectable_flags = ImGuiSelectableFlags_AllowDoubleClick | ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap;
			if (ImGui::Selectable("##selectednode", false, selectable_flags, ImVec2(0, 0))) {
	
			}

			if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
			{
				dragdrop = { res.str, res.type };
				auto ptr = &dragdrop;


				ImGui::SetDragDropPayload("AnimGraphVariableDrag", &ptr, sizeof(VariableNameAndType*));

				ImGui::TextColored(scriptparamtype_to_color(res.type),res.str.c_str());

				ImGui::EndDragDropSource();
			}

			ImGui::SameLine();
			ImGui::Text(res.str.c_str());
			ImGui::TableNextColumn();
			std::string s = EnumTrait<anim_graph_value>::StaticType.get_enum_str((int)res.type);
			auto find = s.rfind("::");
			if (find != std::string::npos)
				s = s.substr(find + 2);
			ImGui::TextColored(scriptparamtype_to_color(res.type), s.c_str());

			ImGui::PopID();
		}
		ImGui::EndTable();
	}
	ImGui::End();
}


void AnimationGraphEditor::compile_and_run()
{
	bool good_to_run = compile_graph_for_playing();
	get_tree()->post_load_init();
	if (good_to_run) {
		out.initialize_animator(get_tree());
		playback = graph_playback_state::running;
	}
}

void AnimationGraphEditor::overlay_draw()
{

}


void AnimationGraphEditor::create_new_document()
{
	printf("creating new document");
	set_empty_name();	// when saving, user is prompted
	editing_tree = new Animation_Tree_CFG;
	is_owning_editing_tree = true;
	// add the output pose node to the root layer
	add_root_node_to_layer(nullptr, 0, false);
	// create context for root layer
	default_editor = ImNodes::EditorContextCreate();
	ImNodes::EditorContextSet(default_editor);
}
#include "Render/Material.h"
void AnimationGraphEditor::set_animator_instance_from_string(std::string str) {
	auto class_ = ClassBase::create_class<AnimatorInstance>(str.c_str());
	if (!class_) {
		sys_print("!!! couldnt find animatorInstance class %s\n", str.c_str());
	}

	out.set_animator_instance(class_);

	notify_observers("OnSetAnimatorInstance");
}
void AnimationGraphEditor::set_model_from_str(std::string str) {
	auto mod = mods.find_or_load(str.c_str());
	if (!mod) {
		sys_print("!!! couldnt find preview model %s\n", str.c_str());
	}
	out.set_model(mod);

	notify_observers("OnSetModel");
}
void AnimationGraphEditor::try_load_preview_models()
{
#if 0
	{
		Material* mymat = mats.create_temp_shader("sprite_texture");
		mymat->billboard = billboard_setting::SCREENSPACE;
		mymat->images[0] = g_imgs.find_texture("icon/light.png");
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
#endif
	set_animator_instance_from_string(opt.AnimatorClass);
	set_model_from_str(opt.PreviewModel);
}

void AnimationGraphEditor::open_document_internal(const char* name)
{
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
				set_doc_name(name);

				for (int i = 0; i < nodes.size(); i++) {
					nodes[i]->init();
				}

			}
		}
		if(needs_new_doc)
			sys_print("!!! Couldn't open animation tree file %s, creating new document instead\n", name);
	}

	if (needs_new_doc) {
		set_empty_name();
		create_new_document();
		ASSERT(!current_document_has_path());
	}

	ASSERT(editing_tree);

	// initialize other state
	graph_tabs = TabState(this);
	// push root tab, always visible
	graph_tabs.add_tab(nullptr, nullptr, glm::vec2(0.f), true);

	self_grid.add_property_list_to_grid(get_props(), this);

	// load preview model and set and register renderable
	try_load_preview_models();

	notify_observers("OnOpenNewDocument");

	// refresh control_param editor
	//control_params.init_from_tree(get_tree()->params.get());
}


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

#include "Framework/AddClassToFactory.h"




void GraphOutput::show(bool is_playing)
{
	if (!obj.is_valid())
		obj = idraw->register_obj();
	Render_Object obj_data;
	obj_data.model = model;
	if (is_playing && model->get_skel() && anim) {
		obj_data.animator = anim.get();
		obj_data.transform = model->get_root_transform();
	}
	idraw->update_obj(obj,obj_data);
}
void GraphOutput::hide()
{
	idraw->remove_obj(obj);
}
void GraphOutput::clear()
{
	model = nullptr;
	anim.reset();
	idraw->remove_obj(obj);
}
void GraphOutput::set_animator_instance(AnimatorInstance* inst)
{
	anim.reset( inst );
	idraw->remove_obj(obj);
}

#include "Player.h"

AnimatorInstance* GraphOutput::get_animator()
{
	return anim.get();
}

ListAnimationDataInModel::ListAnimationDataInModel()
{
	ed.register_me("OnSetModel", this);
	ed.register_me("OnClose", this);
	name_filter[0] = 0;
}
static std::string to_lower(const std::string& s) {
	std::string out;
	out.reserve(s.size());
	for (auto c : s)
		out.push_back(tolower(c));
	return out;
}

void ListAnimationDataInModel::set_model(const Model* model)
{
	drag_drop_name = "";
	vec.clear();
	name_filter[0] = 0;
	selected_name = "";
	this->model = model;
	if (!model||!model->get_skel())
		return;
	auto& clips = model->get_skel()->get_clips_hashmap();
	for (auto& clip : clips) {
		vec.push_back(clip.first);
	}
	std::sort(vec.begin(), vec.end(), [](const std::string& a, const std::string& b) -> bool { return to_lower(a) < to_lower(b); });
}
void ListAnimationDataInModel::imgui_draw()
{
	if (!ImGui::Begin("Animation List")) {
		ImGui::End();
		return;
	}

	uint32_t ent_list_flags = ImGuiTableFlags_PadOuterX | ImGuiTableFlags_Borders |
		ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable;

	//if (set_keyboard_focus) {
	//	ImGui::SetKeyboardFocusHere();
	//	set_keyboard_focus = false;
	//}
	static bool match_case = false;
	ImGui::SetNextItemWidth(200.0);
	ImGui::InputTextWithHint("FILTER", "filter animation name", name_filter, 256);
	const int name_filter_len = strlen(name_filter);


	std::string all_lower_cast_filter_name;
	if (!match_case) {
		all_lower_cast_filter_name = name_filter;
		for (int i = 0; i < name_filter_len; i++)
			all_lower_cast_filter_name[i] = tolower(all_lower_cast_filter_name[i]);
	}

	if (ImGui::BeginTable("animedBrowserlist", 1, ent_list_flags))
	{
		ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.0f, 0);

		ImGui::TableHeadersRow();

		for (int row_n = 0; row_n < vec.size(); row_n++)
		{
			auto& res = vec[row_n];

			if (name_filter_len > 0) {
				if (res.find(name_filter) == std::string::npos)
					continue;
			}

			ImGui::PushID(res.c_str());
			const bool item_is_selected = res == selected_name;

			ImGui::TableNextRow();
			ImGui::TableNextColumn();

			ImGuiSelectableFlags selectable_flags = ImGuiSelectableFlags_AllowDoubleClick | ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap;
			if (ImGui::Selectable("##selectednode", item_is_selected, selectable_flags, ImVec2(0, 0))) {
				selected_name = res;
			}

			if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
			{
				drag_drop_name = res;
				auto ptr = &drag_drop_name;


				ImGui::SetDragDropPayload("AnimationItemAnimGraphEd", &ptr, sizeof(std::string*));

				ImGui::Text(res.c_str());

				ImGui::EndDragDropSource();
			}

			ImGui::SameLine();
			ImGui::Text(res.c_str());

			ImGui::PopID();
		}
		ImGui::EndTable();
	}
	ImGui::End();
}
void ListAnimationDataInModel::on_notify(const std::string& str)
{
	if (str == "OnClose") {
		model = nullptr;
		vec.clear();
	}
	else if (str == "OnSetModel") {
		set_model(ed.out.get_model());
	}
}

AnimGraphClipboard::AnimGraphClipboard()
{
	ed.register_me("OnClose",this);
}
void AnimGraphClipboard::on_notify(const std::string& strname) {
	if (strname == "OnClose")
		clipboard.clear();
}

void AnimGraphClipboard::handle_event(const SDL_Event& event) {
	if (event.type == SDL_KEYDOWN) {
		if (event.key.keysym.scancode == SDL_SCANCODE_C && event.key.keysym.mod & KMOD_LCTRL) {
			int count = ImNodes::NumSelectedNodes();
			std::vector<int> ids(count,0);
			ImNodes::GetSelectedNodes(ids.data());
			
			clipboard.clear();
			for (int i = 0; i < ids.size(); i++)
				clipboard.push_back(ed.find_node_from_id(ids[i]));
		}
		else if (event.key.keysym.scancode == SDL_SCANCODE_V && event.key.keysym.mod & KMOD_LCTRL) {
			paste_selected();
		}
	}
}
void AnimGraphClipboard::paste_selected()
{
	// copying expects the serialize context ...
	AgSerializeContext context(ed.get_tree());
	TypedVoidPtr userptr(NAME("AgSerializeContext"), &context);

	// create copy of the animation graph node
	// create copy of the editor node with the node* set to the copied agraph node

	std::vector<Base_EdNode*> copied_list;
	std::vector<ImVec2> ss_pos;

	ImVec2 posmin = ImVec2(FLT_MAX,FLT_MAX);
	ImVec2 posmax = ImVec2(FLT_MIN, FLT_MIN);

	for (int i = 0; i < clipboard.size(); i++) {
		Base_EdNode* source = clipboard[i];
		if (!source->can_delete())
			continue;

		if (source->is_a<Statemachine_EdNode>() || source->is_a<State_EdNode>()) {
			sys_print("??? Can't copy+paste statemachine or state nodes (maybe a future feature)\n");
			/* would have to do recursive stuff to copy layers and states work a bit funky */
			continue;
		}

		Base_EdNode* copied_node = source->create_copy(userptr)->cast_to<Base_EdNode>();

		const ClassTypeInfo* ti = &copied_node->get_type();
		for (; ti; ti = ti->super_typeinfo) {
			if (!ti->props) continue;

			for (int j = 0; j < ti->props->count; j++) {
				auto& prop = ti->props->list[j];
				if (strcmp(prop.custom_type_str, "SerializeNodeCFGRef") == 0) {
					
					// found a property that references a runtime graph node, copy its data
					BaseAGNode* runtime_node = *(BaseAGNode**)prop.get_ptr(source);
					
					BaseAGNode* copied_runtime = runtime_node->create_copy(userptr)->cast_to<BaseAGNode>();

					BaseAGNode** runtime_node_ptr_copy = (BaseAGNode**)prop.get_ptr(copied_node);
					*runtime_node_ptr_copy = copied_runtime;

					const ClassTypeInfo* ti_ag = &(*runtime_node_ptr_copy)->get_type();
					for (; ti_ag; ti_ag = ti_ag->super_typeinfo) {
						if (!ti_ag->props) continue;
						for (int l = 0; l < ti_ag->props->count; l++) {
							if (strcmp(ti_ag->props->list[l].custom_type_str, "AgSerializeNodeCfg") == 0) {
								BaseAGNode** ptr = (BaseAGNode**)ti_ag->props->list[l].get_ptr(*runtime_node_ptr_copy);
								*ptr = nullptr;
							}
						}
					}


					ed.add_node_to_tree_manual(copied_runtime);
				}
			}
		}

		copied_node->id = ed.current_id++;

		copied_node->graph_layer = ed.graph_tabs.get_current_layer_from_tab();
	
		ed.nodes.push_back(copied_node);

		copied_node->init();

		copied_list.push_back(copied_node);
		auto ss = ImNodes::GetNodeScreenSpacePos(source->id);
		ss_pos.push_back(ss);
		posmax = ImVec2(glm::max(ss.x, posmax.x), glm::max(ss.y, posmax.y));
		posmin = ImVec2(glm::min(ss.x, posmin.x), glm::min(ss.y, posmin.y));
	}

	ImVec2 posmiddle = ImVec2((posmax.x + posmin.x)*0.5, (posmax.y + posmin.y)*0.5);

#define VECADD(a,b) ImVec2(a.x+b.x,a.y+b.y)
#define VECSUB(a,b) ImVec2(a.x-b.x,a.y-b.y)
	ImNodes::ClearNodeSelection();
	auto mousepos = ImGui::GetMousePos();
	for (int i = 0; i < copied_list.size(); i++) {
		auto where_ = VECSUB(ss_pos[i], posmiddle);
		where_ = VECADD(where_, mousepos);
		ImNodes::SetNodeScreenSpacePos(copied_list[i]->id, where_);
		ImNodes::SelectNode(copied_list[i]->id);
	}

}
void AnimGraphClipboard::remove_references(Base_EdNode* node)
{
	for (int i = 0; i < clipboard.size(); i++) {
		if (clipboard[i] == node) {
			clipboard.erase(clipboard.begin() + i);
			i--;
		}
	}
}