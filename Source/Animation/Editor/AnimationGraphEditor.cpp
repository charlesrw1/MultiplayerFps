#include "AnimationGraphEditor.h"
#include <cstdio>
#include "imnodes.h"
#include "glm/glm.hpp"

#include "Render/Texture.h"
#include "GameEnginePublic.h"

#include "Framework/EnumDefReflection.h"
#include "Framework/DictWriter.h"
#include "Framework/ReflectionMacros.h"
#include "Framework/ArrayReflection.h"
#include "Framework/ObjectSerialization.h"
#include "Framework/MyImguiLib.h"
#include "Framework/Files.h"
#include "Framework/MulticastDelegate.h"

#include <fstream>

#include "State_node.h"
#include "Statemachine_node.h"
#include "OsInput.h"
#include "Root_node.h"
#include "UI/Widgets/Layouts.h"
#include "UI/GUISystemPublic.h"

#include "Game/StdEntityTypes.h"
#include "Framework/AddClassToFactory.h"

#include "GameEnginePublic.h"

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


class AG_GuiLayout : public GUIFullscreen
{
public:
	void on_pressed(int x, int y, int button) override {
		eng->get_gui()->set_focus_to_this(this);
		mouse_down_delegate.invoke(x, y, button);
	}
	void on_released(int x, int y, int button) override {
		mouse_up_delegate.invoke(x, y, button);
	}
	void on_key_down(const SDL_KeyboardEvent& key_event) override {
		key_down_delegate.invoke(key_event);
	}
	void on_key_up(const SDL_KeyboardEvent& key_event) override {
		key_up_delegate.invoke(key_event);
	}
	void on_mouse_scroll(const SDL_MouseWheelEvent& wheel) override {
		wheel_delegate.invoke(wheel);
	}

	MulticastDelegate<const SDL_KeyboardEvent&> key_down_delegate;
	MulticastDelegate<const SDL_KeyboardEvent&> key_up_delegate;
	MulticastDelegate<int, int, int> mouse_down_delegate;
	MulticastDelegate<int, int, int> mouse_up_delegate;
	MulticastDelegate<const SDL_MouseWheelEvent&> wheel_delegate;
};


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


//static CurveEditorImgui cei;
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

	
}

void AnimationGraphEditor::close_internal()
{
	EditorTool3d::close_internal();

	on_close.invoke();

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
	*graph_tabs = TabState(this);
	node_props->clear_all();

	ImNodes::EditorContextFree(default_editor);

	gui->unlink_and_release_from_parent();
}

static std::string saved_settings = "";


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
#include "Assets/AssetDatabase.h"

void TabState::imgui_draw() {

	update_tab_names();

	int rendered = 0;

	if (tabs.empty()) return;

	auto forward_img = GetAssets().find_global_sync<Texture>("icon/forward.png");
	auto back_img = GetAssets().find_global_sync<Texture>("icon/back.png");


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
					ImGui::TextColored(ImVec4(1.0, 0.3, 0.3, 1.0), "read conditions");

				ImGui::EndTooltip();
			}

		}

	}
}
bool AnimationGraphEditor::can_save_document() {
	return playback != graph_playback_state::running;
}
#include "AssetCompile/Someutils.h"

bool AnimationGraphEditor::save_document_internal()
{
	// first compile, compiling writes editor node data out to the CFG node
	// this converts data to serialized form (ie Node* become indicies)
	bool good = compile_graph_for_playing();

	{
		DictWriter write;
		write.set_should_add_indents(false);

		editing_tree->write_to_dict(write);
		auto outfile = FileSys::open_write_game(get_doc_name());
		outfile->write(write.get_output().c_str(), write.get_output().size());
		outfile->close();
	}
	{
		DictWriter write;
		save_editor_nodes(write);
		auto name = strip_extension(get_doc_name()) + ".ag_e";
		auto outfile = FileSys::open_write_game(name);
		outfile->write(write.get_output().c_str(), write.get_output().size());
		outfile->close();
	}

	// now the graph is in a compilied state with serialized nodes, unserialize it so it works again
	// with the engine
	get_tree()->post_load_init();	// initialize the memory offsets

	return true;
}



