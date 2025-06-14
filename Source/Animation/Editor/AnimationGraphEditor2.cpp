#include "AnimationGraphEditor2.h"
#include "LevelEditor/PropertyEditors.h"
#include "imnodes.h"
#include "Assets/AssetDatabase.h"
#include "Render/Texture.h"
#include "Framework/Log.h"
#include "GraphUtil.h"
#include "AnimCommands.h"

using std::make_unique;
AnimationGraphEditorNew animgraphnew;
IEditorTool* g_anim_ed_graph = &animgraphnew;

void AnimationGraphEditorNew::add_command(Command* command)
{
	cmd_manager.add_command(command);
}

void AnimationGraphEditorNew::handle_link_changes()
{
	int start_atr = 0;
	int end_atr = 0;
	int link_id = 0;

	if (ImNodes::IsLinkCreated(&start_atr, &end_atr))
	{
		if (start_atr >= INPUT_START && start_atr < OUTPUT_START)
			std::swap(start_atr, end_atr);

		//int start_node_id = Base_EdNode::get_nodeid_from_output_id(start_atr);
		//int end_node_id = Base_EdNode::get_nodeid_from_input_id(end_atr);
		//int start_idx = Base_EdNode::get_slot_from_id(start_atr);
		//int end_idx = Base_EdNode::get_slot_from_id(end_atr);
		//
		//ASSERT(start_idx == 0);

		add_command(new AddLinkCommand(*this,GraphPortHandle(end_atr), GraphPortHandle(start_atr)));

	//	Base_EdNode* node_s = find_node_from_id(start_node_id);
	//	Base_EdNode* node_e = find_node_from_id(end_node_id);
	//
	//	if (end_idx >= node_e->inputs.size() /* because state nodes can have exposed slots that dont exist yet*/ || node_s->can_output_to_type(node_e->inputs[end_idx].type))
	//		bool destroy = node_e->add_input(this, node_s, end_idx);
	}
	if (ImNodes::IsLinkDropped(&start_atr)) {

		//bool is_input = start_atr >= INPUT_START && start_atr < OUTPUT_START;
		//
		//uint32_t id = 0;
		//if (is_input)
		//	id = Base_EdNode::get_nodeid_from_input_id(start_atr);
		//else
		//	id = Base_EdNode::get_nodeid_from_output_id(start_atr);
		//
		//auto node = find_node_from_id(id);
		//
		//drop_state.from = node;
		//drop_state.from_is_input = is_input;
		//drop_state.slot = Base_EdNode::get_slot_from_id(start_atr);
		//*open_popup_menu_from_drop = true;
	}
	if (ImNodes::IsLinkDestroyed(&link_id)) {

		uint32_t node_id = Base_EdNode::get_nodeid_from_link_id(link_id);
		uint32_t slot = Base_EdNode::get_slot_from_id(link_id);

		add_command(new RemoveLinkCommand(*this,GraphPortHandle(link_id)));

	//	Base_EdNode* node_s = find_node_from_id(node_id);
	//
	//	node_s->on_remove_pin(slot);
	//
	//	node_s->on_post_remove_pins();
	}
}

void AnimationGraphEditorNew::init()
{
	//PropertyFactoryUtil::register_anim_editor(*this, grid_factory);

	select_mgr = make_unique<NodeSelectionManager>(*this);
	graph = make_unique<EditorNodeGraph>();
	settings = make_unique<AnimNodeGraphSettings>();
	tab_manager = make_unique<GraphTabManager>(*this);
	playback = make_unique<PlaybackManager>(*this);
	property_window = make_unique<GraphPropertyWindow>(*this);
	imnodes = make_unique<ImNodesInterface>();
	concmds = ConsoleCmdGroup::create("");
	concmds->add("anim.undo", [this](const Cmd_Args&) {
		cmd_manager.undo();
		});
	concmds->add("anim.del", [this](const Cmd_Args&) {
		delete_selected();
		});

	init_node_factory();

	imnodes_context = ImNodes::CreateContext();
	//ImNodes::GetIO().LinkDetachWithModifierClick.Modifier = &is_modifier_pressed;
	ImNodes::GetStyle().Flags |= ImNodesStyleFlags_GridSnapping | ImNodesStyleFlags_GridLinesPrimary;

	if (!graph->get_root()) {
		auto root = graph->create_layer();
		graph->set_root(root);
	}

	assert(graph->get_root());

	tab_manager->open_tab(graph->get_root()->get_id(), true);
}