bool AnimationGraphEditor::load_editor_nodes(DictParser& in)
{
	AgSerializeContext context;
	context.set_tree(get_tree());


	if (!in.expect_string("editor") || !in.expect_item_start())
		return false;
	{
		if (!in.expect_string("rootstate") || !in.expect_item_start())
			return false;
		auto out = read_properties(*get_props(), this, in, {}, &context);
		if (!out.second || !in.check_item_end(out.first))
			return false;
	}

	{
		if (!in.expect_string("nodes") || !in.expect_list_start())
			return false;
		bool good = in.read_list_and_apply_functor([&](StringView view) -> bool {
			Base_EdNode* node = read_object_properties<Base_EdNode>(&context, in, view);
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
	AgSerializeContext context;
	context.set_tree(get_tree());

	out.write_value("editor");
	out.write_item_start();

	out.write_key("rootstate");
	out.write_item_start();
	write_properties(*get_props(), this, out, {});
	out.write_item_end();

	out.write_key_list_start("nodes");
	for (int i = 0; i < nodes.size(); i++) {
		Base_EdNode* node = nodes[i];
		write_object_properties(node, &context, out);
	}
	out.write_list_end();

	out.write_item_end();
}

void AnimationGraphEditor::pause_playback()
{
	ASSERT(playback != graph_playback_state::stopped);
	playback = graph_playback_state::paused;
}

void AnimationGraphEditor::hook_menu_bar()
{
	if (ImGui::BeginMenu("View")) {
		ImGui::Checkbox("Graph", &opt.open_graph);
		ImGui::Checkbox("Control params", &opt.open_control_params);
		ImGui::Checkbox("Viewport", &opt.open_viewport);
		ImGui::Checkbox("Property Ed", &opt.open_prop_editor);
		ImGui::EndMenu();
	}
}

void AnimationGraphEditor::start_or_resume_playback()
{
	if (playback == graph_playback_state::stopped)
		compile_and_run();
	else
		playback = graph_playback_state::running;

	control_params->refresh_props();
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

			node_props->clear_all();

			std::vector<PropertyListInstancePair> info;

			add_props_from_ClassBase(info, mynode);
			add_props_from_ClassBase(info, mynode->get_graph_node());
			mynode->add_props_for_extra_editable_element(info);

			for (int i = 0; i < info.size(); i++) {
				if(info[i].list) /* some nodes have null props */
					node_props->add_property_list_to_grid(info[i].list, info[i].instance);
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

				node_props->clear_all();

				std::vector<PropertyListInstancePair> info;
				node_s->get_link_props(info, slot);

				for (int i = 0; i < info.size(); i++) {
					if (info[i].list) /* some nodes have null props */
						node_props->add_property_list_to_grid(info[i].list, info[i].instance);
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
			node_props->update();
		else
			ImGui::Text("No node selected\n");
	}
	ImGui::End();
}

void AnimationGraphEditor::stop_playback()
{
	playback = graph_playback_state::stopped;

	control_params->refresh_props();
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



#include "Framework/CurveEditorImgui.h"

void AnimationGraphEditor::imgui_draw()
{
	//cei.draw();
	//seqimgui.draw();
	//cei.draw();

	node_props->set_read_only(graph_is_read_only());

	if (opt.open_prop_editor)
		draw_prop_editor();

	control_params->imgui_draw();


	if (ImGui::Begin("AnimGraph settings")) {
		self_grid.update();
		if (self_grid.rows_had_changes) {
			auto anim_instance = (!anim_class_type.ptr) ? new AnimatorInstance : (AnimatorInstance*)anim_class_type.ptr->allocate();
			ed.out.set_animator_instance(anim_instance);
			on_set_animator_instance.invoke(ed.out.get_animator());
			ed.out.set_model(output_model.get());
			on_set_model.invoke(output_model.get());
		}
	}
	ImGui::End();


	is_modifier_pressed = ImGui::GetIO().KeyAlt;


	ImGui::Begin("animation graph editor");

	if (ImGui::GetIO().MouseClickedCount[0] == 2) {

		if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_RootWindow) && ImNodes::NumSelectedNodes() == 1) {
			int node = 0;
			ImNodes::GetSelectedNodes(&node);

			Base_EdNode* mynode = find_node_from_id(node);
			
			const editor_layer* layer = mynode->get_layer();

			if (layer) {
				auto findtab = graph_tabs->find_tab_index(mynode);
				if (findtab!=-1) {
					graph_tabs->push_tab_to_view(findtab);
				}
				else {
					graph_tabs->add_tab(layer, mynode, glm::vec2(0.f), true);
				}
			}
			ImNodes::ClearNodeSelection();

		}
	}

	{
		auto playimg = GetAssets().find_global_sync<Texture>("icon/play.png");
		auto stopimg = GetAssets().find_global_sync<Texture>("icon/stop.png");
		auto pauseimg = GetAssets().find_global_sync<Texture>("icon/pause.png");
		auto saveimg = GetAssets().find_global_sync<Texture>("icon/save.png");

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


	graph_tabs->imgui_draw();

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
			bool is_sm = graph_tabs->get_active_tab()->is_statemachine_tab();
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
	auto strong_error = GetAssets().find_global_sync<Texture>("icon/fatalerr.png");
	auto weak_error = GetAssets().find_global_sync<Texture>("icon/error.png");
	auto info_img = GetAssets().find_global_sync<Texture>("icon/question.png");

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
		bool is_sm = graph_tabs->get_active_tab()->is_statemachine_tab();
		uint32_t layer = graph_tabs->get_current_layer_from_tab();
		if (!is_sm) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("AnimationItemAnimGraphEd"))
			{
				std::string* resource = *(std::string**)payload->Data;
				auto node = user_create_new_graphnode(Clip_EdNode::StaticType.classname, layer);
				Clip_EdNode* cl = node->cast_to<Clip_EdNode>();
				ASSERT(cl);

				//cl->node->clip_name = *resource;

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

void AnimationGraphEditor::on_key_down(const SDL_KeyboardEvent& key) {
	switch (key.keysym.scancode)
	{
	case SDL_SCANCODE_DELETE:
		if (is_focused) {
			delete_selected();
		}
		break;

	case SDL_SCANCODE_SPACE:
		if (key.keysym.mod & KMOD_LCTRL && !ImGui::GetIO().WantCaptureKeyboard) {
			if (get_playback_state() == graph_playback_state::running)
				pause_playback();
			else
				start_or_resume_playback();
		}

		break;
	}
}
void AnimationGraphEditor::on_wheel(const SDL_MouseWheelEvent& wheel) {
	
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
			graph_tabs->remove_nodes_tab(nodes[i]);
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

	clipboard->remove_references(node);

	for (int i = 0; i < nodes.size(); i++) {
		if (i != index) {
			nodes[i]->remove_reference(node);
		}
	}

	// node has a sublayer, remove child nodes

	auto sublayer = node->get_layer();
	if (sublayer) {
		// remove tab reference
		graph_tabs->remove_nodes_tab(node);
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

			int cur_layer = graph_tabs->get_current_layer_from_tab();

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

		tree->animator_class = anim_class_type;

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


	// initialize memory offets for runtime
	editing_tree->data_used = 0;

	// clear slot_names, direct play nodes will append this
	editing_tree->direct_slot_names.clear();

	// to get access to ptr->index hashmap
	AgSerializeContext ctx;
	ctx.set_tree(get_tree());

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

//void AnimationGraphEditor::on_change_focus(editor_focus_state newstate)
//{
//	//if (newstate == editor_focus_state::Background) {
//	//	gui->unlink_and_release_from_parent();
//	//	if (eng->get_state() != Engine_State::Game) {
//	//		stop_playback();
//	//		compile_and_run();
//	//	}
//	//	control_params->refresh_props();
//	//	out.hide();
//	//	playback = graph_playback_state::running;
//	//	//Cmd_Manager::get()->execute(Cmd_Execute_Mode::NOW, "dump_imgui_ini animdock.ini");
//	//}
//	//else if(newstate == editor_focus_state::Closed){
//	//	gui->unlink_and_release_from_parent();
//	//
//	//	close();
//	//	//Cmd_Manager::get()->execute(Cmd_Execute_Mode::NOW, "dump_imgui_ini animdock.ini");
//	//}
//	//else {
//	//	eng->get_gui()->add_gui_panel_to_root(gui.get());
//	//	eng->get_gui()->set_focus_to_this(gui.get());
//	//
//	//	// focused, stuff can start being rendered
//	//	playback = graph_playback_state::stopped;
//	//	control_params->refresh_props();
//	//	Cmd_Manager::get()->execute(Cmd_Execute_Mode::NOW, "load_imgui_ini animdock.ini");
//	//}
//}



void AnimationGraphEditor::tick(float dt)
{
	///assert(get_focus_state() != editor_focus_state::Closed);

	EditorTool3d::tick(dt);

	if(1)
	{
		//assert(eng->get_state() != Engine_State::Game);
		if (get_playback_state() == graph_playback_state::running) {
			auto animator = out.get_animator();
			if(animator)
				animator->tick_tree_new(dt * g_slomo.get_float());
		}
		
		out.show(get_playback_state() != graph_playback_state::stopped);

	}
	else {	// not focused, game running likely
		playback = graph_playback_state::running;
	}

}

#include "Framework/ArrayReflection.h"

ControlParamsWindow::ControlParamsWindow()
{
	ed.on_set_animator_instance.add(this, &ControlParamsWindow::on_set_animator_instance);
	ed.on_close.add(this, &ControlParamsWindow::on_close);

}


#include "glm/gtx/euler_angles.hpp"
void ControlParamsWindow::imgui_draw()
{
	if (!ImGui::Begin("Control parameters")) {
		ImGui::End();
		return;
	}

	uint32_t ent_list_flags = ImGuiTableFlags_PadOuterX | ImGuiTableFlags_Borders |
		ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable;


	const bool is_graph_running = ed.playback != AnimationGraphEditor::graph_playback_state::stopped;

	const int num_cols = (is_graph_running) ? 3 : 2;

	if (ImGui::BeginTable("controlproplistabc", num_cols, ent_list_flags))
	{
		ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.0f, 0);
		if (is_graph_running)
			ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
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

			if (is_graph_running) {
				ImGui::TableNextColumn();
				switch (res.type)
				{
				case anim_graph_value::float_t: {
					float f = res.nativepi->get_float(ed.out.get_animator());
					ImGui::DragFloat("##inpf", &f, 0.05);
					res.nativepi->set_float(ed.out.get_animator(), f);
				}break;
				case anim_graph_value::bool_t: {
					bool b = res.nativepi->get_int(ed.out.get_animator());
					ImGui::Checkbox("##inpf", &b);
					res.nativepi->set_int(ed.out.get_animator(), b);
				}break;
				case anim_graph_value::vec3_t: {
					glm::vec3* v = (glm::vec3*)res.nativepi->get_ptr(ed.out.get_animator());
					ImGui::DragFloat3("##inpf", &v->x,0.025);
				}break;
				case anim_graph_value::quat_t: {
					glm::quat* v = (glm::quat*)res.nativepi->get_ptr(ed.out.get_animator());
					
					glm::vec3 eul = glm::eulerAngles(*v);
					eul *= 180.f / PI;
					if (ImGui::DragFloat3("##eul", &eul.x, 1.0)) {
						eul *= PI / 180.f;
						*v = glm::quat(eul);
					}

				}break;
				case anim_graph_value::int_t:
				{
					int b = res.nativepi->get_int(ed.out.get_animator());
					ImGui::InputInt("##inpf", &b);
					res.nativepi->set_int(ed.out.get_animator(), b);
				}break;
				};
			}



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

void AnimationGraphEditor::create_new_document()
{
	printf("creating new document");
	set_empty_doc();	// when saving, user is prompted
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
	if (anim_class_type.ptr)
		ed.out.set_animator_instance((AnimatorInstance*)anim_class_type.ptr->allocate());	// fixme
	else
		ed.out.set_animator_instance(new AnimatorInstance);

	on_set_model.invoke(output_model.get());
	on_set_animator_instance.invoke(ed.out.get_animator());
}


extern ConfigVar ed_default_sky_material;


ConfigVar animed_default_model("animed_default_model", "SWAT_model.cmdl", CVAR_DEV, "sets the default model for the anim editor");

void AnimationGraphEditor::post_map_load_callback()
{
	Cmd_Manager::get()->execute(Cmd_Execute_Mode::NOW, "load_imgui_ini animdock.ini");

	output_model = GetAssets().find_sync<Model>(animed_default_model.get_string());
	ed.out.set_model(output_model.get());

	auto& name = get_doc_name();

	bool needs_new_doc = true;
	if (!name.empty()) {
		// try loading graphname, create new document on fail
		editing_tree = GetAssets().find_sync<Animation_Tree_CFG>(name).get();

		if (editing_tree) {
			DictParser parser;
			auto editorFilePath = std::string(name) + "_e";
			auto file = FileSys::open_read_game(editorFilePath);
			if (file) {
				parser.load_from_file(file.get());
				bool good = load_editor_nodes(parser);
				if (good) {

					sys_print(Debug,"graph successfully loaded\n");

					// tree was loaded and editor nodes were loaded
					needs_new_doc = false;
					is_owning_editing_tree = false;

					for (int i = 0; i < nodes.size(); i++) {
						nodes[i]->init();
					}
				}
				else
					sys_print(Error, "couldn't load editor nodes for tree %s\n", name);
			}
			else
				sys_print(Error, "couldnt open animation graph editor file %s\n", editorFilePath.c_str());
		}
		else
			sys_print(Error, "Couldn't open animation tree file %s, creating new document instead\n", name);
	}

	if (needs_new_doc) {
		if (editing_tree)
			editing_tree = nullptr;	// not memory leak since the tree gets cleaned up later with references
		set_empty_doc();
		create_new_document();
		ASSERT(!current_document_has_path());
	}

	ASSERT(editing_tree);

	anim_class_type = editing_tree->animator_class;

	// initialize other state
	*graph_tabs = TabState(this);
	// push root tab, always visible
	graph_tabs->add_tab(nullptr, nullptr, glm::vec2(0.f), true);

	self_grid.add_property_list_to_grid(get_props(), this);

	// load preview model and set and register renderable
	try_load_preview_models();

	on_open_new_doc.invoke();

	eng->get_gui()->add_gui_panel_to_root(gui.get());
	eng->get_gui()->set_focus_to_this(gui.get());

	// focused, stuff can start being rendered
	playback = graph_playback_state::stopped;
	control_params->refresh_props();
}



void GraphOutput::show(bool is_playing)
{
	if (!obj.is_valid())
		obj = idraw->get_scene()->register_obj();
	Render_Object obj_data;
	obj_data.model = model;
	if (is_playing && model->get_skel() && anim) {
		obj_data.animator = anim.get();
		obj_data.transform = model->get_root_transform();

	}
	//obj_data.mat_override = (MaterialInstance*)imaterials->find_material_instance("orborbmat"); // fixme
	idraw->get_scene()->update_obj(obj,obj_data);
}
void GraphOutput::hide()
{
	idraw->get_scene()->remove_obj(obj);
}
void GraphOutput::clear()
{
	model = nullptr;
	anim.reset();
	idraw->get_scene()->remove_obj(obj);
}
void GraphOutput::set_animator_instance(AnimatorInstance* inst)
{
	anim.reset( inst );
	idraw->get_scene()->remove_obj(obj);
}


AnimatorInstance* GraphOutput::get_animator()
{
	return anim.get();
}



AnimGraphClipboard::AnimGraphClipboard()
{
	ed.gui->key_down_delegate.add(this, &AnimGraphClipboard::on_key_down);
	ed.on_close.add(this, &AnimGraphClipboard::on_close);
}
void AnimGraphClipboard::on_close() {
	clipboard.clear();
}

void AnimGraphClipboard::on_key_down(const SDL_KeyboardEvent& key)
{
	if (key.keysym.scancode == SDL_SCANCODE_C && key.keysym.mod & KMOD_LCTRL) {
		int count = ImNodes::NumSelectedNodes();
		std::vector<int> ids(count, 0);
		ImNodes::GetSelectedNodes(ids.data());

		clipboard.clear();
		for (int i = 0; i < ids.size(); i++)
			clipboard.push_back(ed.find_node_from_id(ids[i]));
	}
	else if (key.keysym.scancode == SDL_SCANCODE_V && key.keysym.mod & KMOD_LCTRL) {
		paste_selected();
	}
}

void AnimGraphClipboard::paste_selected()
{
	// copying expects the serialize context ...
	AgSerializeContext context;
	context.set_tree(ed.get_tree());

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
			sys_print(Warning, "Can't copy+paste statemachine or state nodes (maybe a future feature)\n");
			/* would have to do recursive stuff to copy layers and states work a bit funky */
			continue;
		}

		Base_EdNode* copied_node = source->create_copy(&context)->cast_to<Base_EdNode>();

		const ClassTypeInfo* ti = &copied_node->get_type();
		for (; ti; ti = ti->super_typeinfo) {
			if (!ti->props) continue;

			for (int j = 0; j < ti->props->count; j++) {
				auto& prop = ti->props->list[j];
				if (strcmp(prop.custom_type_str, "SerializeNodeCFGRef") == 0) {
					
					// found a property that references a runtime graph node, copy its data
					BaseAGNode* runtime_node = *(BaseAGNode**)prop.get_ptr(source);
					
					BaseAGNode* copied_runtime = runtime_node->create_copy(&context)->cast_to<BaseAGNode>();

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

		copied_node->graph_layer = ed.graph_tabs->get_current_layer_from_tab();
	
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

DECLARE_ENGINE_CMD(animed_play_slot)
{
	if (args.size() != 3) {
		sys_print(Info, "usage animed_play_slot <slot> <anim>");
		return;
	}
	if (ed.playback != AnimationGraphEditor::graph_playback_state::running) {
		sys_print(Error, "can only play slots when graph is running\n");
		return;
	}
	std::string slotname = args.at(1);
	std::string anim = args.at(2);

	ed.out.get_animator()->play_animation_in_slot(anim, slotname.c_str(), 1.0, 0.0);
}
AnimationGraphEditor::AnimationGraphEditor() {
	gui = std::make_unique<AG_GuiLayout>();

	gui->wheel_delegate.add(this, &AnimationGraphEditor::on_wheel);
	gui->key_down_delegate.add(this, &AnimationGraphEditor::on_key_down);


	control_params = std::make_unique<ControlParamsWindow>();
	node_props = std::make_unique<PropertyGrid>();
	graph_tabs = std::make_unique<TabState>(this);
	clipboard = std::make_unique<AnimGraphClipboard>();

}