void GraphTabManager::draw_popup_menu()
{
	
	for (const auto&[name,create] : editor.get_prototypes().creations) {
		//const ClassBase* default_obj = infos.at(i)->default_class_object;
		//assert(default_obj && default_obj->is_a<Base_EdNode>());
		//const Base_EdNode* node = default_obj->cast_to<Base_EdNode>();
		//
		//	if (node->is_state_node() != is_state_mode)
		//		continue;

		//if (!node->allow_creation_from_menu())
		//	continue;

		//if (drop_state.from && drop_state.from_is_input && drop_state.slot < drop_state.from->inputs.size() && /* because state nodes can have num slots greater than actual slots */
		//	!node->can_output_to_type(drop_state.from->inputs[drop_state.slot].type))
		//	continue;
		//
		//if (drop_state.from && !drop_state.from_is_input && !drop_state.from->is_a<State_EdNode>())
		//	continue;

		//const std::string& name = node->get_name();

		if (ImGui::Selectable(name.c_str())) {
			opt<GraphLayerHandle> layer = get_active_tab();
			assert(layer.has_value());

			editor.add_command(new AddNodeCommand(editor,name, mouse_click_pos, layer.value()));

			//	int cur_layer = graph_tabs->get_current_layer_from_tab();

		//	Base_EdNode* a = user_create_new_graphnode(node->get_type().classname, cur_layer);

			//ImNodes::ClearNodeSelection();
			//ImNodes::SetNodeScreenSpacePos(a->id, mouse_click_pos);
			//ImNodes::SelectNode(a->id);
			//
			//if (drop_state.from) {
			//	if (drop_state.from_is_input) {
			//		drop_state.from->add_input(this, a, drop_state.slot);
			//	}
			//	else
			//		a->add_input(this, drop_state.from, 0);
			//}
		}
	}
}
#include "Basic_nodes.h"

void AnimationGraphEditorNew::init_node_factory()
{
	prototypes.add("Add", []() {
		auto n = new Math_EdNode;
		n->name = "Add";
		return n;
		});
	prototypes.add("Sub", []() {
		auto n = new Math_EdNode;
		n->name = "Sub";
		return n;
		});
}

void AnimationGraphEditorNew::delete_selected()
{
	std::vector<int> link_ids;
	link_ids.resize(ImNodes::NumSelectedLinks());
	if (link_ids.size() > 0) {
		ImNodes::GetSelectedLinks(link_ids.data());
	}
	vector<int> node_ids;
	node_ids.resize(ImNodes::NumSelectedNodes());
	if (node_ids.size() > 0) {
		ImNodes::GetSelectedNodes(node_ids.data());
	}

	add_command(new RemoveGraphObjectsCommand(*this, link_ids, node_ids));

}

void AnimationGraphEditorNew::close_internal()
{
}

void AnimationGraphEditorNew::tick(float dt)
{
	EditorTool3d::tick(dt);

}

void AnimationGraphEditorNew::hook_menu_bar()
{
}

bool AnimationGraphEditorNew::can_save_document()
{
	return true;
}

void AnimationGraphEditorNew::imgui_draw()
{
	tab_manager->draw();
	property_window->draw();
	handle_link_changes();
	cmd_manager.execute_queued_commands();
}

bool AnimationGraphEditorNew::save_document_internal()
{



	return false;
}

const ClassTypeInfo& AnimationGraphEditorNew::get_asset_type_info() const
{
	return Animation_Tree_CFG::StaticType;
}

const char* AnimationGraphEditorNew::get_save_file_extension() const
{
	return "ag";
}
void GraphTabManager::draw()
{
	int rendered = 0;

	if (tabs.empty())
		return;

	auto forward_img = g_assets.find_global_sync<Texture>("icon/forward.png");
	auto back_img = g_assets.find_global_sync<Texture>("icon/back.png");


	bool wants_back = (ImGui::IsWindowFocused() && !ImGui::GetIO().WantCaptureKeyboard && ImGui::IsKeyPressed(ImGuiKey_LeftArrow));
	bool wants_forward = (ImGui::IsWindowFocused() && !ImGui::GetIO().WantCaptureKeyboard && ImGui::IsKeyPressed(ImGuiKey_RightArrow));

	//ImGui::BeginDisabled(active_tab_hist.empty());
	//if (ImGui::ImageButton(ImTextureID(uint64_t(back_img->gl_id)), ImVec2(16, 16)) || (wants_back && !active_tab_hist.empty())) {
	//
	//	forward_tab_stack.push_back(active_tab);
	//	active_tab = active_tab_hist.back();
	//	active_tab_hist.pop_back();
	//	active_tab_dirty = true;
	//}
	//ImGui::EndDisabled();
	//ImGui::SameLine();
	//ImGui::BeginDisabled(forward_tab_stack.empty());
	//if (ImGui::ImageButton(ImTextureID(uint64_t(forward_img->gl_id)), ImVec2(16, 16)) || (wants_forward && !forward_tab_stack.empty())) {
	//	active_tab_hist.push_back(active_tab);
	//	active_tab = forward_tab_stack.back();
	//	forward_tab_stack.pop_back();
	//	active_tab_dirty = true;
	//}
	//ImGui::EndDisabled();

	// keeps a history of what that last tab actually rendered was
	static int actual_last_tab_rendered = -1;

	vector<int> delete_indicies;

	EditorNodeGraph& graph = editor.get_graph();

	if (ImGui::BeginTabBar("tabs")) {
		for (int n = 0; n < tabs.size(); n++) {
			bool needs_select = n == active_tab && active_tab_dirty;
			auto flags = (needs_select) ? ImGuiTabItemFlags_SetSelected : 0;

			NodeGraphLayer* const layer = graph.get_layer(tabs.at(n));
			if (!layer) {
				LOG_WARN("no layer");
				continue;
			}


			bool open_bool = true;
			ImGui::PushID(layer);
			const string tabname = layer->get_tab_name();
			if (ImGui::BeginTabItem(string_format("%s###", tabname.c_str()), &open_bool, flags))
			{
				bool this_is_an_old_active_tab_or_just_skip = n != active_tab && active_tab_dirty;

				if (this_is_an_old_active_tab_or_just_skip) {
					ImGui::EndTabItem();
					ImGui::PopID();
					continue;
				}

				bool this_tab_needs_a_reset = n != active_tab;

				//uint32_t layer = (tabs[n].layer) ? tabs[n].layer->id : 0;
				ImNodesEditorContext* const context = layer->get_context();
				if (this_tab_needs_a_reset) {
					ImNodes::ClearNodeSelection();
				}
				ImNodes::EditorContextSet(context);
				if (this_tab_needs_a_reset) {
					ImNodes::ClearNodeSelection();
				}


				layer->draw(graph);

				//parent->draw_graph_layer(layer);

				rendered++;
				ImGui::EndTabItem();

				if (active_tab != n) {
					ASSERT(!active_tab_dirty);
					active_tab = n;
				}
			}
			ImGui::PopID();
			if (!open_bool) {
				delete_indicies.push_back(n);
			}
		}
		ImGui::EndTabBar();
	}

	active_tab_dirty = false;

	if (delete_indicies.size() > 0) {
		// fixme
	}

	const bool is_focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows | ImGuiFocusedFlags_RootWindow);
	if (is_focused && ImGui::GetIO().MouseClicked[1]) {
		ImGui::OpenPopup("my_select_popup");
		mouse_click_pos = GraphUtil::to_glm(ImGui::GetMousePos());
	}
	if (ImGui::BeginPopup("my_select_popup"))
	{
		draw_popup_menu();
		ImGui::EndPopup();
	}

}

void GraphTabManager::open_tab(GraphLayerHandle handle, bool set_active)
{
	tabs.push_back(handle);
	active_tab = tabs.size() - 1;
	active_tab_dirty = true;
}

void GraphTabManager::close_tab(GraphLayerHandle handle)
{
}
/*

	// Handle Hovered links
	int link = 0;
	if (ImNodes::IsLinkHovered(&link)) {


		int node_id = Base_EdNode::get_nodeid_from_link_id(link);
		int slot = Base_EdNode::get_slot_from_id(link);
		Base_EdNode* node_s = anim_graph_ed.find_node_from_id(node_id);
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

*/
#include "GraphUtil.h"
void Base_EdNode::draw_imnode()
{
	const Color32 nodecolor = get_node_color();
	const string title = get_title();

	ImNodes::PushColorStyle(ImNodesCol_TitleBar, nodecolor.to_uint());
	Color32 select_color = GraphUtil::add_brightness(nodecolor, 30);
	Color32 hover_color = GraphUtil::add_brightness(GraphUtil::mix_with(nodecolor, { 5, 225, 250 }, 0.6), 5);
	ImNodes::PushColorStyle(ImNodesCol_TitleBarHovered, hover_color.to_uint());
	ImNodes::PushColorStyle(ImNodesCol_TitleBarSelected, select_color.to_uint());

	// node is selected
	const bool is_selected = editor->is_node_selected(*this);

	if (is_selected) {
		ImNodes::PushStyleVar(ImNodesStyleVar_NodeBorderThickness, 2.0);
		ImNodes::PushColorStyle(ImNodesCol_NodeOutline, GraphUtil::color32_to_int({ 255, 174, 0 }));
	}

	ImNodes::BeginNode(self.id);

	ImNodes::BeginNodeTitleBar();
	{
		auto cursorpos = ImGui::GetCursorPos();
		ImGui::Dummy(ImVec2(100, 0.0));
		ImGui::SameLine(0, 0);
		ImGui::SetCursorPosX(cursorpos.x);
		ImGui::Text("%s\n", title.c_str());
	}

	if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal) && ImGui::BeginTooltip()) {
		auto drawlist = ImGui::GetForegroundDrawList();
		auto& style = ImGui::GetStyle();
		auto min = ImGui::GetCursorScreenPos();
		const string& tooltip = get_tooltip();
		auto sz = ImGui::CalcTextSize(tooltip.c_str());
		float width = ImGui::CalcItemWidth();

		auto minrect = ImVec2(min.x - style.FramePadding.x * 5.0, min.y);
		auto maxrect = ImVec2(min.x + sz.x + style.FramePadding.x * 5.0, min.y + sz.y + style.FramePadding.y * 2.0);
		float border = 2.0;
		drawlist->AddRectFilled(ImVec2(minrect.x - border, minrect.y - border), ImVec2(maxrect.x + border, maxrect.y + border), Color32{ 0,0,0, 255 }.to_uint());


		drawlist->AddRectFilled(minrect, maxrect, Color32{ 255,255,255, 255 }.to_uint());
		auto cursor = ImGui::GetCursorScreenPos();
		drawlist->AddText(cursor, Color32{ 0,0,0 }.to_uint(), tooltip.c_str());
		//ImGui::Text(str.c_str());
		//ImGui::Text(node->get_tooltip().c_str());
		ImGui::EndTooltip();
	}

#pragma warning(disable: 4312)
	//if (!node->compile_error_string.empty()) {
	//	ImGui::SameLine();
	//	ImGui::Image((ImTextureID)strong_error->gl_id, ImVec2(16, 16));
	//
	//	if (ImGui::IsItemHovered() && ImGui::BeginTooltip()) {
	//		ImGui::Text(node->compile_error_string.c_str());
	//		ImGui::EndTooltip();
	//	}
	//
	//}
	//
	//if (node->children_have_errors) {
	//	ImGui::SameLine();
	//	ImGui::Image((ImTextureID)weak_error->gl_id, ImVec2(16, 16));
	//
	//	if (ImGui::IsItemHovered() && ImGui::BeginTooltip()) {
	//		ImGui::Text("children have errors");
	//		ImGui::EndTooltip();
	//	}
	//}
	//
	//if (!node->compile_info_string.empty()) {
	//	ImGui::SameLine();
	//	ImGui::Image((ImTextureID)info_img->gl_id, ImVec2(16, 16));
	//
	//	if (ImGui::IsItemHovered() && ImGui::BeginTooltip()) {
	//		ImGui::Text(node->compile_info_string.c_str());
	//		ImGui::EndTooltip();
	//	}
	//}
#pragma warning(default: 4312)



	ImNodes::EndNodeTitleBar();

	float x1 = ImGui::GetItemRectMin().x;
	float x2 = ImGui::GetItemRectMax().x;


	//ImGui::BeginDisabled(graph_is_read_only());
	draw_node_top_bar();
	//ImGui::EndDisabled();

	//MyImSeperator(x1, x2, 1.0);

	ImVec4 pin_color = ImGui::GetStyle().Colors[ImGuiCol_Text];
	if (has_pin_colors())
		pin_color = get_pin_colors();
	
	vector<int> input_ports;
	vector<int> output_ports;
	get_ports(input_ports, output_ports);

	for (int input_idx : input_ports) {
		const GraphPort& port = ports.at(input_idx);
		GraphPortHandle phandle = port.get_handle(self);
		opt<GraphLink> link = find_link(phandle);
		ImNodesPinShape pin = ImNodesPinShape_Quad;

		if (link.has_value())
			pin = ImNodesPinShape_TriangleFilled;
		ImNodes::BeginInputAttribute(phandle.id, pin);
		const string& str = port.name;
		ImGui::Text("%s",str.c_str());

		//auto input_type = node->inputs[j].type;

		//ImGui::TextColored(graph_pin_type_to_color(input_type), str.c_str());
		ImNodes::EndInputAttribute();
	}
	for (int output_idx : output_ports) {
		const GraphPort& port = ports.at(output_idx);
		GraphPortHandle phandle = port.get_handle(self);
		ImNodes::BeginOutputAttribute(phandle.id);
		const string& output_str = port.name;

		auto posX = (ImGui::GetCursorPosX() + (x2 - x1) - ImGui::CalcTextSize(output_str.c_str()).x - 2 * ImGui::GetStyle().ItemSpacing.x) - 5.0;
		if (posX > ImGui::GetCursorPosX())
			ImGui::SetCursorPosX(posX);

		//auto output_type = node->get_output_type_general();

		//ImGui::TextColored(graph_pin_type_to_color(output_type), output_str.c_str());
		ImGui::Text("%s", output_str.c_str());
		ImNodes::EndOutputAttribute();
	}
	ImNodes::EndNode();

	ImNodes::PopColorStyle();
	ImNodes::PopColorStyle();
	ImNodes::PopColorStyle();

	if (is_selected) {
		ImNodes::PopStyleVar();
		ImNodes::PopColorStyle();
	}

	//bool draw_flat_links = node->draw_flat_links();
	for (int input_idx : input_ports) {
		const GraphPort& port = ports.at(input_idx);
		GraphPortHandle phandle = port.get_handle(self);
		opt<GraphLink> link = find_link(phandle);
		
		if (link.has_value()) {
			const GraphLink& lv = link.value();

			int offset = 0;
			//if (draw_flat_links) {
			//	for (int k = 0; k < j; k++) {
			//		if (node->inputs[k].node == node->inputs[j].node)
			//			offset++;
			//	}
			//}


			//bool pushed_colors = node->push_imnode_link_colors(j);

			//ImNodes::Link(node->getlink_id(j), node->inputs[j].node->getoutput_id(0), node->getinput_id(j), draw_flat_links, offset);
			ImNodes::Link(lv.get_link_id(), lv.input.id, lv.output.id);

			//if (pushed_colors) {
			//	ImNodes::PopColorStyle();
			//	ImNodes::PopColorStyle();
			//	ImNodes::PopColorStyle();
			//
			//}
		}
	}
}
void NodeGraphLayer::draw(EditorNodeGraph& graph)
{

	auto winsize = ImGui::GetWindowSize();
	if (wants_reset_view) {
		ImNodes::EditorContextResetPanning(ImVec2(0, 0));
		ImNodes::EditorContextResetPanning(ImVec2(winsize.x / 4, winsize.y / 2.4));
		wants_reset_view = false;
	}

	auto strong_error = g_assets.find_global_sync<Texture>("icon/fatalerr.png");
	auto weak_error = g_assets.find_global_sync<Texture>("icon/error.png");
	auto info_img = g_assets.find_global_sync<Texture>("icon/question.png");

	ImNodes::BeginNodeEditor();
	for (int _node_id : layer_nodes) {
		GraphNodeHandle node_handle(_node_id);
		Base_EdNode* node = graph.get_node(GraphNodeHandle(_node_id));
		if (!node) {
			LOG_WARN("no node in layer");
			continue;
		}
		node->draw_imnode();
	}

	ImNodes::MiniMap();
	ImNodes::EndNodeEditor();


	handle_drag_drop();

}
void NodeGraphLayer::handle_drag_drop()
{
	if (ImGui::BeginDragDropTarget())
	{
		//bool is_sm = graph_tabs->get_active_tab()->is_statemachine_tab();
		//uint32_t layer = graph_tabs->get_current_layer_from_tab();
		//if (!is_sm) {
		//	if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("AssetBrowserDragDrop"))
		//	{
		//		AssetOnDisk* resource = *(AssetOnDisk**)payload->Data;
		//		if (resource->type->get_asset_class_type() == &AnimationSeqAsset::StaticType) {
		//			auto node = user_create_new_graphnode(Clip_EdNode::StaticType.classname, layer);
		//			Clip_EdNode* cl = node->cast_to<Clip_EdNode>();
		//			ASSERT(cl);
		//			cl->node->Clip = g_assets.find_sync<AnimationSeqAsset>(resource->filename);
		//			ImNodes::ClearNodeSelection();
		//			ImNodes::SetNodeScreenSpacePos(cl->id, ImGui::GetMousePos());
		//			ImNodes::SelectNode(cl->id);
		//		}
		//	}
		//	else if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("AnimGraphVariableDrag")) {
		//		VariableNameAndType* var = *(VariableNameAndType**)payload->Data;
		//		auto node = user_create_new_graphnode(Variable_EdNode::StaticType.classname, layer);
		//		Variable_EdNode* v = node->cast_to<Variable_EdNode>();
		//		ASSERT(v);
		//
		//		v->variable = *var;
		//
		//		ImNodes::ClearNodeSelection();
		//		ImNodes::SetNodeScreenSpacePos(v->id, ImGui::GetMousePos());
		//		ImNodes::SelectNode(v->id);
		//	}
		//}
		ImGui::EndDragDropTarget();
	}
}

GraphPropertyWindow::GraphPropertyWindow(AnimationGraphEditorNew& editor)
	: graph_grid(editor.get_factory()), node_grid(editor.get_factory())
{
}

void ImNodesInterface::set_node_position(GraphNodeHandle self, glm::vec2 pos)
{
	ImNodes::SetNodeScreenSpacePos(self.id, GraphUtil::to_imgui(pos));
}
static GraphPortHandle getinput_id(GraphNodeHandle self, int inputslot) {
	return inputslot + self.id * MAX_INPUTS + INPUT_START;
}
static GraphPortHandle getoutput_id(GraphNodeHandle self, int outputslot) {
	return outputslot + self.id * MAX_INPUTS + OUTPUT_START;
}
GraphPortHandle GraphPortHandle::make(GraphNodeHandle node, int index, bool is_output)
{
	return is_output ? getoutput_id(node, index) : getinput_id(node, index);
}
GraphPortHandle GraphPort::get_handle(GraphNodeHandle node) const
{
	return GraphPortHandle::make(node, index, is_output());
}